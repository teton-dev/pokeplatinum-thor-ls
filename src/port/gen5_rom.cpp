// The Black 2 / White 2 ROM for the Gen 5 battles; see include/port/gen5_rom.h
#include "port/gen5_rom.h"

#include <nitro.h>
#include <simulator/sim_carts.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "gen5.h"

namespace fs = std::filesystem;

namespace {

// The dumps pokebw2 decompiles and rebuilds byte for byte (its README): only their data fits its code
struct KnownRom {
    const char *gameCode;
    const char *name;
    const char *sha1;
};

const KnownRom sKnownRoms[] = {
    { "IREO", "Black 2", "e51e6dfb8678a3d19dcd2a10691b96a569ca0abb" },
    { "IRDO", "White 2", "b5d7490be7b415b8f1e672a53e978a9cc667e56a" },
};

// What the battles read: the archives (a/...) and the sound data. Download Play's and Wi-Fi's files aren't
// extracted
bool IsExtracted(const std::string &path)
{
    return path.rfind("a/", 0) == 0 || path == "swan_sound_data.sdat";
}

// The import's record in the extracted folder: the game, and the self-check's result
const char *const kInfoFile = "rom.txt";

std::mutex sMutex;
std::string sStatus;
std::string sGameName;
std::atomic<int> sState { GEN5_ROM_NONE };
std::atomic<bool> sThreadDone { false };
bool sLoaded = false;

void SetStatus(const std::string &text)
{
    std::lock_guard<std::mutex> lock(sMutex);
    sStatus = text;
}

std::string Dir()
{
    return SIM_SettingsPath("gen5");
}

std::string TempDir()
{
    return Dir() + ".tmp";
}

// The record of an earlier import, if there is one
void Load()
{
    if (sLoaded) {
        return;
    }
    sLoaded = true;
    std::ifstream info(Dir() + "/" + kInfoFile);
    std::string line;
    while (std::getline(info, line)) {
        if (line.rfind("game=", 0) == 0) {
            sGameName = line.substr(5);
        } else if (line.rfind("check=", 0) == 0) {
            sStatus = line.substr(6);
        }
    }
    if (!sGameName.empty()) {
        sState = GEN5_ROM_READY;
    }
}

struct RomFile {
    std::string path;
    uint32_t start, end;
};

uint32_t Read32(const std::vector<uint8_t> &data, size_t offset)
{
    return data[offset] | data[offset + 1] << 8 | data[offset + 2] << 16 | (uint32_t)data[offset + 3] << 24;
}

uint16_t Read16(const std::vector<uint8_t> &data, size_t offset)
{
    return data[offset] | data[offset + 1] << 8;
}

// The ROM's file system: the file name table's folders and the allocation table's ranges
bool ListFiles(std::ifstream &rom, std::vector<RomFile> &files, std::string &error)
{
    std::vector<uint8_t> header(0x50);
    rom.seekg(0);
    rom.read((char *)header.data(), header.size());
    const uint32_t fntOffset = Read32(header, 0x40), fntSize = Read32(header, 0x44);
    const uint32_t fatOffset = Read32(header, 0x48), fatSize = Read32(header, 0x4C);
    if (!rom || fntSize < 8 || fntSize > (1 << 20) || fatSize > (1 << 20)) {
        error = "Its file system can't be read";
        return false;
    }
    std::vector<uint8_t> fnt(fntSize), fat(fatSize);
    rom.seekg(fntOffset);
    rom.read((char *)fnt.data(), fnt.size());
    rom.seekg(fatOffset);
    rom.read((char *)fat.data(), fat.size());
    if (!rom) {
        error = "Its file system can't be read";
        return false;
    }

    // Each folder's entry: where its names start and its first file's ID. Names: a length byte (with 0x80 for a
    // folder, followed by its ID), 0 at the end
    std::vector<std::pair<uint16_t, std::string>> folders = { { 0xF000, "" } };
    while (!folders.empty()) {
        auto [id, prefix] = folders.back();
        folders.pop_back();
        const size_t entry = (id & 0xFFF) * 8;
        if (entry + 8 > fnt.size()) {
            error = "Its file system is damaged";
            return false;
        }
        size_t at = Read32(fnt, entry);
        uint16_t fileId = Read16(fnt, entry + 4);
        while (at < fnt.size() && fnt[at] != 0) {
            const uint8_t length = fnt[at] & 0x7F;
            const bool folder = (fnt[at] & 0x80) != 0;
            if (at + 1 + length + (folder ? 2 : 0) > fnt.size()) {
                error = "Its file system is damaged";
                return false;
            }
            const std::string name((const char *)&fnt[at + 1], length);
            at += 1 + length;
            if (folder) {
                folders.push_back({ Read16(fnt, at), prefix + name + "/" });
                at += 2;
            } else {
                if ((size_t)fileId * 8 + 8 > fat.size()) {
                    error = "Its file system is damaged";
                    return false;
                }
                files.push_back({ prefix + name, Read32(fat, fileId * 8), Read32(fat, fileId * 8 + 4) });
                fileId++;
            }
        }
    }
    return true;
}

void ImportThread(std::string path, bool deleteAfter)
{
    std::string error;
    std::string game;
    std::error_code ec;

    std::ifstream rom(path, std::ios::binary);
    char gameCode[5] = {};
    rom.seekg(0xC);
    rom.read(gameCode, 4);
    const KnownRom *known = nullptr;
    for (const KnownRom &candidate : sKnownRoms) {
        if (rom && memcmp(gameCode, candidate.gameCode, 4) == 0) {
            known = &candidate;
        }
    }
    if (!rom) {
        error = "That file can't be read.";
    } else if (known == nullptr) {
        error = "That isn't Pokemon Black 2 or White 2 (USA) (game code " + std::string(gameCode) + ").";
    }

    // The whole ROM's SHA1
    if (error.empty()) {
        rom.seekg(0, std::ios::end);
        const uint64_t size = rom.tellg();
        rom.seekg(0);
        MATHSHA1Context sha1;
        MATH_SHA1Init(&sha1);
        std::vector<char> buffer(1 << 20);
        uint64_t done = 0;
        int lastPercent = -1;
        while (rom && done < size) {
            rom.read(buffer.data(), buffer.size());
            const std::streamsize got = rom.gcount();
            MATH_SHA1Update(&sha1, buffer.data(), (u32)got);
            done += got;
            const int percent = (int)(done * 100 / size);
            if (percent != lastPercent) {
                lastPercent = percent;
                SetStatus("Checking your " + std::string(known->name) + " ROM: " + std::to_string(percent) + "%");
            }
            if (got == 0) {
                break;
            }
        }
        rom.clear();
        u8 digest[MATH_SHA1_DIGEST_SIZE];
        MATH_SHA1GetHash(&sha1, digest);
        char hex[MATH_SHA1_DIGEST_SIZE * 2 + 1];
        for (int i = 0; i < MATH_SHA1_DIGEST_SIZE; i++) {
            snprintf(hex + i * 2, 3, "%02x", digest[i]);
        }
        if (done != size || strcmp(hex, known->sha1) != 0) {
            error = "That " + std::string(known->name) + " ROM isn't a clean dump of the USA game (its SHA1 is " +
                hex + "). The Gen 5 battles need the original one, unpatched.";
        }
        game = known->name;
    }

    // The files, into gen5.tmp/, which becomes gen5/ once complete
    std::vector<RomFile> files;
    if (error.empty() && ListFiles(rom, files, error)) {
        fs::remove_all(TempDir(), ec);
        std::vector<char> buffer(1 << 20);
        int count = 0, total = 0;
        for (const RomFile &file : files) {
            total += IsExtracted(file.path) ? 1 : 0;
        }
        for (const RomFile &file : files) {
            if (!IsExtracted(file.path)) {
                continue;
            }
            const fs::path target = fs::path(TempDir()) / file.path;
            fs::create_directories(target.parent_path(), ec);
            std::ofstream out(target, std::ios::binary);
            rom.seekg(file.start);
            uint32_t left = file.end - file.start;
            while (out && rom && left > 0) {
                const uint32_t chunk = std::min<uint32_t>(left, buffer.size());
                rom.read(buffer.data(), chunk);
                out.write(buffer.data(), chunk);
                left -= chunk;
            }
            if (!out || !rom) {
                error = "Couldn't extract " + file.path + " (is the device full? It takes about 270 MB).";
                break;
            }
            count++;
            if (count % 8 == 0 || count == total) {
                SetStatus("Extracting the battles' files: " + std::to_string(count) + " of " + std::to_string(total));
            }
        }
    }
    rom.close();

    if (error.empty()) {
        std::ofstream info(fs::path(TempDir()) / kInfoFile);
        info << "game=" << game << "\n";
        info.close();
        fs::remove_all(Dir(), ec);
        fs::rename(TempDir(), Dir(), ec);
        if (ec) {
            error = "Couldn't move the files into place: " + ec.message();
        }
    } else {
        fs::remove_all(TempDir(), ec);
    }
    if (deleteAfter) {
        fs::remove(path, ec);
    }

    {
        std::lock_guard<std::mutex> lock(sMutex);
        sStatus = error.empty() ? "Extracted; checking that the Gen 5 code reads it..." : error;
        if (error.empty()) {
            sGameName = game;
        }
    }
    sThreadDone = true;
}

} // namespace

