#pragma once
#include <memory>
#include <vector>

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include "Global.h"
#include "matrix.h"
#include "Lock.h"
namespace ng {
	namespace renderer {
		namespace buffers {
			class PixelBufferRGBA :public ng::tools::LockTool {
			public:
				PixelBufferRGBA(int width = 0, int height = 0) :m_width(0),m_height(0),m_size(0),m_mem_size(0)
				{
					Resize(width, height);
				}

				inline void Resize(int width, int height) {
					if (IsLocked()) return;


					if (width == m_width && height == m_height) return;

					m_buffer.reset();

					if (width <= 0 || height <= 0) {
						m_height = 0;
						m_width = 0;
						return;
					}
					m_height = height;
					m_width = width;
					m_size = m_width * m_height;
					m_mem_size = m_size * sizeof(uint32_t);
					m_buffer = std::shared_ptr<uint32_t[]>(new uint32_t[m_size]);
					Clear();
				}

				inline int Width() const { return m_width;}
				inline int Height() const { return m_height; }
				inline size_t Size() const { return m_size; }
				inline size_t MemSize() const { return m_mem_size; }
				inline uint32_t* GetRawBuffer() const { return m_buffer.get(); }

				inline void Clear(const uint32_t &color=0x000000ff) {
					std::fill(m_buffer.get(), m_buffer.get() + m_size, color);
				}
				inline void Clear(const uint8_t& r, const uint8_t& g, const uint8_t& b, const uint8_t& a=0xff)
				{
					Clear(RGBA(r, g, b, a));
				}
				inline void Clear(const float& r, const float& g, const float& b, const float& a = 1.0f)
				{
					Clear(RGBA(uint8_t(r * 255.0f), uint8_t(g * 255.0f), uint8_t(b * 255.0f), uint8_t(a * 255.0f)));
				}
				inline void Clear(const double& r, const double& g, const double& b, const double& a = 1.0)
				{
					Clear(RGBA(uint8_t(r * 255.0), uint8_t(g * 255.0), uint8_t(b * 255.0), uint8_t(a * 255.0)));

				}
				inline void Clear(const math::Vector3f& color) { Clear(color.x(), color.y(), color.z(), 1.0f); }
				inline void Clear(const math::Vector3d& color) { Clear(color.x(), color.y(), color.z(), 1.0); }
				inline void Clear(const math::Vector4f& color) { Clear(color.x(), color.y(), color.z(), color.w()); }
				inline void Clear(const math::Vector4d& color) { Clear(color.x(), color.y(), color.z(), color.w()); }

				inline void PutPixel(const unsigned int& x, const  unsigned& y, const  uint32_t& color) {
					m_buffer.get()[(x + y * Width()) % Size()] = color;

				}

				inline void PutPixel(const uint32_t& offset, const uint32_t& color) {
					m_buffer.get()[offset % Size()] = color;

				}

				inline uint32_t GetPixel(const unsigned int& x, const  unsigned& y) {
					return m_buffer.get()[(x + y * Width()) % Size()];

				}

				inline uint32_t GetPixel(const uint32_t& offset) {
					return m_buffer.get()[offset % Size()];

				}

				// NEU: const - reines Lesen, veraendert den Objektzustand nicht.
	// shared_ptr::get() ist auch auf einem const shared_ptr aufrufbar,
	// daher funktioniert der Zugriff unveraendert.
				inline uint32_t GetPixel(const unsigned int& x, const  unsigned& y) const {
					return m_buffer.get()[(x + y * Width()) % Size()];

				}

				// NEU: const, aus demselben Grund.
				inline uint32_t GetPixel(const uint32_t& offset) const {
					return m_buffer.get()[offset % Size()];

				}




				inline void CopyFrom( PixelBufferRGBA src) {
					if (src.MemSize() == MemSize()) {
						std::memcpy(GetRawBuffer(),src.GetRawBuffer(),MemSize());
						return;
					}
					
					
					
					CopyFrom(src, 0, 0, Width(), Height(),0,0);
				}


				// In PixelBufferRGBA (Texture.h), oeffentliche Methode.

				inline bool SaveAsBMP(const char* path, float boost = 1.0f) const
				{
					const int w = Width(), h = Height();
					if (w <= 0 || h <= 0) return false;

					const int rowBytes = (w * 3 + 3) & ~3;                 // Zeilen auf 4 Byte aufgefuellt
					const uint32_t dataSize = (uint32_t)(rowBytes * h);
					const uint32_t fileSize = 54 + dataSize;

#ifdef _MSC_VER
					FILE* f = nullptr;
					if (fopen_s(&f, path, "wb") != 0 || !f) return false;
#else
					FILE* f = fopen(path, "wb");
					if (!f) return false;
#endif

					unsigned char hdr[54] = { 0 };
					hdr[0] = 'B'; hdr[1] = 'M';
					const uint32_t off = 54, dib = 40;
					const uint16_t planes = 1, bpp = 24;
					memcpy(hdr + 2, &fileSize, 4);
					memcpy(hdr + 10, &off, 4);
					memcpy(hdr + 14, &dib, 4);
					memcpy(hdr + 18, &w, 4);
					memcpy(hdr + 22, &h, 4);
					memcpy(hdr + 26, &planes, 2);
					memcpy(hdr + 28, &bpp, 2);
					memcpy(hdr + 34, &dataSize, 4);
					fwrite(hdr, 1, 54, f);

					auto conv = [boost](uint32_t v) -> unsigned char
						{
							return (unsigned char)std::min(255.0f, float(v) * boost);
						};

					std::vector<unsigned char> row(rowBytes, 0);
					for (int y = h - 1; y >= 0; --y)                       // BMP speichert von unten nach oben
					{
						for (int x = 0; x < w; ++x)
						{
							const uint32_t px = GetPixel(x, y);
							row[x * 3 + 0] = conv(RGBToBlue(px));
							row[x * 3 + 1] = conv(RGBToGreen(px));
							row[x * 3 + 2] = conv(RGBToRed(px));
						}
						fwrite(row.data(), 1, rowBytes, f);
					}

					fclose(f);
					return true;
				}

				inline void CopyFrom(
					const PixelBufferRGBA& src,
					int srcX,
					int srcY,
					int width,
					int height,
					int dstX,
					int dstY)
				{
					if (width <= 0 || height <= 0)
						return;

					if (srcX < 0 || srcY < 0 ||
						dstX < 0 || dstY < 0)
						return;

					width = std::min(
						width,
						src.m_width - srcX);

					height = std::min(
						height,
						src.m_height - srcY);

					width = std::min(
						width,
						m_width - dstX);

					height = std::min(
						height,
						m_height - dstY);

					if (width <= 0 || height <= 0)
						return;

					const size_t rowBytes =
						static_cast<size_t>(width) * sizeof(uint32_t);

					for (int y = 0; y < height; ++y)
					{
						uint32_t* dst =
							m_buffer.get() +
							(dstY + y) * m_width +
							dstX;

						const uint32_t* srcPtr =
							src.m_buffer.get() +
							(srcY + y) * src.m_width +
							srcX;

						std::memcpy(
							dst,
							srcPtr,
							rowBytes);
					}
				}

				~PixelBufferRGBA()
				{
					m_buffer.reset();

				}
			private:
				int m_width;
				int m_height;
				size_t m_size;
				size_t m_mem_size;
				std::shared_ptr<uint32_t[]> m_buffer;
			};

		
		}


	}
}