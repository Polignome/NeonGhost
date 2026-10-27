#include <Global.h>
#include "DarkGhostNeonBaseApplication.h"


using namespace ng::app;

std::vector<std::string> GetArguments(int argc, char** argv)
{
    std::vector<std::string> arguments;
    arguments.reserve(argc);

    for (int i = 0; i < argc; ++i)
    {
        arguments.emplace_back(argv[i]);
    }

    return arguments;
}



DarkGhostNeonBaseApplication::DarkGhostNeonBaseApplication(int argc, char** argv) {
    std::vector<std::string> args = GetArguments(argc, argv);
    size_t i = 1;
    while (i < args.size()) {
        if (args[i][0] == '-') {
            if (i + 1 < args.size()) {
                if (args[i + 1][0] != '-') {
                    Event_ParseArguments(args[i], args[i + 1]);
                    i++;
                }
                else Event_ParseArguments(args[i], "");
            }
            else Event_ParseArguments(args[i], "");
        }
        i++;
    }
}

DarkGhostNeonBaseApplication::~DarkGhostNeonBaseApplication() {

}
void DarkGhostNeonBaseApplication::Event_ParseArguments(const std::string& name, const std::string& value)
{
    #ifdef NG_VERBOSE_MODE
    printf("[Parse Arg] name=\'%s\' value=\'%s\'\n", name.c_str(), value.c_str());
    #endif




}