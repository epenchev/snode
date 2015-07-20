//
// config_reader.cpp
// Copyright (C) 2015  Emil Penchev, Bulgaria

#include "config_reader.h"
#include <boost/property_tree/xml_parser.hpp>
#include <boost/lexical_cast.hpp>
#include <string>
#include <exception>

namespace snode
{

void app_config::init(const std::string& filepath)
{
    try
    {
        read_xml(filepath, ptree_);
    } catch (std::exception &ex) {
        throw config_reader_error(ex.what());
    }
}

unsigned app_config::io_threads()
{
    // don't consider default value as error
    return ptree_.get("io_threads", 1);
}

unsigned app_config::process_threads()
{
    // don't consider default value as error
    return ptree_.get("max_threads", 1);
}

bool app_config::daemonize()
{
    try
    {
        std::string val = ptree_.get<std::string>("daemon");
        if (!val.empty())
        {
            if (val.compare("yes"))
                return true;
            else
                return false;
        }
    } catch (std::exception& ex) {
        // do nothing
    }

    return false;
}

std::string app_config::logfile()
{
    return ptree_.get("logfile", "");
}

std::string app_config::pidfile()
{
    return ptree_.get("pidfile", "");
}

std::string app_config::username()
{
    return ptree_.get("admin.username", "");
}

std::string app_config::password()
{
    return ptree_.get("admin.password", "");
}

static void get_options(boost::property_tree::ptree& ptree_reader, options_map_t& out_options)
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

std::list<media_config>& app_config::streams()
{
    if (!streams_.empty())
        return streams_;

    try
    {
        boost::property_tree::ptree::iterator iter = ptree_.get_child("streams").begin();
        while (iter != ptree_.get_child("streams").end())
        {
            media_config stream;
            std::string val = iter->second.get<std::string>("live");
            if (!val.empty())
            {
                if (val.compare("yes"))
                {
                    // value is not interesting as long as the option is set
                    stream.options["yes"] = "true";
                }
            }

            stream.location = iter->second.get<std::string>("location");
            std::string class_name = iter->second.get<std::string>("class_source", "");
            if (!class_name.empty())
                stream.options["source"] = class_name;

            class_name = iter->second.get<std::string>("filter_source", "");
            if (!class_name.empty())
                stream.options["filter"] = class_name;

            get_options(iter->second, stream.options);
            streams_.push_back(stream);
            ++iter;
        }
    } catch (std::exception& ex) {
        throw config_reader_error(ex.what());
    }

    return streams_;
}

std::list<service_config>& app_config::servers()
{
    if (!servers_.empty())
        return servers_;

    try
    {
        boost::property_tree::ptree::iterator iter = ptree_.get_child("servers").begin();
        while (iter != ptree_.get_child("servers").end())
        {
            service_config server;
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

            get_options(iter->second, server.options);
            servers_.push_back(server);
            ++iter;
        }
    } catch (std::exception& ex) {
        throw config_reader_error(ex.what());
    }

    return servers_;
}

}
