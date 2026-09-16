#pragma once
#include "Global.h"
#include "Matrix.h"
#include "Pixelbuffer.h"
#include "AABBox2D.h"
#include "BoundingCircle2D.h"
#include "Object2D.h"
#include "Blend.h"

namespace ng
{
    namespace renderer
    {
        namespace gfx2d
        {
			
			template<typename T>
			class Sprite : public objects::Object2D<T>
			{
			public:
				using Vec2 = typename objects::Object2D<T>::Vec2;

				Sprite() : m_texture(&DummyTexture()) {}
				explicit Sprite(buffers::PixelBufferRGBA& texture) : m_texture(&texture) {}

				void SetTexture(buffers::PixelBufferRGBA& texture) { m_texture = &texture; this->MarkBoundsDirty(); }
				buffers::PixelBufferRGBA& Texture() { return *m_texture; }
				const buffers::PixelBufferRGBA& Texture() const { return *m_texture; }
				bool IsEmpty() const { return m_texture == &DummyTexture(); }

				
				void SetBlendFunc(blend::BlendFunc fn) { m_blend = fn; }
				const blend::BlendFunc GetBlendFunc() const { return m_blend; }

				void SetColorKey(const blend::ColorKey& key) { m_colorKey = key; }
				const blend::ColorKey& GetColorKey() const { return m_colorKey; }
			

			protected:
				Vec2 GetHalfExtents() const override
				{
					return Vec2(static_cast<T>(m_texture->Width()) * T(0.5),
						static_cast<T>(m_texture->Height()) * T(0.5));
				}

			private:
				static buffers::PixelBufferRGBA& DummyTexture()
				{
					static buffers::PixelBufferRGBA dummy(0, 0);
					return dummy;
				}

				buffers::PixelBufferRGBA* m_texture;
				blend::BlendFunc m_blend = nullptr;
				blend::ColorKey m_colorKey{};
			};
        }
    }
}