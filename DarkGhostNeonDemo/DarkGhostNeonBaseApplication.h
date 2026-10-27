#pragma once
#include <string>
#include <vector>


namespace ng {
	namespace app {
		class DarkGhostNeonBaseApplication
		{
		public:
			DarkGhostNeonBaseApplication(int argv, char** argc);
			~DarkGhostNeonBaseApplication();
		protected:
			virtual void Event_ParseArguments(const std::string& name, const std::string& value);

		private:
		};
	}
}