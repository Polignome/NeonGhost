#include "Wad3File.h"
#include <fstream>
#include <cstring>
#include <algorithm>
#include <cctype>


using namespace ng::map::texture;

namespace
{
    std::string ToLower(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s;
    }
}

// ------------------------------------------------------------------
// Konstruktoren
// ------------------------------------------------------------------
Wad3File::Wad3File() {

}

Wad3File::Wad3File(const std::vector<uint8_t>& fileData)
    : m_buffer(fileData)
{
    ReadHeader();
    ReadDirectory();
}

Wad3File::Wad3File(const std::string& path)
{
    
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
        throw std::runtime_error("WAD3-Datei konnte nicht geoeffnet werden: " + path);

    const std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    m_buffer.resize(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(m_buffer.data()), size))
        throw std::runtime_error("WAD3-Datei konnte nicht gelesen werden: " + path);

    ReadHeader();
    ReadDirectory();
}

// ------------------------------------------------------------------
// Low-Level-Reader
// ------------------------------------------------------------------
std::string Wad3File::ReadString(size_t offset, size_t length) const
{
    std::string out;
    out.reserve(length);

    for (size_t i = 0; i < length; i++)
    {
        const uint8_t c = ReadUint8(offset + i);
        if (c == 0)
            break;
        out += static_cast<char>(c);
    }

    return out;
}

uint8_t Wad3File::ReadUint8(size_t offset) const
{
    if (offset >= m_buffer.size())
        throw std::runtime_error("Wad3File: Lesezugriff ausserhalb des Buffers");
    return m_buffer[offset];
}

int32_t Wad3File::ReadInt32(size_t offset) const
{
    if (offset + 4 > m_buffer.size())
        throw std::runtime_error("Wad3File: Lesezugriff ausserhalb des Buffers");
    int32_t value;
    std::memcpy(&value, m_buffer.data() + offset, sizeof(value));
    return value; // Annahme: Zielplattform ist little-endian (x86/x64)
}

uint32_t Wad3File::ReadUint32(size_t offset) const
{
    if (offset + 4 > m_buffer.size())
        throw std::runtime_error("Wad3File: Lesezugriff ausserhalb des Buffers");
    uint32_t value;
    std::memcpy(&value, m_buffer.data() + offset, sizeof(value));
    return value; // Annahme: Zielplattform ist little-endian (x86/x64)
}

// ------------------------------------------------------------------
// Header / Directory
// ------------------------------------------------------------------
void Wad3File::ReadHeader()
{
    const std::string magic = ReadString(0, 4);
    if (magic != "WAD3")
        throw std::runtime_error("Keine gueltige WAD3-Datei");

    m_numEntries = ReadInt32(4);
    m_dirOffset = ReadInt32(8);
}

void Wad3File::ReadDirectory()
{
    constexpr size_t ENTRY_SIZE = 32;

    m_entries.clear();
    m_entries.reserve(static_cast<size_t>(m_numEntries));

    for (int32_t i = 0; i < m_numEntries; i++)
    {
        const size_t off = static_cast<size_t>(m_dirOffset) + static_cast<size_t>(i) * ENTRY_SIZE;

        Entry entry;
        entry.filePos = ReadInt32(off + 0);
        entry.diskSize = ReadInt32(off + 4);
        entry.size = ReadInt32(off + 8);
        entry.type = ReadUint8(off + 12);
        entry.compression = ReadUint8(off + 13);
        entry.name = ReadString(off + 16, 16);

        m_entries.push_back(std::move(entry));
    }
}

// ------------------------------------------------------------------
// Public API
// ------------------------------------------------------------------
std::vector<std::string> Wad3File::GetTextureNames() const
{
    std::vector<std::string> names;
    names.reserve(m_entries.size());
    for (const auto& e : m_entries)
        names.push_back(e.name);
    return names;
}

std::optional<Wad3File::Entry> Wad3File::FindEntry(const std::string& name) const
{
    const std::string target = ToLower(name);

    auto it = std::find_if(m_entries.begin(), m_entries.end(),
        [&](const Entry& e) { return ToLower(e.name) == target; });

    if (it == m_entries.end())
        return std::nullopt;

    return *it;
}

Wad3File::Texture Wad3File::GetTexture(const std::string& name) const
{
    const auto entryOpt = FindEntry(name);
    if (!entryOpt)
        throw std::runtime_error("Textur nicht gefunden: " + name);

    const Entry& entry = *entryOpt;
    const size_t base = static_cast<size_t>(entry.filePos);

    const std::string texName = ReadString(base + 0, 16);
    const uint32_t width = ReadUint32(base + 16);
    const uint32_t height = ReadUint32(base + 20);

    const uint32_t mip0Offset = ReadUint32(base + 24);

    const size_t pixelOffset = base + mip0Offset;
    const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);

    if (pixelOffset + pixelCount > m_buffer.size())
        throw std::runtime_error("Wad3File: Texturdaten ausserhalb des Buffers");

    const uint8_t* indices = m_buffer.data() + pixelOffset;

    // Palette sitzt hinter allen 4 Mip-Leveln
    const size_t mip1Size = (static_cast<size_t>(width) >> 1) * (static_cast<size_t>(height) >> 1);
    const size_t mip2Size = (static_cast<size_t>(width) >> 2) * (static_cast<size_t>(height) >> 2);
    const size_t mip3Size = (static_cast<size_t>(width) >> 3) * (static_cast<size_t>(height) >> 3);

    const size_t paletteOffset =
        pixelOffset +
        pixelCount +
        mip1Size +
        mip2Size +
        mip3Size +
        2;

    if (paletteOffset + pixelCount * 3 > m_buffer.size())
        throw std::runtime_error("Wad3File: Palette ausserhalb des Buffers");

    Texture tex;
    tex.name = texName;
    tex.width = width;
    tex.height = height;
    tex.pixels.resize(pixelCount);

    const bool isTransparentTex = !texName.empty() && texName[0] == '{';

    for (size_t i = 0; i < pixelCount; i++)
    {
        const uint8_t idx = indices[i];

        const uint8_t r = ReadUint8(paletteOffset + static_cast<size_t>(idx) * 3 + 0);
        const uint8_t g = ReadUint8(paletteOffset + static_cast<size_t>(idx) * 3 + 1);
        const uint8_t b = ReadUint8(paletteOffset + static_cast<size_t>(idx) * 3 + 2);

        // Quake/HL transparent blue handling
        uint8_t a = 255;

        // Viele WAD3-Texturen verwenden Index 255 als transparent
        if (idx == 255 && isTransparentTex)
        {
            a = 0;
        }

        tex.pixels[i] =
            (static_cast<uint32_t>(a) << 24) |
            (static_cast<uint32_t>(b) << 16) |
            (static_cast<uint32_t>(g) << 8) |
            static_cast<uint32_t>(r);
    }

    return tex;
}