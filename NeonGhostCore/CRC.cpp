#include "pch.h"
#include "CRC.h"


using namespace ng::tools;

const uint64_t poly = 0xC96C5795D7870F42;
CRC* CRC::m_instace = nullptr;
static CRC m_init_crc;


CRC::CRC() {
	if (m_instace != nullptr) return;
	m_instace = this;
	CRC::generate_table();
}


void CRC::generate_table()
{
	for (int i = 0; i < 256; ++i)
	{
		uint16_t crc = i;

		for (unsigned int j = 0; j < 8; ++j)
		{
			// is current coefficient set?
			if (crc & 1)
			{
				// yes, then assume it gets zero'd (by implied x^64 coefficient of dividend)
				crc >>= 1;

				// and add rest of the divisor
				crc ^= poly;
			}
			else
			{
				// no? then move to next coefficient
				crc >>= 1;
			}
		}

		table[i] = crc;
	}
}

// will give an example CRC calculation for input array {0xDE, 0xAD}
//
// each byte represents a group of 8 coefficients for 8 dividend terms
//
// the actual polynomial dividend is:
//
// = DE       AD       00 00 00 00 00 00 00 00 (hex)
// = 11011110 10101101 0000000000000000000...0 (binary)
//   || ||||  | | || |
//   || ||||  | | || +------------------------ x^71
//   || ||||  | | |+-------------------------- x^69
//   || ||||  | | +--------------------------- x^68
//   || ||||  | +----------------------------- x^66
//   || ||||  +------------------------------- x^64
//   || ||||  
//   || |||+---------------------------------- x^78
//   || ||+----------------------------------- x^77
//   || |+------------------------------------ x^76
//   || +------------------------------------- x^75
//   |+--------------------------------------- x^73
//   +---------------------------------------- x^72
//

// the basic idea behind how the table lookup results can be used with one
// another is that:
//
// Mod(A * x^n, P(x)) = Mod(x^n * Mod(A, P(X)), P(X))
//
// in other words, an input data shifted towards the higher degree terms
// changes the pre-computed crc of the input data by shifting it also
// the same amount towards higher degree terms (mod the polynomial)

// here is an example:
//
// 1) input:
//
//    00 00 00 00 00 00 00 00 AD DE
//          
// 2) index crc table for byte DE (really for dividend 00 00 00 00 00 00 00 00 DE)
//
//    we get A8B4AFBDC5A6ACA4
//
// 3) apply that to the input stream:
//
//    00 00 00 00 00 00 00 00 AD DE 
//       A8 B4 AF BD C5 A6 AC A4
//    -----------------------------
//    00 A8 B4 AF BD C5 A6 AC 09
//
// 4) index crc table for byte 09 (really for dividend 00 00 00 00 00 00 00 00 09)
// 
//    we get 448FCBB7FCB9E309
//
// 5) apply that to the input stream
//
//    00 A8 B4 AF BD C5 A6 AC 09
//    44 8F CB B7 FC B9 E3 09
//    --------------------------
//    44 27 7F 18 41 7C 45 A5
//
//

uint64_t CRC::calculate_crc(unsigned char* stream, unsigned int n)
{

	uint64_t crc = 0;

	for (unsigned int i = 0; i < n; ++i)
	{
		unsigned char index = (unsigned char)(stream[i] ^ crc);
		uint64_t lookup = table[index];

		crc >>= 8;
		crc ^= lookup;
	}

	return crc;
}



uint64_t CRC::CRC64(const std::string& s) {
	assert(m_instace);
	return m_instace->calculate_crc((unsigned char*)s.c_str(), (unsigned int)s.length());
}


uint64_t CRC::CRC64(unsigned char* stream, unsigned int n) {
	assert(m_instace);
	return m_instace->calculate_crc(stream, n);
}
