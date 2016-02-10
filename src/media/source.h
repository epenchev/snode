//
// source.h
// Copyright (C) 2016  Emil Penchev, Bulgaria


#ifndef SOURCE_H_
#define SOURCE_H_

#include <string>
#include <memory>

namespace snode
{
namespace media
{

/// General source representation
class source
{
public:
    typedef unsigned char char_type;
    typedef std::char_traits<char_type> traits;
    typedef typename traits::int_type int_type;
    typedef typename traits::pos_type pos_type;
    typedef typename traits::off_type off_type;
    typedef streams::async_streambuf<char_type, streams::sourcebuf<media::source> > streambuf_type;
    typedef streambuf_type::istream_type stream_type;
    typedef streams::async_streambuf<char_type, streams::producer_consumer_buffer<char_type> > live_streambuf_type;
    typedef streams::async_istream<char_type, streams::producer_consumer_buffer<char_type> > live_stream_type;
    typedef std::shared_ptr<streambuf_type> streambuf_type_ptr;

    /// Reads up to (count) characters into (ptr) and returns the count of characters copied or 0 if the end of the source is reached.
    /// The return value (actual characters copied) could be <= count.
    /// If offset is set to value greater than -1 then source sets read position to this value and all other reads start from that offset.
    size_t read(char_type* ptr, size_t count, off_type offset = -1)
    {
        return readfunc_(this, ptr, count, offset);
    }

    /// Get the size (count characters) of the source
    size_t size() const
    {
        return sizefunc_(this);
    }

    /// Closes the underlying stream buffer preventing further read operations.
    void close()
    {
        closefunc_(this);
    }

    /// Object of type async_istream to access the source data.
    /// For live data source live_istream() must be used instead.
    stream_type& stream()
    {
        if (!stream_.is_valid())
        {
            if (!streambuf_)
                streambuf_ = std::make_shared<streams::sourcebuf<media::source> >(this);

            if (streambuf_->can_read())
                stream_ = streambuf_->create_istream();
        }
        return stream_;
    }

    /// Object of type async_istream to access live data stream.
    /// For static data source stream() must be used instead.
    live_stream_type& live_stream()
    {
        if (!live_stream_.is_valid())
        {
            if (live_streambuf().can_read())
                live_stream_ = live_streambuf().create_istream();
        }
        return live_stream_;
    }

    /// Get source specific implementation
    template<typename TImpl>
    inline TImpl& get_impl()
    {
        return static_cast<source_impl<TImpl>*>(this)->impl();
    }

    /// Factory method.
    /// objects from this class will not be created directly but from a reg_factory<> instance.
    static media::source* create_object() { return NULL; }
protected:

    typedef size_t (*size_func) (media::source* base);
    typedef void (*close_func)(media::source* base);
    typedef live_streambuf_type& (*streambuf_func)(media::source* base);
    typedef size_t (*read_func)(media::source* base, char_type* ptr, size_t count, off_type offset);

    source(size_func sizefunc, close_func closefunc, read_func readfunc, streambuf_func streambuffunc)
        : sizefunc_(sizefunc),
          closefunc_(closefunc),
          readfunc_(readfunc),
          streambuf_func_(streambuffunc),
          streambuf_(std::nullptr_t)
    {}

    live_streambuf_type& live_streambuf()
    {
        return streambuf_func_(this);
    }

    /// function bindings with implementation
    size_func  sizefunc_;
    close_func closefunc_;
    read_func readfunc_;
    streambuf_func streambuf_func_;

    // static data
    streambuf_type_ptr streambuf_;
    // streams
    stream_type stream_;
    live_stream_type live_stream_;
};

/// Template based implementation bridge for custom media_source implementations.
/// TImpl template is the actual source implementation.
/// A custom implementation must implement read(), close() and streambuf() method and an factory class that complies with reg_factory.
template<typename TImpl>
class source_impl : public source
{
public:

    /// Bridge for source::size()
    static size_t size(media::source* base)
    {
        source_impl<TImpl>* src(static_cast<source_impl<TImpl>*>(base));
        return src->impl_.size();
    }

    /// Bridge for source::read()
    static void read(source* base, char_type* ptr, size_t count, off_type offset)
    {
        source_impl<TImpl>* src(static_cast<source_impl<TImpl>*>(base));
        src->impl_.read(ptr, count, offset);
    }

    /// Bridge for source::close()
    static void close(media::source* base)
    {
        source_impl<TImpl>* source(static_cast<source_impl<TImpl>*>(base));
        src->impl_.close();
    }

    /// Bridge for source::streambuf()
    static source::live_streambuf_type& streambuf(media::source* base)
    {
        source_impl<TImpl>* source(static_cast<source_impl<TImpl>*>(base));
        return src->impl_.streambuf();
    }

    source_impl(TImpl& impl) : media::source(&source_impl::size,
                                             &source_impl::close,
                                             &source_impl::read,
                                             &source_impl::streambuf), impl_(impl)
    {}

    /// return the actual source implementation
    TImpl& impl() { return impl_; }
private:
    TImpl& impl_;
};

} // end namespace media
} // end namespace snode

#endif /* MEDIA_MEDIA_SOURCE_H_ */
