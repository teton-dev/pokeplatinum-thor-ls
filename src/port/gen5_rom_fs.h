// The file system of an imported Black 2 / White 2 ROM (NitroFS), read from a ROM the player supplied: untrusted
// data. Header-only and free of SDK dependencies so tests/unit/gen5_rom_fs_test.cpp can build it alone.
// Used by gen5_rom.cpp. The Goldenrod fork keeps an identical copy (lib/port/gen5_rom_fs.h); change both.
#ifndef POKEPLATINUM_PORT_GEN5_ROM_FS_H
#define POKEPLATINUM_PORT_GEN5_ROM_FS_H

#include <cstddef>
#include <cstdint>
#include <istream>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace gen5rom {

struct RomFile {
    std::string path; // folders joined with '/', as the ROM names them (not yet checked: see IsSafePath)
    uint32_t start, end; // byte range in the ROM
};

inline uint32_t Read32(const std::vector<uint8_t> &data, size_t offset)
{
    return data[offset] | data[offset + 1] << 8 | data[offset + 2] << 16 | (uint32_t)data[offset + 3] << 24;
}

inline uint16_t Read16(const std::vector<uint8_t> &data, size_t offset)
{
    return data[offset] | data[offset + 1] << 8;
}

namespace detail {

inline bool IsReservedDeviceName(const std::string &component)
{
    // Windows opens these as devices whatever the extension ("NUL.txt")
    std::string stem = component.substr(0, component.find('.'));
    for (char &c : stem) {
        c = (char)((c >= 'a' && c <= 'z') ? c - 'a' + 'A' : c);
    }
    if (stem == "CON" || stem == "PRN" || stem == "AUX" || stem == "NUL") {
        return true;
    }
    return stem.size() == 4 && (stem.compare(0, 3, "COM") == 0 || stem.compare(0, 3, "LPT") == 0) && stem[3] >= '0' &&
        stem[3] <= '9';
}

inline bool IsSafeComponent(const std::string &component)
{
    if (component.empty() || component[0] == '.' || component.back() == '.') {
        return false; // "", ".", ".." (also "..." and "x." which Windows trims to ".." or "x")
    }
    for (char c : component) {
        const bool allowed = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' ||
            c == '-' || c == '.';
        if (!allowed) {
            return false; // separators, drive colons (and NTFS streams), spaces, control bytes, non-ASCII
        }
    }
    return !IsReservedDeviceName(component);
}

} // namespace detail

// Whether path ("a/0/1", "swan_sound_data.sdat") can be joined under an extraction folder without leaving it: a
// relative path of plain components (BW2's names are digits, letters, '_', '-' and '.')
inline bool IsSafePath(const std::string &path)
{
    size_t from = 0;
    while (true) {
        const size_t slash = path.find('/', from);
        const std::string component = path.substr(from, slash == std::string::npos ? std::string::npos : slash - from);
        if (!detail::IsSafeComponent(component)) {
            return false;
        }
        if (slash == std::string::npos) {
            return true;
        }
        from = slash + 1;
    }
}

// Reads the ROM's file name table and allocation table. romSize is the ROM's length in bytes: every file range must
// lie inside it. Fails closed (returns false with error set) on a table that is out of bounds, loops, or lists a
// range that ends before it starts.
inline bool ListFiles(std::istream &rom, uint64_t romSize, std::vector<RomFile> &files, std::string &error)
{
    constexpr uint32_t kMaxTableBytes = 1 << 20;
    std::vector<uint8_t> header(0x50);
    rom.seekg(0);
    rom.read((char *)header.data(), header.size());
    const uint32_t fntOffset = Read32(header, 0x40), fntSize = Read32(header, 0x44);
    const uint32_t fatOffset = Read32(header, 0x48), fatSize = Read32(header, 0x4C);
    if (!rom || fntSize < 8 || fntSize > kMaxTableBytes || fatSize > kMaxTableBytes ||
        (uint64_t)fntOffset + fntSize > romSize || (uint64_t)fatOffset + fatSize > romSize) {
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
    // folder, followed by its ID), 0 at the end. A folder may be reached once: a second visit is a loop or a
    // shared folder, neither of which a real ROM has.
    std::set<uint16_t> visited;
    std::vector<std::pair<uint16_t, std::string>> folders = { { 0xF000, "" } };
    while (!folders.empty()) {
        auto [id, prefix] = folders.back();
        folders.pop_back();
        if (!visited.insert(id).second) {
            error = "Its file system is damaged (a folder loops)";
            return false;
        }
        const size_t entry = (size_t)(id & 0xFFF) * 8;
        if (entry + 8 > fnt.size()) {
            error = "Its file system is damaged";
            return false;
        }
        size_t at = Read32(fnt, entry);
        uint32_t fileId = Read16(fnt, entry + 4);
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
                const uint32_t start = Read32(fat, fileId * 8), end = Read32(fat, fileId * 8 + 4);
                if (end < start || end > romSize) {
                    error = "Its file system is damaged (a file lies outside the ROM)";
                    return false;
                }
                files.push_back({ prefix + name, start, end });
                fileId++;
            }
        }
    }
    return true;
}

} // namespace gen5rom

#endif // POKEPLATINUM_PORT_GEN5_ROM_FS_H
