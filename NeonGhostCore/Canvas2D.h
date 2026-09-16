#pragma once
#include "PixelBuffer.h"
#include "Blend.h"
#include "Sprite.h"
namespace ng {
	namespace renderer {
		namespace gfx2d {
			
		

			class Canvas2D
			{
			public:

				

				Canvas2D(buffers::PixelBufferRGBA& target);
				void DrawLine(int x0, int y0, int x1, int y1, uint32_t color);
				void DrawLine(int x0, int y0, int x1, int y1, uint32_t color0, uint32_t color1);
				void DrawRectangle(int x, int y, int w, int h, uint32_t color);
				void DrawBox(int x, int y, int w, int h, uint32_t color);
				void DrawCircle(int cx, int cy, int r, uint32_t color);
				void DrawCircleFilled(int cx, int cy, int r, uint32_t color);
				void DrawEllipse(int cx, int cy, int rx, int ry, uint32_t color);
				void DrawEllipseFilled(int cx, int cy, int rx, int ry, uint32_t color);
				inline void Clear(const uint32_t& color = 0x000000ff) {
					m_target.Clear(color);
				}
				inline void Clear(const uint8_t& r, const uint8_t& g, const uint8_t& b, const uint8_t& a = 0xff)
				{
					m_target.Clear(RGBA(r, g, b, a));
				}
				inline void Clear(const float& r, const float& g, const float& b, const float& a = 1.0f)
				{
					m_target.Clear(RGBA(uint8_t(r * 255.0f), uint8_t(g * 255.0f), uint8_t(b * 255.0f), uint8_t(a * 255.0f)));
				}
				inline void Clear(const double& r, const double& g, const double& b, const double& a = 1.0)
				{
					m_target.Clear(uint8_t(r * 255.0), uint8_t(g * 255.0), uint8_t(b * 255.0), uint8_t(a * 255.0));

				}
				inline void Clear(const math::Vector3f& color) { m_target.Clear(color.x(), color.y(), color.z(), 1.0f); }
				inline void Clear(const math::Vector3d& color) { m_target.Clear(color.x(), color.y(), color.z(), 1.0); }
				inline void Clear(const math::Vector4f& color) { m_target.Clear(color.x(), color.y(), color.z(), color.w()); }
				inline void Clear(const math::Vector4d& color) { m_target.Clear(color.x(), color.y(), color.z(), color.w()); }
				

				void DrawSprite(
				    buffers::PixelBufferRGBA& sprite,
					float x, float y,
					float scalex = 1.0, float scaley = 1.0, float rotationRad=0,
					blend::BlendFunc blend=blend::Opaque,
					 const blend::ColorKey &key = {});

				void DrawSprite(gfx2d::Sprite<double> sprite);
				void DrawSprite(gfx2d::Sprite<float> sprite);

				int Width() { return m_target.Width(); }
				int Height() { return m_target.Height(); }


				~Canvas2D();


			private:
				buffers::PixelBufferRGBA& m_target;

				inline bool InBounds(int x, int y, int w, int h)
				{
					return x >= 0 && y >= 0 && x < w && y < h;
				}

				inline void PutPixelClipped( int x, int y, uint32_t color)
				{
					if (InBounds(x, y, m_target.Width(), m_target.Height()))
						m_target.PutPixel(x, y, color);
				}

				inline void FillSpan(int x, int y, int length, uint32_t color)
				{
					uint32_t* row = m_target.GetRawBuffer() + y * m_target.Width() + x;
					std::fill(row, row + length, color);
				}


			};
		}
	}
}