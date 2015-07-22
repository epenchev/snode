//
// http_helpers.h
// Copyright (C) 2015  Emil Penchev, Bulgaria

#ifndef HTTP_HELPERS_H_
#define HTTP_HELPERS_H_

#include <string>

namespace snode
{
namespace http
{
    /// Constants for MIME types.
    class mime_types
    {
    public:
#define _MIME_TYPES
#define DAT(a,b) const static std::string a;
#include "http_constants.dat"
#undef _MIME_TYPES
#undef DAT
    };

    /// Constants for charset types.
    class charset_types
    {
    public:
#define _CHARSET_TYPES
#define DAT(a,b) const static std::string a;
#include "http_constants.dat"
#undef _CHARSET_TYPES
#undef DAT
    };

    /// Determines whether or not the given content type is 'textual' according the feature specifications.
    bool is_content_type_textual(const std::string& content_type);

    /// Determines whether or not the given content type is JSON according the feature specifications.
    bool is_content_type_json(const std::string&content_type);

    /// Parses the given Content-Type header value to get out actual content type and charset.
    /// If the charset isn't specified the default charset for the content type will be set.
    void parse_content_type_and_charset(const std::string& content_type, std::string& content, std::string& charset);

    /// Gets the default charset for given content type. If the MIME type is not textual or recognized Latin1 will be returned.
    std::string get_default_charset(const std::string& content_type);

    // simple helper functions to trim whitespace.
    void ltrim_whitespace(std::string& str);
    void rtrim_whitespace(std::string& str);
    void trim_whitespace(std::string& str);

    bool validate_method(const std::string& method);


    namespace chunked_encoding
    {
        // Transfer-Encoding: chunked support
        static const size_t additional_encoding_space = 12;
        static const size_t data_offset               = additional_encoding_space-2;

        // Add the data necessary for properly sending data with transfer-encoding: chunked.
        //
        // There are up to 12 additional bytes needed for each chunk:
        //
        // The last chunk requires 5 bytes, and is fixed.
        // All other chunks require up to 8 bytes for the length, and four for the two CRLF
        // delimiters.
        //
        size_t add_chunked_delimiters(uint8_t* data, size_t buffer_size, size_t bytes_read);
    }
}}

#endif
