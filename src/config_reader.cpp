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
// general
static const char* s_threads_section = "threads";
static const char* s_daemon_section = "daemon";
static const char* s_logfile_section = "logfile";
static const char* s_admin_user_section = "admin.username";
static const char* s_admin_password_section = "admin.password";
static const char* s_options_section = "options";
// streams
static const char* s_streams_section = "streams";
static const char* s_streams_location_section = "location";
static const char* s_streams_live_section = "live";
static const char* s_streams_source_section = "source";
static const char* s_streams_filter_section = "filter";
// services
static const char* s_services_section = "services";
// static const char* s_services_transport_protocol = "protocol";
static const char* s_service_name_section = "name";
static const char* s_service_hostport_section = "listen";
// errors
static const char* s_err_missing_source = "missing source";
static const char* s_err_missing_streams = "No streams defined";

void snode_config::init(const std::string& filepath)
{
    try
    {
        read_xml(filepath, ptree_);
        read_streams();
        read_services();
    }
    catch (std::exception &ex)
    {
        err_.set(ex.what());
    }
}

unsigned snode_config::threads()
{
    // don't consider default value as error
    return ptree_.get(s_threads_section, 1);
}

bool snode_config::daemonize()
{
    try
    {
        unsigned val = ptree_.get(s_daemon_section, 0);
        if (val)
            return true;
        else
            return false;
    }
    catch (std::exception& ex)
    {
        err_.set(ex.what());
    }

    return false;
}

std::string snode_config::logfile()
{
    return ptree_.get(s_logfile_section, "");
}

std::string snode_config::username()
{
    return ptree_.get(s_admin_user_section, "");
}

std::string snode_config::password()
{
    return ptree_.get(s_admin_password_section, "");
}

static void get_options(boost::property_tree::ptree& ptree_reader, options_map_t& out_options)
{
    boost::property_tree::ptree::const_assoc_iterator it_assoc = ptree_reader.find(s_options_section);
    if (it_assoc != ptree_reader.not_found())
    {
        boost::property_tree::ptree::iterator it_opt = ptree_reader.get_child(s_options_section).begin();
        while (it_opt != ptree_reader.get_child(s_options_section).end())
        {
            out_options.insert(std::pair<std::string, std::string>(it_opt->first, boost::lexical_cast<std::string>(it_opt->second.data())));
            ++it_opt;
        }
    }
}

const std::list<media_config>& snode_config::streams()
{
    return streams_;
}

void snode_config::read_streams()
    try
    {
        boost::property_tree::ptree::iterator iter = ptree_.get_child(s_streams_section).begin();
        while (iter != ptree_.get_child(s_streams_section).end())
        {
            media_config stream;
            unsigned val = iter->second.get(s_streams_live_section, 0);
            if (val)
            {
                stream.options[s_streams_live_section] = "1";
            }

            stream.location = iter->second.get<std::string>(s_streams_location_section);
            std::string source_class = iter->second.get<std::string>(s_streams_source_section, "");
            if (!source_class.empty())
                stream.options[s_streams_source_section] = source_class;
            else
                throw std::runtime_error(s_err_missing_source);

            std::string filter_class = iter->second.get<std::string>(s_streams_filter_section, "");
            if (!filter_class.empty())
                stream.options[s_streams_filter_section] = filter_class;

            get_options(iter->second, stream.options);
            streams_.push_back(stream);
            ++iter;
        }

        if (streams_.empty())
            err_.set(s_err_missing_streams);
    }
    catch (std::exception& ex)
    {
        err_.set(ex.what());
}

const std::list<net_service_config>& snode_config::services()
{
    return services_;
}

void snode_config::read_services()
{
    try
    {
        boost::property_tree::ptree::iterator iter = ptree_.get_child(s_services_section).begin();
        while (iter != ptree_.get_child(s_services_section).end())
        {
            net_service_config service;
            service.name = iter->second.get<std::string>(s_service_name_section);
            auto service_hostport = iter->second.get<std::string>(s_service_hostport_section);
            auto pos = service_hostport.find_first_of(":");
            if (pos != std::string::npos)
            {
                service.host = service_hostport.substr(0, pos);
                service.listen_port = boost::lexical_cast<unsigned>(service_hostport.substr(pos + 1));
            }
            else
            {
                service.host = "";
                service.listen_port = iter->second.get<unsigned>(s_service_hostport_section);
            }

            get_options(iter->second, service.options);
            services_.push_back(service);
            ++iter;
        }

    }
    catch (std::exception& ex)
    {
        err_.set(ex.what());
    }
}

}
