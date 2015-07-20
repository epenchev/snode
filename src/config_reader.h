//
// config_reader.h
// Copyright (C) 2015  Emil Penchev, Bulgaria

#ifndef CONFIG_READER_H_
#define CONFIG_READER_H_

#include <map>
#include <string>
#include <list>
#include <boost/property_tree/ptree.hpp>

namespace snode
{

typedef std::map<std::string, std::string> options_map_t;
///  General error class used for throwing exceptions from xml_reader.
class config_reader_error : public std::runtime_error
{
public:
    ///  Construct error
    config_reader_error(const std::string& what_arg) : runtime_error(what_arg) {}
    ~config_reader_error() throw() {}
};

/// Configuration parameters for a given streaming/network service.
struct service_config
{
    std::string     name;
    std::string     host;
    unsigned        listen_port;
    options_map_t   options;
};

/// Media stream configuration
struct media_config
{
    std::string     name;
    std::string     location;
    options_map_t   options;
};

/// Main application configuration.
class app_config
{
private:
    boost::property_tree::ptree ptree_;
    std::list<media_config> streams_;
    std::list<service_config> servers_;
public:

    /// Read configuration from file
    void init(const std::string& filename);

    /// Get run as a background service option.
    bool daemonize();

    /// Get I/O event threads count.
    unsigned io_threads();

    /// Get processing threads count.
    unsigned process_threads();

    /// Get log file pathname
    std::string logfile();

    /// Get pid file if set (only Unix)
    std::string pidfile();

    /// Get the user name if set.
    std::string username();

    /// Get the password if set.
    std::string password();

    /// Media streams configuration.
    std::list<media_config>& streams();

    /// Get the list of all service configurations.
    std::list<service_config>& servers();
};

}

#endif /* CONFIG_READER_H_ */
