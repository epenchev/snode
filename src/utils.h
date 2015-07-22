//
// utils.h
// Various common utilities.
// Copyright (C) 2015  Emil Penchev, Bulgaria

#ifndef UTILS_H_
#define UTILS_H_

#include <string>
#include <vector>
#include <cstdint>
#include <system_error>
#include <random>
#include <sstream>
#include <locale.h>

#ifndef _WIN32
#include <boost/algorithm/string.hpp>
#if !defined(ANDROID) && !defined(__ANDROID__)
#include <xlocale.h>
#endif
#endif

/// Various utilities for string conversions.
namespace utility
{
namespace conversions
{
    template <typename Source>
    std::string print_string(const Source& val, const std::locale& loc)
    {
        std::ostringstream oss;
        oss.imbue(loc);
        oss << val;
        if (oss.bad())
            throw std::bad_cast();

        return oss.str();
    }

    template <typename Source>
    std::string print_string(const Source& val)
    {
        return print_string(val, std::locale());
    }

    template <typename Target>
    Target scan_string(const std::string& str, const std::locale& loc)
    {
        Target t;
        std::istringstream iss(str);
        iss.imbue(loc);
        iss >> t;
        if (iss.bad())
            throw std::bad_cast();

        return t;
    }

    template <typename Target>
    Target scan_string(const std::string& str)
    {
        return scan_string<Target>(str, std::locale());
    }
}

namespace details
{
    /// Custom implementation of alpha numeric instead of std::isalnum to avoid
    /// taking global lock for performance reasons.
    inline bool is_alnum(char ch)
    {
        return (ch >= '0' && ch <= '9')
            || (ch >= 'A' && ch <= 'Z')
            || (ch >= 'a' && ch <= 'z');
    }

    /// Cross platform utility function for performing case insensitive string comparison.
    /// (left) is first string to compare, and (right) is second string to compare.
    /// Returns true if the strings are equivalent, false otherwise.
    inline bool str_icmp(const std::string& left, const std::string& right)
    {
#ifdef _WIN32
        return _wcsicmp(left.c_str(), right.c_str()) == 0;
#else
        return boost::iequals(left, right);
#endif
    }

    /// Returns the current date in RFC_1123 date time format
    inline std::string current_date_time()
    {
        time_t     now = time(0);
        struct tm  tstruct;
        char       buf[80];
        tstruct = *localtime(&now);

        /* Visit http://www.cplusplus.com/reference/clibrary/ctime/strftime/
        * for more information about date/time format
        * strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);
        */
        strftime(buf, sizeof(buf), "%a, %d %b %g %T GMT", &tstruct);

        return buf;
    }

}

// temporary workaround for the fact that
// utf16char is not fully supported in GCC
class cmp
{
public:

    static int icmp(std::string left, std::string right)
    {
        size_t i;
        for (i = 0; i < left.size(); ++i)
        {
            if (i == right.size()) return 1;

            auto l = cmp::tolower(left[i]);
            auto r = cmp::tolower(right[i]);
            if (l > r) return 1;
            if (l < r) return -1;
        }
        if (i < right.size()) return -1;
        return 0;
    }

private:
    static char tolower(char c)
    {
        if (c >= 'A' && c <= 'Z')
            return static_cast<char>(c - 'A' + 'a');
        return c;
    }
};


} // namespace utility;

#endif // UTILS_H_

