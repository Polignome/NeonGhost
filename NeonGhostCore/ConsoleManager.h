#pragma once
#include "ConsoleTypes.h"
#include <functional>
#include <initializer_list>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <tuple>
#include <typeindex>
#include <type_traits>
#include <utility>
#include <vector>

namespace ng::console {
enum class EntryType { VARIABLE, FUNCTION };
enum class EntryFlags : unsigned { NONE=0, ARCHIVE=1u<<0, READONLY=1u<<1, HIDDEN=1u<<2, CHEAT=1u<<3 };
inline EntryFlags operator|(EntryFlags a, EntryFlags b) { return EntryFlags(unsigned(a)|unsigned(b)); }
inline EntryFlags operator&(EntryFlags a, EntryFlags b) { return EntryFlags(unsigned(a)&unsigned(b)); }
inline bool HasFlag(EntryFlags a, EntryFlags b) { return (a&b)!=EntryFlags::NONE; }

class IConsoleEntry {
public:
    virtual ~IConsoleEntry()=default;
    virtual EntryType GetEntryType() const=0;
    virtual std::type_index GetTypeID() const=0;
    virtual std::string GetValue() const=0;
    virtual bool SetValue(const std::string&, std::string&)=0;
    virtual bool Invoke(const std::vector<std::string>&, std::string&)=0;
    const std::string& GetName() const { return m_name; }
    const std::string& GetDescription() const { return m_description; }
    EntryFlags GetFlags() const { return m_flags; }
protected:
    IConsoleEntry(std::string n,std::string d,EntryFlags f):m_name(std::move(n)),m_description(std::move(d)),m_flags(f){}
private: std::string m_name,m_description; EntryFlags m_flags;
};

template<class T> class VariableContainer final : public IConsoleEntry {
public:
    using Callback=std::function<void(const T&,const T&)>;
    VariableContainer(std::string n,T& v,std::string d,EntryFlags f,Callback cb={}):IConsoleEntry(std::move(n),std::move(d),f),m_value(&v),m_callback(std::move(cb)){}
    EntryType GetEntryType() const override { return EntryType::VARIABLE; }
    std::type_index GetTypeID() const override { return typeid(T); }
    std::string GetValue() const override { return ToString(*m_value); }
    bool SetValue(const std::string& text,std::string& error) override {
        if (HasFlag(GetFlags(),EntryFlags::READONLY)) { error="Variable is read-only: "+GetName(); return false; }
        T parsed{}; if (!ParseValue(text,parsed)) { error="Invalid value for '"+GetName()+"': "+text; return false; }
        Set(parsed); return true;
    }
    bool Invoke(const std::vector<std::string>&,std::string& result) override { result="Entry is a variable, not a function: "+GetName(); return false; }
    T& Get() { return *m_value; } const T& Get() const { return *m_value; }
    void Set(const T& v) { T old=*m_value; *m_value=v; if(m_callback) m_callback(old,*m_value); }
private: T* m_value; Callback m_callback;
};

namespace detail {
template<class Tuple,std::size_t... I> bool ParseTuple(const std::vector<std::string>& args,Tuple& values,std::index_sequence<I...>) {
    bool ok=true; (void)std::initializer_list<int>{(ok=ok && ParseValue(args[I],*std::get<I>(values)),0)...}; return ok;
}
template<class F,class Tuple,std::size_t... I> decltype(auto) Apply(F&& f,Tuple& values,std::index_sequence<I...>) {
    return std::forward<F>(f)(std::move(*std::get<I>(values))...);
}
}

template<class Signature> class FunctionContainer;
template<class R,class... Args> class FunctionContainer<R(Args...)> final : public IConsoleEntry {
    static_assert(((!std::is_lvalue_reference_v<Args> || std::is_const_v<std::remove_reference_t<Args>>) && ...),"Console arguments cannot be non-const lvalue references.");
public:
    using Function=std::function<R(Args...)>;
    FunctionContainer(std::string n,Function f,std::string d,EntryFlags flags):IConsoleEntry(std::move(n),std::move(d),flags),m_function(std::move(f)){}
    EntryType GetEntryType() const override { return EntryType::FUNCTION; }
    std::type_index GetTypeID() const override { return typeid(R(Args...)); }
    std::string GetValue() const override { return "<function>"; }
    bool SetValue(const std::string&,std::string& error) override { error="Cannot assign to function: "+GetName(); return false; }
    bool Invoke(const std::vector<std::string>& args,std::string& result) override {
        if(args.size()!=sizeof...(Args)) { result="Wrong argument count for '"+GetName()+"'. Expected "+std::to_string(sizeof...(Args))+", got "+std::to_string(args.size())+"."; return false; }
        using Values=std::tuple<std::optional<std::decay_t<Args>>...>; Values values;
        if(!detail::ParseTuple(args,values,std::index_sequence_for<Args...>{})) { result="Could not parse arguments for '"+GetName()+"'."; return false; }
        try {
            if constexpr(std::is_void_v<R>) { detail::Apply(m_function,values,std::index_sequence_for<Args...>{}); result="Function executed."; }
            else { auto value=detail::Apply(m_function,values,std::index_sequence_for<Args...>{}); result=ToString(value); }
            return true;
        } catch(const std::exception& e) { result=std::string("Function error: ")+e.what(); return false; }
          catch(...) { result="Function error: unknown exception."; return false; }
    }
    Function& Get(){return m_function;} const Function& Get() const{return m_function;}
private: Function m_function;
};

class ConsoleManager {
public:
    template<class T> bool RegisterVariable(const std::string& name,T& value,const std::string& description={},EntryFlags flags=EntryFlags::NONE,typename VariableContainer<T>::Callback callback={}) {
        if(Exists(name)) return false; m_entries.emplace(name,std::make_unique<VariableContainer<T>>(name,value,description,flags,std::move(callback))); return true;
    }
    template<class R,class... A> bool RegisterFunction(const std::string& name,R(*fn)(A...),const std::string& description={},EntryFlags flags=EntryFlags::NONE) {
        return RegisterFunction<R(A...)>(name,std::function<R(A...)>(fn),description,flags);
    }
    template<class Signature,class Callable> bool RegisterFunction(const std::string& name,Callable&& callable,const std::string& description={},EntryFlags flags=EntryFlags::NONE) {
        if(Exists(name)) return false; using C=FunctionContainer<Signature>; typename C::Function fn(std::forward<Callable>(callable));
        m_entries.emplace(name,std::make_unique<C>(name,std::move(fn),description,flags)); return true;
    }
    template<class T> T* GetVariable(const std::string& name) {
        auto* e=GetEntry(name); if(!e||e->GetEntryType()!=EntryType::VARIABLE||e->GetTypeID()!=typeid(T)) return nullptr;
        return &static_cast<VariableContainer<T>*>(e)->Get();
    }
    template<class S> typename FunctionContainer<S>::Function* GetFunction(const std::string& name) {
        auto* e=GetEntry(name); if(!e||e->GetEntryType()!=EntryType::FUNCTION||e->GetTypeID()!=typeid(S)) return nullptr;
        return &static_cast<FunctionContainer<S>*>(e)->Get();
    }
    template<class S,class... A> auto Call(const std::string& name,A&&... args)->decltype(std::declval<typename FunctionContainer<S>::Function&>()(std::forward<A>(args)...)) {
        auto* fn=GetFunction<S>(name); if(!fn) throw std::runtime_error("Function not found or signature mismatch: "+name); return (*fn)(std::forward<A>(args)...);
    }
    IConsoleEntry* GetEntry(const std::string& n){auto i=m_entries.find(n);return i==m_entries.end()?nullptr:i->second.get();}
    const IConsoleEntry* GetEntry(const std::string& n) const {auto i=m_entries.find(n);return i==m_entries.end()?nullptr:i->second.get();}
    bool Exists(const std::string& n) const{return m_entries.find(n)!=m_entries.end();}
    bool Remove(const std::string& n){return m_entries.erase(n)!=0;}
    std::vector<std::string> GetNameList(bool includeHidden=true) const {
        std::vector<std::string> out; for(const auto& [n,e]:m_entries) if(includeHidden||!HasFlag(e->GetFlags(),EntryFlags::HIDDEN)) out.push_back(n); return out;
    }
private: std::map<std::string,std::unique_ptr<IConsoleEntry>> m_entries;
};
}
