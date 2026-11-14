#pragma once
#include "ConsoleManager.h"
#include <fstream>
#include <iomanip>
#include <sstream>
namespace ng::console {
class ConsoleConfig {
public:
    explicit ConsoleConfig(ConsoleManager& m):manager(m){}
    bool Save(const std::string& filename,std::string* error=nullptr) const {
        std::ofstream f(filename); if(!f){if(error)*error="Could not open config for writing: "+filename;return false;}
        f<<"# NeonGhost console configuration\n# Only ARCHIVE variables are saved.\n";
        for(const auto& n:manager.GetNameList()){auto* e=manager.GetEntry(n);if(e&&e->GetEntryType()==EntryType::VARIABLE&&HasFlag(e->GetFlags(),EntryFlags::ARCHIVE)) f<<std::quoted(n)<<' '<<std::quoted(e->GetValue())<<'\n';}
        if(!f){if(error)*error="Error writing config: "+filename;return false;} return true;
    }
    bool Load(const std::string& filename,std::string* error=nullptr) const {
        std::ifstream f(filename);if(!f){if(error)*error="Could not open config for reading: "+filename;return false;}
        std::string line;std::size_t number=0;
        while(std::getline(f,line)){++number;auto p=line.find_first_not_of(" \t\r\n");if(p==std::string::npos||line[p]=='#')continue;
            std::istringstream s(line);std::string n,v,extra;if(!(s>>std::quoted(n)>>std::quoted(v))||(s>>extra)){if(error)*error="Invalid config syntax at line "+std::to_string(number);return false;}
            auto* e=manager.GetEntry(n);if(!e||e->GetEntryType()!=EntryType::VARIABLE||!HasFlag(e->GetFlags(),EntryFlags::ARCHIVE))continue;
            std::string why;if(!e->SetValue(v,why)){if(error)*error="Config line "+std::to_string(number)+": "+why;return false;}
        } return true;
    }
private: ConsoleManager& manager;
};
}
