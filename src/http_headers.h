//
// http_headers.h
// Copyright (C) 2015  Emil Penchev, Bulgaria

#ifndef HTTP_HEADERS_H_
#define HTTP_HEADERS_H_

#include <map>
#include <memory>
#include <utility>
#include <cstring>
#include <string>
#include <vector>
#include <sstream>
#include <typeinfo>

#include "utils.h"

namespace snode
{
namespace http
{

/// Represents HTTP headers, acts as a map.
class http_headers
{
public:

    /// Function object to perform case insensitive comparison of wstrings.
    struct case_insensitive_cmp_
    {
        bool operator()(const std::string& str1, const std::string& str2) const
        {
#ifdef _WIN32
            return _wcsicmp(str1.c_str(), str2.c_str()) < 0;
#else
            return utility::cmp::icmp(str1, str2) < 0;
#endif
        }
    };

    typedef std::map<std::string, std::string, case_insensitive_cmp_> headers_map;
    typedef std::map<std::string, std::string, case_insensitive_cmp_>::key_type key_type;
    typedef std::map<std::string, std::string, case_insensitive_cmp_>::key_compare key_compare;
    typedef std::map<std::string, std::string, case_insensitive_cmp_>::size_type size_type;
    typedef std::map<std::string, std::string, case_insensitive_cmp_>::iterator iterator;
    typedef std::map<std::string, std::string, case_insensitive_cmp_>::const_iterator const_iterator;

    /// Constructs an empty set of HTTP headers.
    http_headers() {}

    /// Copy constructor.
    http_headers(const http_headers &other) : headers_(other.headers_) {}

    /// Assignment operator.
    http_headers &operator=(const http_headers &other)
    {
        if (this != &other)
            headers_ = other.headers_;
        return *this;
    }

    /// Move constructor.
    http_headers(http_headers &&other) : headers_(std::move(other.headers_)) {}

    /// Move assignment operator.
    http_headers &operator=(http_headers &&other)
    {
        if(this != &other)
            headers_ = std::move(other.headers_);
        return *this;
    }


    /// Adds a header field with name (name) and value of the header (value).
    /// If the header field exists, the value will be combined as comma separated string.
    template<typename T>
    void add(const key_type& name, const T& value)
    {
        if (has(name))
            headers_[name] =  headers_[name].append(", " + utility::conversions::print_string(value));
        else
            headers_[name] = utility::conversions::print_string(value);
    }

    /// Removes a header field.
    void remove(const key_type& name)
    {
        headers_.erase(name);
    }

    /// Removes all elements from the headers.
    void clear()
    {
        headers_.clear();
    }

    /// Checks if there is a header with the given key.
    /// returns true if there is a header with the given name, false otherwise.
    bool has(const key_type& name) const
    {
        return headers_.find(name) != headers_.end();
    }

    /// Returns the number of header fields.
    size_type size() const
    {
        return headers_.size();
    }

    /// Tests to see if there are any header fields.
    /// returns true if there are no headers, false otherwise.
    bool empty() const
    {
        return headers_.empty();
    }

    /// Returns a reference to header field with given name, if there is no header field one is inserted.
    std::string& operator[](const key_type &name)
    {
        return headers_[name];
    }

    /// Checks if a header field exists with given name and returns an iterator if found. Otherwise
    /// and iterator to end is returned.
    /// returns an iterator to where the HTTP header is found.
    iterator find(const key_type& name)
    {
        return headers_.find(name);
    }

    const_iterator find(const key_type& name) const
    {
        return headers_.find(name);
    }

    /// Attempts to match a header field with the given name using the '>>' operator.
    /// returns true if header field was found and successfully stored in value parameter.
    template<typename T>
    bool match(const key_type& name, T& value) const
    {
        headers_map::const_iterator iter = headers_.find(name);
        if (iter != headers_.end())
        {
            // Check to see if doesn't have a value.
            if (iter->second.empty())
            {
                bind_impl(iter->second, value);
                return true;
            }
            return bind_impl(iter->second, value);
        }
        else
        {
            return false;
        }
    }

    /// Returns an iterator referring to the first header field (beginning of the HTTP headers).
    iterator begin() { return headers_.begin(); }
    const_iterator begin() const { return headers_.begin(); }

    /// Returns an iterator referring to the past-the-end header field.
    iterator end() { return headers_.end(); }
    const_iterator end() const { return headers_.end(); }

    /// Gets the content length of the message.
    std::size_t content_length() const;

    /// Sets the content length of the message.
    void set_content_length(std::size_t length);

    /// Gets the content type of the message, returns the content type of the body.
    std::string content_type() const;

    /// Sets the content type of the message.
    void set_content_type(const std::string& type);

    /// Gets the cache control header of the message, returns the cache control header value.
    std::string cache_control() const;

    /// Sets the cache control header of the message.
    void set_cache_control(const std::string& control);

    /// Gets the date header of the message, returns the date header value.
    std::string date() const;

    /// Sets the date header of the message.
    void set_date();

private:

    template<typename T>
    bool bind_impl(const key_type& text, T& ref) const
    {
        std::istringstream iss(text);
        iss.imbue(std::locale::classic());
        iss >> ref;
        if (iss.fail() || !iss.eof())
            return false;

        return true;
    }

    bool bind_impl(const key_type& text, std::string& ref) const
    {
        ref = text;
        return true;
    }

    // Headers are stored in a map with custom compare func object.
    std::map<std::string, std::string, case_insensitive_cmp_> headers_;
};

}}
#endif // HTTP_HEADERS_H_

