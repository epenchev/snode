//
// media_source.h
// Copyright (C) 2015  Emil Penchev, Bulgaria


#ifndef MEDIA_SOURCE_H_
#define MEDIA_SOURCE_H_

#include <string>

namespace snode
{
namespace media
{

template<typename CharType>
class media_source
{
public:
    typedef CharType char_type;
    typedef std::char_traits<CharType> traits;
    typedef typename traits::int_type int_type;
    typedef typename traits::pos_type pos_type;
    typedef typename traits::off_type off_type;

    /// General interface for reading
    size_t read(CharType* ptr, size_t count, off_type offset)
    {
        return func_(this, ptr, count, offset);
    }

    /// Factory method.
    /// objects from this class will not be created directly but from a reg_factory<> instance.
    static media_source<CharType>* create_object() { return NULL; }
protected:

    typedef size_t (*read_func)(media_source<CharType>* base, CharType* ptr, size_t count, off_type offset);
    media_source(read_func func) : func_(func)
    {}

    read_func func_;
    //snode::readbuffer<CharType>* buf_;

};

template<typename Impl>
class media_source_impl : public media_source<Impl::char_type>
{
public:
    void read(media_source<Impl::char_type>* base, Impl::char_type* ptr, size_t count, off_type offset)
    {
        media_source_impl<Impl>* source(static_cast<media_source_impl<Impl>*>(base));
        source->impl_.read(ptr, count, offset);
    }

    media_source_impl(Impl& impl) : media_source_impl(&media_source_impl::read), impl_(impl)
    {}
private:
    Impl& impl_;
};

} // end namespace media
} // end namespace snode

#endif /* SRC_MEDIA_MEDIA_SOURCE_H_ */
