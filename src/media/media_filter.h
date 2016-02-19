// media_filter.h
// Copyright (C) 2016  Emil Penchev, Bulgaria

#ifndef MEDIA_FILTER_H_
#define MEDIA_FILTER_H_

#include <string>
#include "async_streams.h"

namespace snode
{
namespace media
{

/// General filter representation
class media_filter
{
    typedef unsigned char char_type;
    typedef std::char_traits<char_type> traits;
    typedef typename traits::pos_type pos_type;
    typedef typename traits::off_type off_type;
    typedef streams::async_streambuf<char_type, streams::producer_consumer_buffer<char_type> > streambuf_type;
    typedef streambuf_type::istream_type istream_type;
    typedef streambuf_type::istream_type ostream_type;

    /// Configure the filter with a specific option
    void set_option(const std::string& option)
    {
        optfunc_(this, option);
    }

    /// Set the input stream to be processed.
    void ostream(ostream_type& ostream)
    {
        ostreamfunc_(this, ostream);
    }

    /// Get the output stream with the processed data.
    istream_type istream()
    {
        return istreamfunc_(this);
    }

    /// Get filter specific implementation
    template<typename TImpl>
    inline TImpl& get_impl()
    {
        return static_cast<filter_impl<TImpl>*>(this)->impl();
    }

    /// Factory method.
    /// objects from this class will not be created directly but from a reg_factory<> instance.
    static media_filter* create_object() { return NULL; }

protected:
    streambuf_type streambuf_;    // filter's internal buffer

    typedef void (*option_func) (media_filter* base, const std::string& option);
    typedef void (*ostream_func) (media_filter* base, ostream_type& ostream);
    typedef istream_type (*istream_func) (media_filter* base);

    media_filter(option_func optfunc, ostream_func ostrfunc, istream_func istrfunc) :
        optfunc_(optfunc),
        ostreamfunc_(ostrfunc),
        istreamfunc_(istrfunc)
    {}

    virtual ~media_filter()
    {}

    option_func optfunc_;
    ostream_func ostreamfunc_;
    istream_func istreamfunc_;
};

/// Template based implementation bridge for custom media_filter implementations.
/// TImpl template is the actual filter implementation.
/// A custom implementation must implement read(), close() and streambuf() method and an factory class that complies with reg_factory.
template<typename TImpl>
class filter_impl : public media_filter
{
public:
    filter_impl(TImpl& impl) : media_filter(&filter_impl::set_option,
                                            &filter_impl::ostream,
                                            &filter_impl::istream), impl_(impl)
    {}

    /// Bridge for media_filter::set_option()
    static void set_option(media_filter* base, const std::string& option)
    {
        filter_impl<TImpl>* filter(static_cast<source_impl<TImpl>*>(base));
        filter->impl_.set_option(option);
    }

    /// Bridge for media_filter::ostream()
    static void ostream(media_filter* base, ostream_type& ostream)
    {
        filter_impl<TImpl>* filter(static_cast<source_impl<TImpl>*>(base));
        filter->impl_.set_option(ostream);
    }

    /// Bridge for media_filter::istream()
    static istream_type istream(media_filter* base)
    {
        filter_impl<TImpl>* filter(static_cast<source_impl<TImpl>*>(base));
        return filter->impl_.istream();
    }

    /// return the actual filter implementation
    TImpl& impl() { return impl_; }

private:
    TImpl& impl_;

};

} // end namespace media
} // end namespace snode

#endif /* MEDIA_FILTER_H_ */
