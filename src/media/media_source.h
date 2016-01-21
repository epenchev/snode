//
// media_source.h
// Copyright (C) 2016  Emil Penchev, Bulgaria


#ifndef MEDIA_SOURCE_H_
#define MEDIA_SOURCE_H_

#include <string>

namespace snode
{
namespace media
{

class media_source
{
public:
    typedef unsigned char char_type;
    typedef std::char_traits<char_type> traits;
    typedef typename traits::int_type int_type;
    typedef typename traits::pos_type pos_type;
    typedef typename traits::off_type off_type;
    typedef streams::async_streambuf<char_type, streams::sourcebuf<media_source> > streambuf_type;
    typedef streams::async_istream<char_type, streams::sourcebuf<media_source> > stream_type;
    typedef streams::async_streambuf<char_type, streams::producer_consumer_buffer<char_type> > live_streambuf_type;
    typedef streams::async_istream<char_type, streams::producer_consumer_buffer<char_type> > live_stream_type;

    /// Reads up to (count) characters into (ptr) and returns the count of characters copied.
    /// The return value (actual characters copied) could be <= count.
    /// If offset is set to value greater than -1 then source sets read position to this value and all other reads start from that offset.
    size_t read(char_type* ptr, size_t count, off_type offset = -1)
    {
        return readfunc_(this, ptr, count, offset);
    }

    /// Closes the underlying stream buffer preventing further read operations.
    void close()
    {
        closefunc_(this);
    }

    /// async_stream to access the source data.
    /// If source represents live stream data returned object is NULL,
    /// so consider using live_istream() instead.
    stream_type* stream()
    {
        if (streambuf_.can_read())
            return &stream_;
        else
            return nullptr;
    }

    /// Get not seek-able async_stream if source represents live data stream.
    /// If source is not live data stream the returned stream object is NULL,
    /// so consider using stream() instead.
    live_stream_type* live_stream()
    {
        if (live_streambuf_.can_read())
            return &live_stream_;
        else
            return nullptr;
    }

    /// Get media_source specific implementation
    template<typename Impl>
    inline Impl& get_impl()
    {
        return static_cast<media_source_impl<Impl>*>(this)->impl();
    }

    /// Factory method.
    /// objects from this class will not be created directly but from a reg_factory<> instance.
    static media_source* create_object() { return NULL; }
protected:

    typedef void (*close_func)(media_source* base);
    typedef live_streambuf_type (*streambuf_func)(media_source* base);
    typedef size_t (*read_func)(media_source* base, char_type* ptr, size_t count, off_type offset);

    media_source(close_func closefunc, read_func readfunc, streambuf_func streambuffunc)
        : closefunc_(closefunc),
          readfunc_(readfunc),
          streambuf_func_(streambuffunc)
    {
        // TODO
    }

    /// function bindings with implementation
    close_func closefunc_;
    read_func readfunc_;
    streambuf_func streambuf_func_;

    /// streams and buffers
    streambuf_type streambuf_;
    live_streambuf_type live_streambuf_;
    stream_type stream_;
    live_stream_type live_stream_;

};

/// A Curiously recurring template pattern for creating custom media_source objects.
/// Impl template is the actual source implementation.
/// A custom implementation must implement read(), close() and streambuf() method and an factory class that complies with reg_factory.
template<typename Impl>
class media_source_impl : public media_source
{
public:

    static void read(media_source* base, char_type* ptr, size_t count, off_type offset)
    {
        media_source_impl<Impl>* source(static_cast<media_source_impl<Impl>*>(base));
        source->impl_.read(ptr, count, offset);
    }

    static void close(media_source* base)
    {
        media_source_impl<Impl>* source(static_cast<media_source_impl<Impl>*>(base));
        source->impl_.close();
    }

    static media_source::live_streambuf_type streambuf(media_source* base)
    {
        media_source_impl<Impl>* source(static_cast<media_source_impl<Impl>*>(base));
        return source->impl_.streambuf();
    }

    media_source_impl(Impl& impl) : media_source(&media_source_impl::close, &media_source_impl::read, &media_source_impl::streambuf), impl_(impl)
    {}

    /// return the actual source implementation
    Impl& impl() { return impl_; }
private:
    Impl& impl_;
};

} // end namespace media
} // end namespace snode

#endif /* MEDIA_MEDIA_SOURCE_H_ */
