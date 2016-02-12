// media_filter.h
// Copyright (C) 2016  Emil Penchev, Bulgaria

#ifndef MEDIA_FILTER_H_
#define MEDIA_FILTER_H_

namespace snode
{
namespace media
{

class media_filter
{
    typedef unsigned char char_type;
    typedef streams::async_streambuf<char_type, streams::producer_consumer_buffer<char_type> > streambuf_type;

    void set_option(const std::string& option);

    streambuf_type streambuf()
    {
        return buf_;
    }

protected:
    streambuf_type buf_;

    media_filter()
    {}
    virtual ~media_filter()
    {}
};

} // end namespace media
} // end namespace snode

#endif /* MEDIA_FILTER_H_ */
