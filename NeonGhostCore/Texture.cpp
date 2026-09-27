#include "pch.h"
#include "Texture.h"
#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#endif
using namespace ng;
using namespace renderer;
using namespace gfx;


_Texture::_Texture()
{

}
_Texture::_Texture( buffers::PixelBufferRGBA& buffer)
{
    Load( buffer);
}

_Texture::_Texture(const std::string& filename)
{
    Load(filename);
}

_Texture::_Texture(size_t size, void* buffer)
{
    Load(size, buffer);
}

_Texture::_Texture(const std::vector<uint8_t>& buffer)
{
    Load(buffer);
}


bool _Texture::Load(const std::vector<uint8_t>& buffer)
{
    return Load(buffer.size(), (void*)buffer.data());
}


bool _Texture::Load( buffers::PixelBufferRGBA& buffer)
{
    Resize(buffer.Width(), buffer.Height());
    memcpy(
        GetRawBuffer(),
        buffer.GetRawBuffer(),
        buffer.MemSize()
    );
    BuildMips();
    return true;
}

bool _Texture::Load(const std::string& filename)
{
    int width;
    int height;
    int channels;

    unsigned char* pixels = stbi_load(
        filename.c_str(),
        &width,
        &height,
        &channels,
        STBI_rgb_alpha
    );

    if (!pixels) {
        return false;
    }
    Resize(width, height);

    memcpy(
        GetRawBuffer(),
        pixels,
        width * height * sizeof(uint32_t)
    );

    stbi_image_free(pixels);
    BuildMips();
    return true;
}

bool _Texture::Load(size_t size, void* buffer)
{
    int width;
    int height;
    int channels;

    unsigned char* pixels = stbi_load_from_memory(
        (stbi_uc*)buffer,           // Bilddaten im Speicher
        (int)size,      // Größe in Bytes
        &width,
        &height,
        &channels,
        STBI_rgb_alpha  // immer RGBA
    );

    if (!pixels) {
        
        return false;
    }
    Resize(width, height);
    

    memcpy(
        GetRawBuffer(),
        pixels,
        width * height * sizeof(uint32_t)
    );

    stbi_image_free(pixels);
    BuildMips();
    return true;

}

void _Texture::BuildMips()
{
    m_mips.clear();
    int w = Width(), h = Height();
    const uint32_t* prev = GetRawBuffer();
    while (w > 1 || h > 1)
    {
        int nw = std::max(1, w / 2);
        int nh = std::max(1, h / 2);
        buffers::PixelBufferRGBA level;
        level.Resize(nw, nh);
        uint32_t* dst = level.GetRawBuffer();
        for (int y = 0; y < nh; ++y)
        {
            for (int x = 0; x < nw; ++x)
            {
                int x0 = std::min(x * 2, w - 1), x1 = std::min(x * 2 + 1, w - 1);
                int y0 = std::min(y * 2, h - 1), y1 = std::min(y * 2 + 1, h - 1);
                uint32_t c00 = prev[y0 * w + x0], c10 = prev[y0 * w + x1];
                uint32_t c01 = prev[y1 * w + x0], c11 = prev[y1 * w + x1];
                auto avg = [&](int shift) {
                    uint32_t a = (c00 >> shift) & 0xff, b = (c10 >> shift) & 0xff;
                    uint32_t c = (c01 >> shift) & 0xff, d = (c11 >> shift) & 0xff;
                    return (a + b + c + d) / 4;
                    };
                dst[y * nw + x] = (avg(24) << 24) | (avg(16) << 16) | (avg(8) << 8) | avg(0);
            }
        }
        m_mips.push_back(std::move(level));
        prev = m_mips.back().GetRawBuffer();
        w = nw; h = nh;
    }

}

std::vector<buffers::PixelBufferRGBA> m_mips;
            




namespace ng {
    namespace resource {

        TextureResourceManager::TextureResourceManager() :TemplateResourceManager<renderer::gfx::_Texture>("TEXTURE_RESOURCE") {}
        TextureResourceManager::~TextureResourceManager() {}


