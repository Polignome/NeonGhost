#pragma once
#include "ConsoleConfig.h"
#include <string>
#include <vector>
namespace ng::console {
class Console {
public:
    explicit Console(ConsoleManager& m):manager(m),config(m){}
    std::string Execute(const std::string& command) {
        auto t=Tokenize(command);if(t.empty())return {};const auto& name=t[0];
        if(name=="help")return "Commands: help, list, history, find <prefix>, writeconfig <file>, exec <file>. Variables: name [value]. Functions: name <args...>.";
        if(name=="list"){std::string out;for(const auto& n:manager.GetNameList(false)){auto* e=manager.GetEntry(n);if(!e)continue;out+=n+(e->GetEntryType()==EntryType::VARIABLE?" = ":" <function> ");if(e->GetEntryType()==EntryType::VARIABLE)out+=e->GetValue();if(!e->GetDescription().empty())out+="  # "+e->GetDescription();out+='\n';}return out.empty()?"No entries registered.":out;}
        if(name=="history"){std::string out;for(std::size_t i=0;i<history.size();++i)out+=std::to_string(i)+": "+history[i]+"\n";return out.empty()?"History is empty.":out;}
        if(name=="find"){if(t.size()!=2)return "Usage: find <prefix>";std::string out;for(auto& n:GetCompletions(t[1]))out+=n+'\n';return out.empty()?"No matches.":out;}
        if(name=="writeconfig"){if(t.size()!=2)return "Usage: writeconfig <filename>";std::string e;return config.Save(t[1],&e)?"Configuration saved.":e;}
        if(name=="exec"){if(t.size()!=2)return "Usage: exec <filename>";std::string e;return config.Load(t[1],&e)?"Configuration loaded.":e;}
        auto* e=manager.GetEntry(name);if(!e)return "Unknown command or variable: "+name;
        if(e->GetEntryType()==EntryType::VARIABLE){if(t.size()==1)return name+" = "+e->GetValue();if(t.size()!=2)return "A variable accepts exactly one value. Quote strings containing spaces.";std::string why;if(!e->SetValue(t[1],why))return why;return name+" = "+e->GetValue();}
        std::vector<std::string> args(t.begin()+1,t.end());std::string result;e->Invoke(args,result);return result;
    }
    void Submit(const std::string& command){if(command.empty())return;history.push_back(command);if(history.size()>maxHistory)history.erase(history.begin());PushOutput("> "+command);auto r=Execute(command);if(!r.empty())PushOutput(r);}
    std::vector<std::string> GetCompletions(const std::string& prefix) const {std::vector<std::string> out;for(const auto& n:manager.GetNameList(false))if(n.compare(0,prefix.size(),prefix)==0)out.push_back(n);return out;}
    const std::vector<std::string>& GetOutput()const{return output;} const std::vector<std::string>& GetHistory()const{return history;}
    void ClearOutput(){output.clear();} void ClearHistory(){history.clear();}
    bool SaveConfig(const std::string& f,std::string* e=nullptr)const{return config.Save(f,e);} bool LoadConfig(const std::string& f,std::string* e=nullptr)const{return config.Load(f,e);}
    void PushOutput(std::string s) { output.push_back(std::move(s)); if (output.size() > maxOutput)output.erase(output.begin()); }
private:
    
    ConsoleManager& manager;ConsoleConfig config;std::vector<std::string> history,output;std::size_t maxHistory=128,maxOutput=512;
};
}
