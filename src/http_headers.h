//
// http_headers.h
// Copyright (C) 2015  Emil Penchev, Bulgaria

#ifndef HTTP_HEADERS_H_
#define HTTP_HEADERS_H_

#include <map>
#include <memory>
#include <cstring>
#include <string>
#include <vector>
#include <sstream>
#include <typeinfo>

namespace smkit
{
namespace http
{

struct util_conversions
{
    template <typename Source>
    std::string print_string(const Source& val)
    {
        std::ostringstream oss;
        oss << val;
        if (oss.bad())
            throw std::bad_cast();

        return oss.str();
    }
};

/// Represents HTTP headers, acts as a map.
class http_headers
{
public:
    /// Function object to perform comparison of strings
    struct m_cmp_func
    {
        bool operator()(const std::string &str1, const std::string &str2) const
        {
            return strcmp(str1.c_str(), str2.c_str()) < 0;
        }
    };

    typedef std::map<std::string, std::string, m_cmp_func> headers_map;
    typedef std::map<std::string, std::string, m_cmp_func>::key_type key_type;
    typedef std::map<std::string, std::string, m_cmp_func>::key_compare key_compare;
    typedef std::map<std::string, std::string, m_cmp_func>::size_type size_type;
    typedef std::map<std::string, std::string, m_cmp_func>::iterator iterator;
    typedef std::map<std::string, std::string, m_cmp_func>::const_iterator const_iterator;

    http_headers()
    {
    }

    http_headers(const http_headers &other) : m_headers(other.m_headers)
    {
    }

    http_headers &operator=(const http_headers &other)
    {
        if (this != &other)
            m_headers = other.m_headers;
        return *this;
    }

    template<typename T>
    void add(const key_type& name, const T& value)
    {
        if (has(name))
            m_headers[name] =  m_headers[name].append(", " /*+ util_conversions::print_string(value)*/);
        else
        {
            // m_headers[name] = util_conversions::print_string(value);
        }
    }


    /// Removes a header field.
    void remove(const key_type& name)
    {
        m_headers.erase(name);
    }

    /// Removes all elements from the headers.
    void clear()
    {
        m_headers.clear();
    }

    /// Checks if there is a header with the given key.
    /// returns true if there is a header with the given name, false otherwise.
    bool has(const key_type& name) const
    {
        return m_headers.find(name) != m_headers.end();
    }

    /// Returns the number of header fields.
    size_type size() const
    {
        return m_headers.size();
    }

    /// Tests to see if there are any header fields.
    /// returns true if there are no headers, false otherwise.
    bool empty() const
    {
        return m_headers.empty();
    }

    /// Returns a reference to header field with given name, if there is no header field one is inserted.
    std::string& operator[](const key_type &name)
    {
        return m_headers[name];
    }


    /// Checks if a header field exists with given name and returns an iterator if found. Otherwise
    /// and iterator to end is returned.
    /// returns an iterator to where the HTTP header is found.
    iterator find(const key_type& name)
    {
        return m_headers.find(name);
    }

    const_iterator find(const key_type& name) const
    {
        return m_headers.find(name);
    }

    /// Attempts to match a header field with the given name using the '>>' operator.
    /// returns true if header field was found and successfully stored in value parameter.
    template<typename T>
    bool match(const key_type &name, T &value) const
    {
        headers_map::const_iterator iter = m_headers.find(name);
        if (iter != m_headers.end())
        {
            // Check to see if doesn't have a value.
            if (iter->second.empty())
            {
                /* bind_impl(iter->second, value); */
                return true;
            }
            return true; /* bind_impl(iter->second, value); */
        }
        else
            return false;
    }

    /// <summary>
    /// Returns an iterator referring to the first header field.
    /// </summary>
    /// <returns>An iterator to the beginning of the HTTP headers</returns>
    iterator begin() { return m_headers.begin(); }
    const_iterator begin() const { return m_headers.begin(); }

    /// <summary>
    /// Returns an iterator referring to the past-the-end header field.
    /// </summary>
    /// <returns>An iterator to the element past the end of the HTTP headers.</returns>
    iterator end() { return m_headers.end(); }
    const_iterator end() const { return m_headers.end(); }

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
    //void set_date(const utility::datetime& date);

private:

    template<typename T>
    bool bind_impl(/*const key_type& text*/std::string& text, T& ref) const
    {
        std::istream iss(text);
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
    std::map<std::string, std::string, m_cmp_func> m_headers;
};

}}
#endif // HTTP_HEADERS_H_

