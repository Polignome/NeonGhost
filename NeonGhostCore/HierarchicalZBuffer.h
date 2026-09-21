#pragma once
#include <vector>
#include "ZBuffer.h"
#include "Lock.h"

namespace ng {
	namespace renderer {
		namespace buffers {
			const int MAX_HZB_LEVEL = 8;


			template <class T>
			class HierarchicalZBuffer :public ng::tools::LockTool
			{
			public:
				HierarchicalZBuffer(const int& width = 0,const int& height = 0,const int& mip_levels = MAX_HZB_LEVEL) : m_width(0), m_height(0), n_num_mip_levels(mip_levels), m_size(0), m_clear_depth(0)
				{
					Resize(width, height, mip_levels);
				}
				~HierarchicalZBuffer() {}
				
				void Resize(const int& width, const int& height, const int& mip_levels)
				{
					if (IsLocked()) return;
						
					if (m_width == width && m_height == height && mip_levels == n_num_mip_levels) return;
					
					// FIX (2): einfache Validierung
					if (width <= 0 || height <= 0 || mip_levels <= 0) return;

					n_num_mip_levels = mip_levels;
					m_width = width;
					m_height = height;
					m_size = m_width * m_height;
					m_depth_buffer.Resize(m_width, m_height);
					m_mip_levels.clear();

					int w = m_width;
					int h = m_height;

					for (int level = 0; level < n_num_mip_levels; level++)
					{
						
						m_mip_levels.push_back(ZBuffer<T>((size_t)w, (size_t)h));
						w = std::max(1, w >> 1);
						h = std::max(1, h >> 1);
					}
				}

				inline int Width() { return m_width; }
				inline int Height() { return m_height; }

				void Clear(bool build_HZB = true)
				{
					if (build_HZB) BuildMipMaps();
					
					m_depth_buffer.Clear(m_clear_depth);
				}

				void Clear(const T& z, bool build_HZB = true)
				{
					if (build_HZB) BuildMipMaps();
					m_depth_buffer.Clear(m_clear_depth);
				}
				int GetNumMipMapLevels() { return n_num_mip_levels; }
				
			

				void BuildMipMaps()
				{
					if (n_num_mip_levels <= 0 || m_mip_levels.empty() || m_size <= 0) return;

					m_mip_levels[0] = m_depth_buffer;
					std::memmove(m_mip_levels[0].GetRawBuffer(), m_depth_buffer.GetRawBuffer(), m_depth_buffer.MemSize());

					T* src = m_depth_buffer.GetRawBuffer();
					int srcW = m_width;
					int srcH = m_height;

					for (int level = 1; level < n_num_mip_levels; level++)
					{
						int dstW = std::max(1, srcW >> 1);
						int dstH = std::max(1, srcH >> 1);
						T* dst = m_mip_levels[level].GetRawBuffer();

						for (int y = 0; y < dstH; ++y)
						{
							int sy0 = y * 2;
							int sy1 = std::min(sy0 + 1, srcH - 1);
							const T* row0 = src + sy0 * srcW;
							const T* row1 = src + sy1 * srcW;
							T* out = dst + y * dstW;

							for (int x = 0; x < dstW; ++x)
							{
								int sx0 = x * 2;
								int sx1 = std::min(sx0 + 1, srcW - 1); // WICHTIG: min, nicht max!

								out[x] = std::min(
									std::min(row0[sx0], row0[sx1]),
									std::min(row1[sx0], row1[sx1]));
								
							}
						}
						src = dst;
						srcW = dstW;
						srcH = dstH;
					}
				}

				T* GetRawHZBBuffer(int level)
				{

					if (level < 0 || (size_t)level >= m_mip_levels.size()) return m_depth_buffer.GetRawBuffer();
					return m_mip_levels[level].GetRawBuffer();
				}

				ZBuffer<T> GetZBuffer(int level)
				{

					if (level < 0 || (size_t)level >= m_mip_levels.size()) return m_depth_buffer;
					return m_mip_levels[level];
				}

				T* GetRawBuffer() {
					return m_depth_buffer.GetRawBuffer();
				}

			protected:
			private:

				int m_width;
				int m_height;
				int n_num_mip_levels;
				uint32_t m_size;
				T m_clear_depth;
				ZBuffer<T>  m_depth_buffer;
				std::vector<ZBuffer<T>>  m_mip_levels;
			};
		}
	}
}