        uint64_t TextureResourceManager::LoadTextureFromPixelBuffer(std::string resname, ng::renderer::buffers::PixelBufferRGBA buffer, bool keep_source  , unsigned long attrib)
        {
            if (buffer.Width() * buffer.Height() <= 0 || buffer.GetRawBuffer() == nullptr) return Add(resname, nullptr, attrib);
            ng::renderer::buffers::PixelBufferRGBA raw(buffer.Width(), buffer.Height());
            memmove(raw.GetRawBuffer(), buffer.GetRawBuffer(), buffer.Width()* buffer.Height() * sizeof(uint32_t));
            _Texture* img = new _Texture(raw);

            return Add(resname, img, attrib);

        }




        uint64_t TextureResourceManager::LoadTextureFromFile(std::string resname, std::string filename, bool keep_source, unsigned long attrib) {
            _Texture* img = new _Texture;
            bool err = img->Load(filename);
            if (!err) { delete img; img = nullptr; }
            return Add(resname, img, attrib);

        }
        uint64_t TextureResourceManager::LoadTextureFromMemory(std::string resname, unsigned char* buffer, unsigned long buffer_size, bool keep_source, unsigned long attrib)
        {
            _Texture* img = new _Texture;
            bool err = img->Load(buffer_size, (void*)buffer);
            if (!err) { delete img; img = nullptr; }
            return Add(resname, img, attrib);

        }

        uint64_t TextureResourceManager::LoadRawTextureFromMemory(std::string resname, unsigned char* buffer, unsigned long width, unsigned long height, bool keep_source, unsigned long attrib)
        {
            
            if (width * height <= 0 || buffer == nullptr) return Add(resname, nullptr, attrib);
            ng::renderer::buffers::PixelBufferRGBA raw(width, height);
            memmove(raw.GetRawBuffer(), buffer, width * height * sizeof(uint32_t));
            _Texture* img = new _Texture(raw);

            return Add(resname, img, attrib);

        }



        uint64_t TextureResourceManager::Add(std::string resname, renderer::gfx::_Texture* ptr, unsigned long attrib) {
            return AddResource(resname, ptr, attrib);
        }


        ng::renderer::gfx::_Texture* TextureResourceManager::CreateDummyResourceEvent() {


            int width = 64 * 2;
            int height = 64 * 2;
            int cheker_size = 0;
            bool s = false;



            int32_t c1 = RGB(255, 255, 255);
            int32_t c2 = RGB(255, 0, 0);

            if (width < height) cheker_size = width / 8;
            else cheker_size = height / 8;


            ng::renderer::buffers::PixelBufferRGBA pixels(width, height);



            for (int x = 0; x < width; x++) {

                if (x % cheker_size == 0) s = !s;

                for (int y = 0; y < height; y++)
                {

                    if (y % cheker_size == 0)  s = !s;
                    if (s) pixels.PutPixel(x, y, c1); else pixels.PutPixel(x, y, c2);

                }
            }


            ng::renderer::gfx::_Texture* img = new ng::renderer::gfx::_Texture;
            img->Load(pixels);


            return img;
        }

        unsigned long TextureResourceManager::ParseXMLElement(tinyxml2::XMLElement* element)
        {
            if (element == nullptr) return 1;
            if (std::string("IMAGE") != std::string(element->Name())) {
                return 1;
            }
            if (element->FindAttribute("name") == nullptr) {
                //  std::cout << "IMAGE-Element ohne 'name'-Attribut ignoriert" << std::endl;
                return 1;
            }
            std::string name = element->Attribute("name");

            bool keep_source = false;
            if (element->FindAttribute("keep_source") != nullptr) {
                keep_source = element->BoolAttribute("keep_source");
            }

            unsigned long attrib = ng::resource::DELETE_WHEN_UNUSED;
            if (element->FindAttribute("attrib") != nullptr) {
                attrib = (unsigned long)element->UnsignedAttribute("attrib");
            }

            if (element->FindAttribute("filename") != nullptr) {
                std::string filename = ng::resource::Manager::GetResourcePath() + element->Attribute("filename");
                //    std::cout << "IMAGE " << name << " <- " << filename << std::endl;
                LoadTextureFromFile(name, filename, keep_source, attrib);
                return 0;
            }

            //  std::cout << "IMAGE '" << name << "' hat kein unterstuetztes Quell-Attribut (filename)" << std::endl;
            return 1;
        }


    }
}