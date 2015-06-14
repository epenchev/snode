//
// config_reader.cpp
// Copyright (C) 2015  Emil Penchev, Bulgaria

#include "config_reader.h"
#include <boost/property_tree/xml_parser.hpp>
#include <boost/lexical_cast.hpp>
#include <string>
#include <exception>

namespace smkit
{

void smkit_config::init(const std::string& filepath)
{
    try
    {
        read_xml(filepath, m_ptree);
    }
    catch (std::exception &ex)
    {
        throw config_reader_error(ex.what());
    }
}

unsigned smkit_config::io_threads()
{
    // don't consider default value as error
    return m_ptree.get("io_threads", 1);
}

unsigned smkit_config::max_process_threads()
{
    // don't consider default value as error
    return m_ptree.get("max_threads", 1);
}

bool smkit_config::daemonize()
{
    try
    {
        std::string val = m_ptree.get<std::string>("daemon");
        if (!val.empty())
        {
            if (val.compare("yes"))
                return true;
            else
                return false;
        }
    }
    catch (std::exception& ex)
    {
        // do nothing
    }

    return false;
}

std::string smkit_config::logfile()
{
    return m_ptree.get("logfile", "");
}

std::string smkit_config::pidfile()
{
    return m_ptree.get("pidfile", "");
}

std::string smkit_config::admin_username()
{
    return m_ptree.get("admin.username", "");
}

std::string smkit_config::admin_password()
{
    return m_ptree.get("admin.password", "");
}

static void _get_options(boost::property_tree::ptree& ptree_reader, options_map_t& out_options)
{
    boost::property_tree::ptree::const_assoc_iterator it_assoc = ptree_reader.find("options");
    if (it_assoc != ptree_reader.not_found())
    {
        boost::property_tree::ptree::iterator it_opt = ptree_reader.get_child("options").begin();
        while (it_opt != ptree_reader.get_child("options").end())
        {
            out_options.insert(std::pair<std::string, std::string>(it_opt->first, boost::lexical_cast<std::string>(it_opt->second.data())));
            ++it_opt;
        }
    }
}

std::list<stream_config>& smkit_config::streams()
{
    if (!m_streams.empty())
    {
        return m_streams;
    }

    try
    {
        boost::property_tree::ptree::iterator iter = m_ptree.get_child("streams").begin();
        while (iter != m_ptree.get_child("streams").end())
        {
            stream_config stream;
            std::string val = iter->second.get<std::string>("live");
            if (!val.empty())
            {
                if (val.compare("yes"))
                    stream.is_live = true;
                else
                    stream.is_live = false;
            }
            else
            {
                stream.is_live = false;
            }

            stream.location = iter->second.get<std::string>("location");
            stream.source_classname = iter->second.get<std::string>("class_source", "");
            stream.filter_classname = iter->second.get<std::string>("filter_source", "");
            _get_options(iter->second, stream.options);
            m_streams.push_back(stream);
            ++iter;
        }
    }
    catch (std::exception& ex)
    {
        throw config_reader_error(ex.what());
    }

    return m_streams;
}

std::list<server_handler_config>& smkit_config::server_handlers()
{
    if (!m_servers.empty())
    {
        return m_servers;
    }

    // load list
    try
    {
        boost::property_tree::ptree::iterator iter = m_ptree.get_child("servers").begin();
        while (iter != m_ptree.get_child("servers").end())
        {
            server_handler_config server;
            server.name = iter->second.get<std::string>("name");
            auto listen = iter->second.get<std::string>("listen");
            auto pos = listen.find_first_of(":");
            if (pos != std::string::npos)
            {
                server.host = listen.substr(0, pos);
                server.listen_port = boost::lexical_cast<unsigned>(listen.substr(pos + 1));
            }
            else
            {
                server.host = "";
                server.listen_port = iter->second.get<unsigned>("listen");
            }

            _get_options(iter->second, server.options);
            m_servers.push_back(server);
            ++iter;
        }
    }
    catch (std::exception& ex)
    {
        throw config_reader_error(ex.what());
    }

    return m_servers;
}

}
