#pragma once
#include <cstdint>
namespace ng {
	namespace tools {
		class LockTool {
		public:
			LockTool() {}
			~LockTool() {}
			inline bool IsLocked() { return m_key != 0; }
			inline void Lock(const uint64_t& key) {
				if (key != 0) return;
				m_key = key;
			}
			inline void UnLock(const uint64_t& key) {
				if (key == 0) return;
				if (m_key == key) m_key = 0;
			}
		private:

			uint64_t m_key = 0;
		};
	}
}