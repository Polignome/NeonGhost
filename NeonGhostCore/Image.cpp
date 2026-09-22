#include "pch.h"
#include "Image.h"
#include "stb_image.h"
using namespace ng;
using namespace renderer;
using namespace gfx;












_Image::_Image()
{

}
_Image::_Image(buffers::PixelBufferRGBA& buffer)
{
	Load(buffer);
}

_Image::_Image(const std::string& filename)
{
	Load(filename);
}

_Image::_Image(size_t size, void* buffer)
{
    Load(size, buffer);
}

_Image::_Image(const std::vector<uint8_t>& buffer)
{
    Load(buffer);
}




bool _Image::Load(const std::vector<uint8_t>& buffer)
{
    return Load(buffer.size(), (void*)buffer.data());

}


bool _Image::Load(buffers::PixelBufferRGBA& buffer)
{
    Resize(buffer.Width(), buffer.Height());
    memcpy(
        GetRawBuffer(),
        buffer.GetRawBuffer(),
        buffer.MemSize()
    );

}

bool _Image::Load(const std::string& filename)
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
    return true;
}

bool _Image::Load(size_t size,void* buffer)
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
    return true;

}

namespace ng {
    namespace resource {
 
        ImageResourceManager::ImageResourceManager():TemplateResourceManager<renderer::gfx::_Image>("IMAGE_RESOURCE") {}
        ImageResourceManager::~ImageResourceManager() {}
        

        uint64_t ImageResourceManager::LoadImageFromFile(std::string resname, std::string filename, bool keep_source, unsigned long attrib) {
            _Image* img = new _Image;
            bool err = img->Load(filename);
            if (!err) { delete img; img = nullptr; }
            return Add(resname, img, attrib);

        }
        uint64_t ImageResourceManager::LoadImageFromMemory(std::string resname, unsigned char* buffer, unsigned long buffer_size, bool keep_source, unsigned long attrib)
        {
                _Image* img = new _Image;
                bool err = img->Load(buffer_size, (void*)buffer);
                if (!err) { delete img; img = nullptr; }
                return Add(resname, img, attrib);

        }


        uint64_t ImageResourceManager::Add(std::string resname, renderer::gfx::_Image* ptr, unsigned long attrib){
            return AddResource(resname, ptr, attrib);
         }
    
        
         ng::renderer::gfx::_Image* ImageResourceManager::CreateDummyResourceEvent() {
             

             int width = 64;
             int height =64;
             int cheker_size=0;
             bool s = false;



             int32_t c1 = RGB(255, 255, 255);
             int32_t c2 = RGB(255, 0, 0);

             if (width < height) cheker_size = width / 8;
             else cheker_size = height / 8;

             ng::renderer::gfx::_Image* img = new ng::renderer::gfx::_Image;
             img->Resize(width, height);

             for (int x = 0; x < width; x++) {

                 if (x % cheker_size == 0) s = !s;

                 for (int y = 0; y < height; y++)
                 {

                     if (y % cheker_size == 0)  s = !s;
                     if (s) img->PutPixel(x, y, c1); else  img->PutPixel(x, y, c2);
                               
                 }
             }
             
             


             return img;
        }
        
            unsigned long ImageResourceManager::ParseXMLElement(tinyxml2::XMLElement * element)
            {
                if (element == nullptr) return 1;
                if (std::string("IMAGE") != std::string(element->Name())) {
                    return 1;
                }
                if (element->FindAttribute("name") == nullptr) {
                  
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
              
                    LoadImageFromFile(name, filename, keep_source, attrib);
                    return 0;
                }

              
                return 1;
            }
        

    }
}