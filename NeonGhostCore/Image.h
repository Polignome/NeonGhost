#pragma once
#include "Global.h"
#include <string>
#include "PixelBuffer.h"
#include <vector>
#include "ResourceManager.h"

namespace ng {
    namespace renderer {
        namespace gfx {
            class _Image:public buffers::PixelBufferRGBA
            {
            public:
                _Image();
                _Image(buffers::PixelBufferRGBA& buffer);
                _Image(const std::string& filename);
                _Image(size_t size, void* buffer);
                _Image(const std::vector<uint8_t>& buffer);


                bool Load(buffers::PixelBufferRGBA& buffer);
                bool Load(const std::string& filename);
                bool Load(size_t size, void* buffer);
                bool Load(const std::vector<uint8_t>& buffer);

            protected:
            private:
            };
            typedef ng::resource::Resource<_Image> Image;
        
        }
    }


    namespace resource {
        class ImageResourceManager :public TemplateResourceManager<renderer::gfx::_Image>
        {
        public:
            ImageResourceManager();
            ~ImageResourceManager();


            static uint64_t LoadImageFromFile(std::string resname, std::string filename, bool keep_source = false, unsigned long attrib = DELETE_WHEN_UNUSED);
            static uint64_t LoadImageFromMemory(std::string resname, unsigned char* buffer, unsigned long buffer_size, bool keep_source = false, unsigned long attrib = DELETE_WHEN_UNUSED);

        protected:
            static uint64_t Add(std::string resname, renderer::gfx::_Image* ptr, unsigned long attrib = DELETE_WHEN_UNUSED);
            virtual ng::renderer::gfx::_Image *CreateDummyResourceEvent();
            virtual unsigned long ParseXMLElement(tinyxml2::XMLElement* element);
        };
    }
}