extern "C" void Gen5Rom_Import(const char *path, BOOL deleteAfter)
{
    Load();
    if (sState == GEN5_ROM_IMPORTING) {
        return;
    }
    sState = GEN5_ROM_IMPORTING;
    sThreadDone = false;
    SetStatus("Opening the ROM...");
    std::thread(ImportThread, std::string(path), deleteAfter != FALSE).detach();
}

extern "C" void Gen5Rom_Update(void)
{
    Load();
    if (sState != GEN5_ROM_IMPORTING || !sThreadDone) {
        return;
    }
    sThreadDone = false;

    std::string game;
    {
        std::lock_guard<std::mutex> lock(sMutex);
        game = sGameName;
    }
    std::error_code ec;
    if (game.empty() || !fs::exists(fs::path(Dir()) / kInfoFile, ec)) {
        // The import failed; an earlier one may still be there
        sGameName.clear();
        sLoaded = false;
        std::string failure;
        {
            std::lock_guard<std::mutex> lock(sMutex);
            failure = sStatus;
        }
        Load();
        SetStatus(failure);
        sState = GEN5_ROM_FAILED;
        return;
    }

    // The self-check, on the game's thread: BW2's own archive code reads a file from what was extracted
    char message[160] = "";
    bool ok = false;
    if (Gen5_FilesInit(Dir().c_str())) {
        ok = Gen5_SelfCheck(message, sizeof(message)) != 0;
    } else {
        snprintf(message, sizeof(message), "The game's folder can't reach %s", Dir().c_str());
    }
    printf("Gen 5: %s ROM %s: %s\n", game.c_str(), ok ? "ready" : "failed its check", message);
    if (ok) {
        std::ofstream info(fs::path(Dir()) / kInfoFile, std::ios::app);
        info << "check=" << message << "\n";
        SetStatus(message);
        sState = GEN5_ROM_READY;
    } else {
        Gen5Rom_Remove();
        SetStatus(std::string("The files were extracted, but the Gen 5 code couldn't read them: ") + message);
        sState = GEN5_ROM_FAILED;
    }
}

extern "C" enum Gen5RomState Gen5Rom_State(void)
{
    Load();
    return (enum Gen5RomState)sState.load();
}

extern "C" const char *Gen5Rom_Status(void)
{
    // A copy that stays valid until the next call, as the import thread may change sStatus
    static std::string copy;
    std::lock_guard<std::mutex> lock(sMutex);
    copy = sStatus;
    return copy.c_str();
}

extern "C" const char *Gen5Rom_GameName(void)
{
    Load();
    return sState == GEN5_ROM_IMPORTING || sGameName.empty() ? nullptr : sGameName.c_str();
}

extern "C" const char *Gen5Rom_Dir(void)
{
    static std::string dir;
    dir = Dir();
    return dir.c_str();
}

extern "C" BOOL Gen5Rom_Remove(void)
{
    if (sState == GEN5_ROM_IMPORTING) {
        return FALSE;
    }
    std::error_code ec;
    fs::remove_all(Dir(), ec);
    sGameName.clear();
    sState = GEN5_ROM_NONE;
    SetStatus("");
    return !ec;
}
