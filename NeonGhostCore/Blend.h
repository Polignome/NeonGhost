#pragma once
#include "Global.h"
#include <cstdint>

namespace ng {
	namespace renderer {
		namespace gfx2d {
			namespace blend {

				using BlendFunc = uint32_t(*)(uint32_t dst, uint32_t src);

				inline uint32_t Opaque(uint32_t /*dst*/, uint32_t src)
				{
					return src;
				}

				inline uint32_t Alpha(uint32_t dst, uint32_t src)
				{
					uint32_t srcA = (src >> 24) & 0xFF;
					if (srcA == 0)   return dst;
					if (srcA == 255) return src;

					uint32_t srcR = (src >> 16) & 0xFF, srcG = (src >> 8) & 0xFF, srcB = src & 0xFF;
					uint32_t dstA = (dst >> 24) & 0xFF, dstR = (dst >> 16) & 0xFF, dstG = (dst >> 8) & 0xFF, dstB = dst & 0xFF;

					uint32_t outR = (srcR * srcA + dstR * (255 - srcA)) / 255;
					uint32_t outG = (srcG * srcA + dstG * (255 - srcA)) / 255;
					uint32_t outB = (srcB * srcA + dstB * (255 - srcA)) / 255;
					uint32_t outA = std::max(srcA, dstA);

					return (outA << 24) | (outR << 16) | (outG << 8) | outB;
				}

				inline uint32_t Additive(uint32_t dst, uint32_t src)
				{
					uint32_t r = std::min(255u, ((dst >> 16) & 0xFF) + ((src >> 16) & 0xFF));
					uint32_t g = std::min(255u, ((dst >> 8) & 0xFF) + ((src >> 8) & 0xFF));
					uint32_t b = std::min(255u, (dst & 0xFF) + (src & 0xFF));
					return (0xFFu << 24) | (r << 16) | (g << 8) | b;
				}

				inline uint32_t Multiply(uint32_t dst, uint32_t src)
				{
					uint32_t srcR = (src >> 16) & 0xFF, srcG = (src >> 8) & 0xFF, srcB = src & 0xFF;
					uint32_t dstR = (dst >> 16) & 0xFF, dstG = (dst >> 8) & 0xFF, dstB = dst & 0xFF;

					uint32_t r = (srcR * dstR) / 255;
					uint32_t g = (srcG * dstG) / 255;
					uint32_t b = (srcB * dstB) / 255;

					return (0xFFu << 24) | (r << 16) | (g << 8) | b;
				}

				inline uint32_t Screen(uint32_t dst, uint32_t src)
				{
					uint32_t srcR = (src >> 16) & 0xFF, srcG = (src >> 8) & 0xFF, srcB = src & 0xFF;
					uint32_t dstR = (dst >> 16) & 0xFF, dstG = (dst >> 8) & 0xFF, dstB = dst & 0xFF;

					uint32_t r = 255 - (((255 - srcR) * (255 - dstR)) / 255);
					uint32_t g = 255 - (((255 - srcG) * (255 - dstG)) / 255);
					uint32_t b = 255 - (((255 - srcB) * (255 - dstB)) / 255);

					return (0xFFu << 24) | (r << 16) | (g << 8) | b;
				}

				inline uint32_t Subtract(uint32_t dst, uint32_t src)
				{
					int srcR = (src >> 16) & 0xFF, srcG = (src >> 8) & 0xFF, srcB = src & 0xFF;
					int dstR = (dst >> 16) & 0xFF, dstG = (dst >> 8) & 0xFF, dstB = dst & 0xFF;

					uint32_t r = (uint32_t)std::max(0, dstR - srcR);
					uint32_t g = (uint32_t)std::max(0, dstG - srcG);
					uint32_t b = (uint32_t)std::max(0, dstB - srcB);

					return (0xFFu << 24) | (r << 16) | (g << 8) | b;
				}

				constexpr uint32_t COLORKEY_NONE = 0xFFFFFFFF;


				struct ColorKey
				{
					bool enabled = false;
					uint32_t color = 0;
				};

			}
		}
	}
}

