//
// config_reader.h
// Copyright (C) 2015  Emil Penchev, Bulgaria

#ifndef CONFIG_READER_H_
#define CONFIG_READER_H_

#include <map>
#include <string>
#include <list>
#include <boost/property_tree/ptree.hpp>

namespace smkit
{

typedef std::map<std::string,std::string> options_map_t;
///  General error class used for throwing exceptions from xml_reader.
class config_reader_error : public std::runtime_error
{
public:
    ///  Construct error
    config_reader_error(const std::string& what_arg) : runtime_error(what_arg) {}
    ~config_reader_error() throw() {}
};

struct server_handler_config
{
    std::string name;
    std::string host;
    unsigned    listen_port;
    options_map_t options;
};

struct stream_config
{
    bool is_live;
    std::string name;
    std::string location;
    std::string source_classname;
    std::string filter_classname;
    options_map_t options;
};

class smkit_config
{
private:
    boost::property_tree::ptree m_ptree;
    std::list<stream_config> m_streams;
    std::list<server_handler_config> m_servers;
public:

    /// Read configuration from file
    void        init(const std::string& filename);

    bool        daemonize();
    unsigned    io_threads();
    unsigned    max_process_threads();
    std::string logfile();
    std::string pidfile();
    std::string admin_username();
    std::string admin_password();
    std::list<stream_config>& streams();
    std::list<server_handler_config>& server_handlers();
};

}

#endif /* CONFIG_READER_H_ */
