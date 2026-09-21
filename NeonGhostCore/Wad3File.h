#pragma once
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <stdexcept>
namespace ng {
    namespace map {
        namespace texture {

            class Wad3File
            {
            public:
                struct Entry
                {
                    int32_t     filePos;
                    int32_t     diskSize;
                    int32_t     size;
                    uint8_t     type;
                    uint8_t     compression;
                    std::string name;
                };

                struct Texture
                {
                    std::string           name;
                    uint32_t               width;
                    uint32_t               height;
                    std::vector<uint32_t>  pixels; // RGBA, ein uint32_t pro Pixel (0xAABBGGRR)
                };

                explicit Wad3File();
                // Übernimmt eine Kopie der Datei in den internen Buffer.
                explicit Wad3File(const std::vector<uint8_t>& fileData);

                // Bequemlichkeits-Konstruktor: liest die Datei direkt von der Platte.
                explicit Wad3File(const std::string& path);

                std::vector<std::string> GetTextureNames() const;

                // Liefert nullopt, wenn der Eintrag nicht existiert.
                std::optional<Entry> FindEntry(const std::string& name) const;

                // Wirft std::runtime_error, wenn die Textur nicht gefunden wird.
                Texture GetTexture(const std::string& name) const;

            private:
                std::vector<uint8_t> m_buffer;
                std::vector<Entry>   m_entries;

                int32_t m_numEntries = 0;
                int32_t m_dirOffset = 0;

                // --------------------------------------------------------------
                // Low-Level-Reader (little-endian, wie im WAD3-Format spezifiziert)
                // --------------------------------------------------------------
                std::string ReadString(size_t offset, size_t length) const;
                uint8_t     ReadUint8(size_t offset) const;
                int32_t     ReadInt32(size_t offset) const;
                uint32_t    ReadUint32(size_t offset) const;

                void ReadHeader();
                void ReadDirectory();
            };
        }
    }
}