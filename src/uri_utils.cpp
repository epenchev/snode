//
// uri_utils.h
// Copyright (C) 2015  Emil Penchev, Bulgaria

#include "uri_utils.h"
#include <sstream>

namespace snode
{

std::vector<std::string> uri::split_path(const std::string& uri)
{
    std::vector<std::string> results;
    std::string path;

    std::size_t colon_found = uri.find_first_of(':');
    if (colon_found != std::string::npos)
    {
        // found a colon, the first portion is a scheme
        if (uri[colon_found + 1] == '/' && uri[colon_found + 2] == '/')
        {
            std::size_t end_scheme = (colon_found + 2);
            std::size_t path_begin = uri.find_first_of('/', end_scheme + 1);
            if (path_begin != std::string::npos)
            {
                path.assign(uri, path_begin + 1, uri.length() - (path_begin + 1));
            }
        }
    }

    if (!path.empty())
    {
        std::istringstream iss(path);

        iss.imbue(std::locale::classic());
        std::string str;

        while (std::getline(iss, str, '/'))
        {
            if (!str.empty())
                results.push_back(str);
        }
    }
    return results;
}

bool uri::validate(const std::string& uri)
{
    return true;
}

std::string uri::get_host(const std::string& uri)
{
    return "";
}

int uri::get_port(const std::string& uri)
{
    return 0;
}

std::string uri::get_path(const std::string& uri)
{
    return "";
}

std::map<std::string, std::string> uri::split_query(const std::string& uri)
{
    std::map<std::string, std::string> results;

#if 0
   // Split into key value pairs separated by '&'.
   size_t prev_amp_index = 0;
   while(prev_amp_index != utility::string_t::npos)
   {
       size_t amp_index = query.find_first_of(_XPLATSTR('&'), prev_amp_index);
       if (amp_index == utility::string_t::npos)
           amp_index = query.find_first_of(_XPLATSTR(';'), prev_amp_index);

       utility::string_t key_value_pair = query.substr(
           prev_amp_index,
           amp_index == utility::string_t::npos ? query.size() - prev_amp_index : amp_index - prev_amp_index);
       prev_amp_index = amp_index == utility::string_t::npos ? utility::string_t::npos : amp_index + 1;

       size_t equals_index = key_value_pair.find_first_of(_XPLATSTR('='));
       if(equals_index == utility::string_t::npos)
       {
           continue;
       }
       else if (equals_index == 0)
       {
           utility::string_t value(key_value_pair.begin() + equals_index + 1, key_value_pair.end());
           results[_XPLATSTR("")] = value;
       }
       else
       {
           utility::string_t key(key_value_pair.begin(), key_value_pair.begin() + equals_index);
           utility::string_t value(key_value_pair.begin() + equals_index + 1, key_value_pair.end());
       results[key] = value;
       }
   }
#endif
   return results;
}

std::string uri::get_query(const std::string& uri)
{
    return "";
}

}
