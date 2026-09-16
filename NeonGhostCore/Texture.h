#pragma once
#include "Global.h"
#include "PixelBuffer.h"
#include "ResourceManager.h"



namespace ng {
    namespace renderer {
        namespace gfx {
            class _Texture : public buffers::PixelBufferRGBA
            {
            public:
                _Texture();
                _Texture(buffers::PixelBufferRGBA& buffer);
                _Texture(const std::string& filename);
                _Texture(size_t size, void* buffer);
                _Texture(const std::vector<uint8_t>& buffer);
                bool Load(buffers::PixelBufferRGBA& buffer);
                bool Load(const std::string& filename);
                bool Load(size_t size, void* buffer);
                bool Load(const std::vector<uint8_t>& buffer);

                // NEU: Mip-Kette. Wird einmalig am Ende jedes erfolgreichen
                // Load()-Aufrufs gebaut (siehe BuildMips()), nicht pro Frame.
                size_t MipCount() const { return m_mips.size(); }
                const buffers::PixelBufferRGBA& Mip(size_t level) const { return m_mips[level]; }


                inline size_t GetMipLevels() const { return m_mips.size(); }

                inline const buffers::PixelBufferRGBA& GetMipLevel(int level = 0) const {
                    if (level <= 0 || level > (int)m_mips.size()) return *this;
                    return m_mips[level - 1];
                }
                inline  buffers::PixelBufferRGBA& GetMipLevel(int level = 0) {
                    if (level <= 0 || level > (int)m_mips.size()) return *this;
                    return m_mips[level - 1];
                }
            protected:
            private:
                // NEU: baut die komplette Pyramide per 2x2-Box-Filter bis 1x1.
                // MUSS am Ende jedes Load()-Pfads aufgerufen werden, der die
                // Pixeldaten (neu) befuellt.
                void BuildMips();
                std::vector<buffers::PixelBufferRGBA> m_mips;
            };
            typedef ng::resource::Resource<_Texture> Texture;
        }
    }

    namespace resource {
        class TextureResourceManager :public TemplateResourceManager<renderer::gfx::_Texture>
        {
        public:
            TextureResourceManager();
            ~TextureResourceManager();


            static uint64_t LoadTextureFromFile(std::string resname, std::string filename, bool keep_source = false, unsigned long attrib = DELETE_WHEN_UNUSED);
            static uint64_t LoadTextureFromMemory(std::string resname, unsigned char* buffer, unsigned long buffer_size, bool keep_source = false, unsigned long attrib = DELETE_WHEN_UNUSED);

        protected:
            static uint64_t Add(std::string resname, renderer::gfx::_Texture* ptr, unsigned long attrib = DELETE_WHEN_UNUSED);
            virtual ng::renderer::gfx::_Texture* CreateDummyResourceEvent();
            virtual unsigned long ParseXMLElement(tinyxml2::XMLElement* element);
        };
    }


}
