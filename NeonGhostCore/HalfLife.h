#pragma once
#include "Parser.h"




namespace ng {
	namespace map {
		class HalfLife :
			public  Parser
		{
		public:
			HalfLife() {}
			struct RawLight
			{
				std::string origin; // "x y z"
				std::string light;  // "r g b brightness" oder "r g b" oder leer
			};
			std::vector<RawLight> m_rawLights;

			virtual ~HalfLife() {}
			int load(const std::string& filename, const unsigned long flags = 0);
			int save(const std::string& filename, const unsigned long flags = 0);
		private:
			unsigned long StepStrings(const std::vector<std::string>& str,  long index = 0);
			long ReadBrushFace(const std::vector<std::string>& str, long index,  bool liquit = false);
		};
	}
}