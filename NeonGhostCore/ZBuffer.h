#pragma once
#include <vector>
#include "Lock.h"

namespace ng {
	namespace renderer {
		namespace buffers {
		
			template <class T>
			class ZBuffer :public ng::tools::LockTool {
			public:
				ZBuffer(int width = 0, int height = 0) :m_width(0), m_height(0),m_size(0), m_mem_size(0)
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
						m_size = 0;
						m_mem_size = 0;
						return;
					}
					m_height = height;
					m_width = width;
					m_size = m_width * m_height;
					m_mem_size = m_size * sizeof(T);
					m_buffer = std::shared_ptr<T[]>(new T[m_size]);
					Clear();
				}

				inline int Width() { return m_width; }
				inline int Height() { return m_height; }
				inline size_t Size() { return m_size;	}
				inline T* GetRawBuffer() {return m_buffer.get(); }
				inline size_t MemSize() { return m_mem_size; }

				inline void Clear(T data = 0) {
					std::fill(m_buffer.get(), m_buffer.get() + m_width * m_height, data);
				}

				inline void SetDepth(const unsigned int& x, const  unsigned& y, const  T& depth) {
					m_buffer.get()[(x + y * Width()) % Size()] = depth;

				}

				inline void SetDepth(const uint32_t& offset, const T& depth) {
					m_buffer.get()[offset % Size()] = depth;

				}

				inline T GetDepth(const unsigned int& x, const  unsigned& y) {
					return m_buffer.get()[(x + y * Width()) % Size()];

				}

				inline T GetDepth(const uint32_t& offset) {
					return m_buffer.get()[offset % Size()];

				}


				~ZBuffer()
				{
					m_buffer.reset();

				}
			private:
				int m_width;
				int m_height;
				size_t m_size = 0;
				size_t  m_mem_size = 0;
				std::shared_ptr<T[]> m_buffer;
			};
		}
	}
}