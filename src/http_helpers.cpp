//
// http_helpers.cpp
// Copyright (C) 2015  Emil Penchev, Bulgaria

#include "utils.h"
#include "http_helpers.h"
#include "http_msg.h"

#include <string>
#include <iterator>

namespace snode
{
namespace http
{
    bool is_content_type_one_of(const std::string* first, const std::string* last, const std::string& value)
    {
        while (first != last)
        {
            if(utility::details::str_icmp(*first, value))
            {
                return true;
            }
            ++first;
        }
        return false;
    }

    bool is_content_type_textual(const std::string& content_type)
    {
        static const std::string textual_types[] =
        {
            http::mime_types::message_http,
            http::mime_types::application_json,
            http::mime_types::application_xml,
            http::mime_types::application_atom_xml,
            http::mime_types::application_http,
            http::mime_types::application_x_www_form_urlencoded
        };

        if (content_type.size() >= 4 && utility::details::str_icmp(content_type.substr(0,4), "text"))
        {
            return true;
        }
        return (is_content_type_one_of(std::begin(textual_types), std::end(textual_types), content_type));
    }

    bool is_content_type_json(const std::string& content_type)
    {
        static const std::string json_types[] =
        {
            http::mime_types::application_json,
            http::mime_types::application_xjson,
            http::mime_types::text_json,
            http::mime_types::text_xjson,
            http::mime_types::text_javascript,
            http::mime_types::text_xjavascript,
            http::mime_types::application_javascript,
            http::mime_types::application_xjavascript
        };

        return (is_content_type_one_of(std::begin(json_types), std::end(json_types), content_type));
    }

    void parse_content_type_and_charset(const std::string& content_type, std::string& content, std::string& charset)
    {
        const size_t semi_colon_index = content_type.find_first_of(";");

        // No charset specified.
        if (semi_colon_index == std::string::npos)
        {
            content = content_type;
            trim_whitespace(content);
            charset = get_default_charset(content);
            return;
        }

        // Split into content type and second part which could be charset.
        content = content_type.substr(0, semi_colon_index);
        trim_whitespace(content);
        std::string possible_charset = content_type.substr(semi_colon_index + 1);
        trim_whitespace(possible_charset);
        const size_t equals_index = possible_charset.find_first_of("=");

        // No charset specified.
        if (equals_index == std::string::npos)
        {
            charset = get_default_charset(content);
            return;
        }

        // Split and make sure 'charset'
        std::string charset_key = possible_charset.substr(0, equals_index);
        trim_whitespace(charset_key);
        if (!utility::details::str_icmp(charset_key, "charset"))
        {
            charset = get_default_charset(content);
            return;
        }

        charset = possible_charset.substr(equals_index + 1);

        // Remove the redundant ';' at the end of charset.
        while (charset.back() == ';')
        {
            charset.pop_back();
        }

        trim_whitespace(charset);
        if (charset.front() == '"' && charset.back() == '"')
        {
            charset = charset.substr(1, charset.size() - 2);
            trim_whitespace(charset);
        }
    }

    std::string get_default_charset(const std::string& content_type)
    {
        // We are defaulting everything to Latin1 except JSON which is utf-8.
        if (is_content_type_json(content_type))
        {
            return charset_types::utf8;
        }
        else
        {
            return charset_types::latin1;
        }
    }

    void ltrim_whitespace(std::string& str)
    {
        size_t index;
        for(index = 0; index < str.size() && isspace(str[index]); ++index);
        str.erase(0, index);
    }

    void rtrim_whitespace(std::string& str)
    {
        size_t index;
        for(index = str.size(); index > 0 && isspace(str[index - 1]); --index);
        str.erase(index);
    }

    void trim_whitespace(std::string& str)
    {
        ltrim_whitespace(str);
        rtrim_whitespace(str);
    }

    size_t chunked_encoding::add_chunked_delimiters(uint8_t* data, size_t buffer_size, size_t bytes_read)
    {
        size_t offset = 0;

        if (buffer_size < bytes_read + http::chunked_encoding::additional_encoding_space)
        {
            throw http_exception("Insufficient buffer size.");
        }

        if ( bytes_read == 0 )
        {
            offset = 7;
            data[7] = '0';
            data[8] = '\r';  data[9] = '\n'; // The end of the size.
            data[10] = '\r'; data[11] = '\n'; // The end of the message.
        }
        else
        {
            char buffer[9];
#ifdef _WIN32
            sprintf_s(buffer, sizeof(buffer), "%8IX", bytes_read);
#else
            snprintf(buffer, sizeof(buffer), "%8zX", bytes_read);
#endif
            memcpy(&data[0], buffer, 8);
            while (data[offset] == ' ') ++offset;
            data[8] = '\r'; data[9] = '\n'; // The end of the size.
            data[10+bytes_read] = '\r'; data[11+bytes_read] = '\n'; // The end of the chunk.
        }

        return offset;
    }

#if (!defined(_WIN32) || defined(__cplusplus_winrt))
    const std::array<bool,128> valid_chars =
    {{
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0-15
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //16-31
        0, 1, 0, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 1, 1, 0, //32-47
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, //48-63
        0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //64-79
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, //80-95
        0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //96-111
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0 //112-127
    }};

    // Checks if the method contains any invalid characters
    bool validate_method(const std::string& method)
    {
        for (const auto &ch : method)
        {
            size_t ch_sz = static_cast<size_t>(ch);
            if (ch_sz >= 128)
                return false;

            if (!valid_chars[ch_sz])
                return false;
        }

        return true;
    }
#endif

}} // namespace snode::http
