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

/// Configuration parameters for a given network service.
struct net_service_config
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
class snode_config
{
private:

    /// Custom error code for snode_config
    struct error
    {
        std::string message() const { return msg_; }
        error() : is_set_(false) {}
        error(const std::string& msg) : msg_(msg), is_set_(true) {}
        explicit operator bool() const noexcept { return is_set_; }

    private:
        friend class snode_config;

        void set(const std::string& msg)
        {
            msg_ = msg;
            is_set_ = true;
        }

        void clear()
        {
            msg_.clear();
            is_set_ = false;
        }

        std::string msg_;
        bool is_set_;
    };

    void read_streams();
    void read_services();

    snode_config::error err_;
    boost::property_tree::ptree ptree_;
    std::list<media_config> streams_;
    std::list<net_service_config> services_;

public:

    /// Read configuration from file
    void init(const std::string& filename);

    /// Get run as a background service option.
    bool daemonize();

    /// Get threads count.
    unsigned threads();

    /// Get log file pathname
    std::string logfile();

    /// Get the user name if set.
    std::string username();

    /// Get the password if set.
    std::string password();

    /// Media streams configuration.
    const std::list<media_config>& streams();

    /// Get the list of all service configurations.
    const std::list<net_service_config>& services();

    const snode_config::error& error() const { return err_; }
};

}

#endif /* CONFIG_READER_H_ */
