#pragma once
#include "Global.h"
#include <cstddef>

namespace ng {
	namespace tools {
		class CRC {
		public:
			CRC();
			~CRC() {}
			static uint64_t CRC64(unsigned char* stream, unsigned int n);
			static uint64_t CRC64(const std::string& s);
		private:
			uint64_t calculate_crc(unsigned char* stream, unsigned int n);
			void generate_table();
			uint64_t table[256];
			static CRC* m_instace;
		};

	}
}
