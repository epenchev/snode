//
// reg_factory.h
// Copyright (C) 2014  Emil Penchev, Bulgaria

#ifndef REG_FACTORY_TMPL_H_
#define REG_FACTORY_TMPL_H_

#include <map>
#include <set>
#include <string>

namespace smkit
{

/// Abstract factory from template, register classes at runtime.
template <typename T>
class reg_factory
{
public:
    typedef T* (*create_func)();
    
    static void registrate(const std::string& name, reg_factory::create_func func)
    {
        if ( get_registry().end() == get_registry().find(name) )
        {
            get_registry()[name] = func;
        }
    }
    
    static T* create_instance(const std::string& name)
    {
        typename std::map<std::string, reg_factory::create_func>::iterator it = get_registry().find(name);
        return it == get_registry().end() ? NULL : (it->second)();
    }

    static void get_reg_list(std::set<std::string>& outlist)
    {
        outlist.clear();
        typename std::map<std::string, reg_factory::create_func>::iterator it;
        for (it = get_registry().begin(); it != get_registry().end(); it++)
        {
            outlist.insert(it->first);
        }
    }

    template <typename D>
    struct registrator
    {
        registrator(const std::string& name)
        {
            reg_factory::registrate(name, D::create_object);
        }
    private: // disable copy
        registrator(const registrator&);
        void operator=(const registrator&);
    };

protected:    
    static std::map<std::string, reg_factory::create_func>& get_registry()
    {
        static std::map<std::string, reg_factory::create_func> s_registry;
        return s_registry;
    }
};

template <typename Listener, typename Reg>
struct listener_registrator
{
    listener_registrator()
    {
		Reg::registrate(Listener::create_listener);
    }
private: // disable copy
    listener_registrator(const listener_registrator&);
    void operator=(const listener_registrator&);
};
} // end of smkit
#endif // REG_FACTORY_TMPL_H_


