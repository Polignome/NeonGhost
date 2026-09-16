#pragma once
#include "Parser.h"




namespace ng {
	namespace map {
		class HalfLife :
			public  Parser
		{
		public:
			HalfLife() {}

			virtual ~HalfLife() {}
			int load(const std::string& filename, const unsigned long flags = 0);
			int save(const std::string& filename, const unsigned long flags = 0);
		private:
			unsigned long StepStrings(const std::vector<std::string>& str, unsigned long index = 0);
		};
	}
}