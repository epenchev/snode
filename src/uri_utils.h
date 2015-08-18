//
// uri_utils.h
// Copyright (C) 2015  Emil Penchev, Bulgaria

#ifndef URI_UTILS_H_
#define URI_UTILS_H_

#include <string>
#include <map>
#include <vector>

namespace snode
{

struct uri
{

/// Splits a path into its hierarchical components.
/// (uri) The full URI as a string.
/// Returns a std::vector<std:string> containing the segments in the path.
static std::vector<std::string> split_path(const std::string& uri);

/// Splits a query into its key-value components.
/// (uri) The full URI as a string.
/// Returns a std::map<std::string, std::string> containing the key-value components of the query.
static std::map<std::string, std::string> split_query(const std::string& uri);

/// Validates a string as a URI.
/// (uri) The URI string to be validated.
static bool validate(const std::string& uri);

/// Get the host component of the URI as an encoded string.
/// (uri) The full URI as a string.
static std::string get_host(const std::string& uri);

/// Get the port component of the URI. Returns -1 if no port is specified.
/// (uri) The full URI as a string.
static int get_port(const std::string& uri);

/// Get the path component of the URI as an encoded string.
/// (uri) The full URI as a string.
/// Returns empty string if path is missing.
static std::string get_path(const std::string& uri);

/// Get the query component of the URI as an encoded string.
/// (uri) The full URI as a string.
/// Returns The URI query as a string.
static std::string get_query(const std::string& uri);


/// A loopback URI is one which refers to a hostname or ip address with meaning only on the local machine.
/// Examples include "locahost", or ip addresses in the loopback range (127.0.0.0/24).
/// Returns> true if this URI references the local host, false otherwise.
static bool is_host_loopback(const std::string& uri)
{
    return false;
    //return !is_empty() && ((host() == _XPLATSTR("localhost")) || (host().size() > 4 && host().substr(0,4) == _XPLATSTR("127.")));
}

/// A wildcard URI is one which refers to all hostnames that resolve to the local machine (using the * or +)
/// Example is http://*:80
static bool is_host_wildcard(const std::string& uri)
{
    return false;
    //return !is_empty() && (this->host() == _XPLATSTR("*") || this->host() == _XPLATSTR("+"));
}

};

}

#endif /* URI_UTILS_H_ */
