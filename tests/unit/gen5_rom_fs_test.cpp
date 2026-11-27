// Unit tests for the imported ROM's file-system reader (src/port/gen5_rom_fs.h): hostile tables must fail closed.
// Build and run: tests/unit/run.sh
#include <cstdio>
#include <cstring>
#include <sstream>

#include "gen5_rom_fs.h"

using namespace gen5rom;

static int sFailures = 0;
#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);          \
            sFailures++;                                                         \
        }                                                                        \
    } while (0)

static void Put32(std::string &s, size_t at, uint32_t v)
{
    for (int i = 0; i < 4; i++) {
        s[at + i] = (char)(v >> (8 * i));
    }
}

// A ROM image: header, then FNT at 0x100, FAT at 0x400, padded to `size`
struct Rom {
    std::string bytes;
    Rom(const std::string &fnt, const std::string &fat, size_t size = 0x1000) : bytes(size, '\0')
    {
        Put32(bytes, 0x40, 0x100);
        Put32(bytes, 0x44, (uint32_t)fnt.size());
        Put32(bytes, 0x48, 0x400);
        Put32(bytes, 0x4C, (uint32_t)fat.size());
        bytes.replace(0x100, fnt.size(), fnt);
        bytes.replace(0x400, fat.size(), fat);
    }
};

static std::string FatEntry(uint32_t start, uint32_t end)
{
    std::string e(8, '\0');
    Put32(e, 0, start);
    Put32(e, 4, end);
    return e;
}

// Root folder entry (names at 0x10, first file 0, folder count 1), then the name list; padded to 0x10
static std::string Fnt(const std::string &names)
{
    std::string f(0x10, '\0');
    Put32(f, 0, 0x10);
    f[5] = 0; // first file ID 0
    f[6] = 1; // parent/count field
    f += names;
    f.push_back('\0');
    return f;
}

static std::string File(const std::string &name)
{
    return std::string(1, (char)name.size()) + name;
}

static bool Run(const Rom &rom, std::vector<RomFile> &files, std::string &error)
{
    std::istringstream in(rom.bytes);
    return ListFiles(in, rom.bytes.size(), files, error);
}

int main()
{
    // Valid: two files in the root
    {
        std::vector<RomFile> files;
        std::string error;
        Rom rom(Fnt(File("a") + File("swan_sound_data.sdat")), FatEntry(0x800, 0x900) + FatEntry(0x900, 0x910));
        CHECK(Run(rom, files, error));
        CHECK(files.size() == 2);
        CHECK(files.size() == 2 && files[1].path == "swan_sound_data.sdat" && files[1].end == 0x910);
    }
    // Range ends before it starts
    {
        std::vector<RomFile> files;
        std::string error;
        Rom rom(Fnt(File("x")), FatEntry(0x900, 0x800));
        CHECK(!Run(rom, files, error));
        CHECK(!error.empty());
    }
    // Range past the end of the ROM
    {
        std::vector<RomFile> files;
        std::string error;
        Rom rom(Fnt(File("x")), FatEntry(0x800, 0x2000));
        CHECK(!Run(rom, files, error));
    }
    // A folder that lists itself: must terminate with an error (was an endless loop)
    {
        std::vector<RomFile> files;
        std::string error;
        std::string folderSelf = std::string(1, (char)(0x80 | 1)) + "d" + std::string("\x00\xF0", 2);
        Rom rom(Fnt(folderSelf), "");
        CHECK(!Run(rom, files, error));
        CHECK(error.find("loops") != std::string::npos);
    }
    // File ID beyond the allocation table
    {
        std::vector<RomFile> files;
        std::string error;
        Rom rom(Fnt(File("x") + File("y")), FatEntry(0x800, 0x810));
        CHECK(!Run(rom, files, error));
    }
    // Tables outside the ROM
    {
        std::vector<RomFile> files;
        std::string error;
        Rom rom(Fnt(File("x")), FatEntry(0x800, 0x810));
        rom.bytes.resize(0x300); // the ROM ends before its allocation table
        CHECK(!Run(rom, files, error));
    }

    // Path safety
    CHECK(IsSafePath("a/0/1"));
    CHECK(IsSafePath("swan_sound_data.sdat"));
    CHECK(IsSafePath("a/0/2/9"));
    CHECK(!IsSafePath(""));
    CHECK(!IsSafePath("a/../../x"));
    CHECK(!IsSafePath("a/./x"));
    CHECK(!IsSafePath("a//x"));
    CHECK(!IsSafePath("/etc/passwd"));
    CHECK(!IsSafePath("a/x/"));
    CHECK(!IsSafePath("a\\..\\y"));
    CHECK(!IsSafePath("a/C:/x"));
    CHECK(!IsSafePath("a/x:stream"));
    CHECK(!IsSafePath("a/.../x"));
    CHECK(!IsSafePath("a/x. "));
    CHECK(!IsSafePath("a/x."));
    CHECK(!IsSafePath("a/NUL"));
    CHECK(!IsSafePath("a/nul.txt"));
    CHECK(!IsSafePath("a/COM1"));
    CHECK(IsSafePath("a/COMX"));
    CHECK(!IsSafePath(std::string("a/x\0y", 5)));
    CHECK(!IsSafePath("a/\x01"));

    if (sFailures == 0) {
        std::printf("gen5_rom_fs: all tests passed\n");
    }
    return sFailures == 0 ? 0 : 1;
}
