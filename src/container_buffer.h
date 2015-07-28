//
// container_buffer.h
// Copyright (C) 2015  Emil Penchev, Bulgaria

#ifndef CONTAINER_BUFFER_H_
#define CONTAINER_BUFFER_H_

#include <vector>
#include <queue>
#include <algorithm>
#include <iterator>
#include <assert.h>
#include "async_streams.h"
#include "async_task.h"

namespace snode
{
namespace streams
{

/// The basic_container_buffer class serves as a memory-based steam buffer that supports writing or reading
/// sequences of characters.
/// A class to allow users to create input and out streams based on STL collections.
/// The sole purpose of this class to avoid users from having to know anything about stream buffers.
/// CollectionType - The type of the STL collection (ex. std::string, std::vector ..).
template<typename CollectionType>
class container_buffer : public async_streambuf<typename CollectionType::value_type, container_buffer<CollectionType>>
{
public:
    typedef typename CollectionType::value_type char_type;
    typedef typename CollectionType::value_type CharType;
    typedef async_streambuf<CharType, container_buffer> base_stream_type;
    typedef typename container_buffer<CollectionType>::traits traits;
    typedef typename container_buffer<CollectionType>::int_type int_type;
    typedef typename container_buffer<CollectionType>::pos_type pos_type;
    typedef typename container_buffer<CollectionType>::off_type off_type;

    /// Returns the underlying data container
    CollectionType& collection()
    {
        return data_;
    }


    /// Constructor
    container_buffer(std::ios_base::openmode mode) : base_stream_type(mode), current_position_(0)
    {
        validate_mode(mode);
    }

    /// Constructor
    container_buffer(CollectionType data, std::ios_base::openmode mode)
      : base_stream_type(std::ios_base::out | std::ios_base::in), data_(std::move(data)), current_position_(0)
              //current_position_((mode & std::ios_base::in) ? 0 : data_.size())
    {
        validate_mode(mode);
    }

    /// Destructor
    ~container_buffer()
    {
        // Invoke the synchronous versions since we need to
        // purge the request queue before deleting the buffer
        this->close_read_impl();
        this->close_write_impl();
    }

    /// internal implementation of can_seek() from async_streambuf
    bool can_seek_impl() { return this->is_open(); }

    /// internal implementation of has_size() from async_streambuf
    bool has_size_impl() { return this->is_open(); }

    /// Gets the size of the stream, if known. Calls to has_size() will determine whether
    /// the result of size can be relied on.
    uint64_t size()
    {
        return uint64_t(data_.size());
    }

    /// internal implementation of buffer_size() from async_streambuf
    size_t buffer_size_impl(std::ios_base::openmode = std::ios_base::in) const
    {
        return 0;
    }

    /// Sets the stream buffer implementation to buffer or not buffer.
    /// (size) The size to use for internal buffering, 0 if no buffering should be done.
    /// (direction) The direction of buffering (in or out).
    void set_buffer_size(size_t , std::ios_base::openmode = std::ios_base::in)
    {
        return;
    }

    /// internal implementation of in_avail() from async_streambuf
    size_t in_avail_impl() const
    {
        // See the comment in seek around the restriction that we do not allow read head to
        // seek beyond the current write_end.
        assert(current_position_ <= data_.size());

        size_t readhead(current_position_);
        size_t write_end(data_.size());
        return (size_t)(write_end - readhead);
    }

    /// internal implementation of sync() from async_streambuf
    bool sync_impl()
    {
        return (true);
    }

    /// internal implementation of putc() from async_streambuf
    template<typename WriteHandler>
    void putc_impl(CharType ch, WriteHandler handler)
    {
        int_type res = (this->write(&ch, 1) == 1) ? static_cast<int_type>(ch) : traits::eof();
        async_event_task::connect(handler, res);
    }

    /// internal implementation of putn() from async_streambuf
    template<typename WriteHandler>
    void putn_impl(CharType* ptr, size_t count, WriteHandler handler)
    {
        size_t res = this->write(ptr, count);
        async_event_task::connect(handler, res);
    }

    /// internal implementation of alloc() from async_streambuf
    CharType* alloc_impl(size_t count)
    {
        if (!this->can_write()) return nullptr;

        // Allocate space
        resize_for_write(current_position_+count);

        // Let the caller copy the data
        return (CharType*)&data_[current_position_];
    }

    /// internal implementation of commit() from async_streambuf
    void commit_impl(size_t count)
    {
        // Update the write position and satisfy any pending reads
        update_current_position(current_position_ + count);
    }

    /// internal implementation of acquire() from async_streambuf
    bool acquire_impl(CharType*& ptr, size_t& count)
    {
        ptr = nullptr;
        count = 0;

        if (!this->can_read()) return false;

        count = in_avail_impl();

        if (count > 0)
        {
            ptr = (CharType*)&data_[current_position_];
            return true;
        }
        else
        {
            // Can only be open for read OR write, not both. If there is no data then
            // we have reached the end of the stream so indicate such with true.
            return true;
        }
    }

    /// internal implementation of release() from async_streambuf
    void release_impl(CharType* ptr, size_t count)
    {
        if (ptr != nullptr)
            update_current_position(current_position_ + count);
    }

    /// internal implementation of getn() from async_streambuf
    template<typename ReadHandler>
    void getn_impl(CharType* ptr, size_t count, ReadHandler handler)
    {
        int_type res = this->read(ptr, count);
        async_event_task::connect(handler, res);
    }

    /// internal implementation of sgetn() from async_streambuf
    size_t sgetn_impl(CharType* ptr, size_t count)
    {
        return this->read(ptr, count);
    }

    /// internal implementation of scopy() from async_streambuf
    size_t scopy_impl(CharType* ptr, size_t count)
    {
        return this->read(ptr, count, false);
    }

    /// internal implementation of bumpc() from async_streambuf
    template<typename ReadHandler>
    void bumpc_impl(ReadHandler handler)
    {
        int_type res = this->read_byte(true);
        async_event_task::connect(handler, res);
    }

    /// internal implementation of sbumpc() from async_streambuf
    int_type sbumpc_impl()
    {
        return this->read_byte(true);
    }

    /// internal implementation of getc() from async_streambuf
    template<typename ReadHandler>
    void getc_impl(ReadHandler handler)
    {
        int_type res = this->read_byte(false);
        async_event_task::connect(handler, res);
    }

    /// internal implementation of sgetc() from async_streambuf
    int_type sgetc_impl()
    {
        // add lock ?
        return this->read_byte(false);
    }

    /// internal implementation of nextc() from async_streambuf
    template<typename ReadHandler>
    void nextc_impl(ReadHandler handler)
    {
        int_type res = this->read_byte(true);
        async_event_task::connect(handler, res);
    }

    /// internal implementation of ungetc() from async_streambuf
    template<typename ReadHandler>
    void ungetc_impl(ReadHandler handler)
    {
        /*
        auto pos = seekoff(-1, std::ios_base::cur, std::ios_base::in);
        if ( pos == (pos_type)traits::eof())
            async_event_task::connect(handler, static_cast<int_type>(traits::eof()));
        int_type res = this->getc();
        */
    }

    /// internal implementation of getpos() from async_streambuf
    pos_type getpos_impl(std::ios_base::openmode mode) const
    {
        if ( ((mode & std::ios_base::in) && !this->can_read()) ||
             ((mode & std::ios_base::out) && !this->can_write()))
             return static_cast<pos_type>(traits::eof());

        return static_cast<pos_type>(current_position_);
    }

    /// Seeks to the given position implementation.
    pos_type seekpos_impl(pos_type position, std::ios_base::openmode mode)
    {
        pos_type beg(0);

        // In order to support relative seeking from the end position we need to fix an end position.
        // Technically, there is no end for the stream buffer as new writes would just expand the buffer.
        // For now, we assume that the current write_end is the end of the buffer. We use this artificial
        // end to restrict the read head from seeking beyond what is available.

        pos_type end(data_.size());

        if (position >= beg)
        {
            auto pos = static_cast<size_t>(position);

            // Read head
            if ((mode & std::ios_base::in) && this->can_read())
            {
                if (position <= end)
                {
                    // We do not allow reads to seek beyond the end or before the start position.
                    update_current_position(pos);
                    return static_cast<pos_type>(current_position_);
                }
            }

            // Write head
            if ((mode & std::ios_base::out) && this->can_write())
            {
                // Allocate space
                resize_for_write(pos);

                // Nothing to really copy

                // Update write head and satisfy read requests if any
                update_current_position(pos);

                return static_cast<pos_type>(current_position_);
            }
        }

        return static_cast<pos_type>(traits::eof());
    }

    /// Seeks to a position given by a relative offset implementation.
    pos_type seekoff_impl(off_type offset, std::ios_base::seekdir way, std::ios_base::openmode mode)
    {
        pos_type beg = 0;
        pos_type cur = static_cast<pos_type>(current_position_);
        pos_type end = static_cast<pos_type>(data_.size());

        switch ( way )
        {
        case std::ios_base::beg:
            return seekpos(beg + offset, mode);

        case std::ios_base::cur:
            return seekpos(cur + offset, mode);

        case std::ios_base::end:
            return seekpos(end + offset, mode);

        default:
            return static_cast<pos_type>(traits::eof());
        }
    }

    void close_read_impl()
    {
        // todo implementation
    }

    void close_write_impl()
    {
        // todo implementation
    }

private:

    static void validate_mode(std::ios_base::openmode mode)
    {
        // Disallow simultaneous use of the stream buffer for writing and reading.
        if ((mode & std::ios_base::in) && (mode & std::ios_base::out))
            throw std::invalid_argument("this combination of modes on container stream not supported");
    }

    /// <summary>
    /// Determine if the request can be satisfied.
    /// </summary>
    bool can_satisfy(size_t)
    {
        // We can always satisfy a read, at least partially, unless the
        // read position is at the very end of the buffer.
        return (in_avail_impl() > 0);
    }

    /// <summary>
    /// Reads a byte from the stream and returns it as int_type.
    /// Note: This routine shall only be called if can_satisfy() returned true.
    /// </summary>
    int_type read_byte(bool advance = true)
    {
        CharType value;
        auto read_size = this->read(&value, 1, advance);
        return read_size == 1 ? static_cast<int_type>(value) : traits::eof();
    }


    /// Reads up to count characters into ptr and returns the count of characters copied.
    /// The return value (actual characters copied) could be <= count.
    /// Note: This routine shall only be called if can_satisfy() returned true.
    size_t read(CharType *ptr, size_t count, bool advance = true)
    {
        if (!can_satisfy(count))
            return 0;

        size_t request_size(count);
        size_t read_size = std::min(request_size, in_avail_impl());

        size_t newpos = current_position_ + read_size;

        auto read_begin = begin(data_) + current_position_;
        auto read_end = begin(data_) + newpos;

#ifdef _WIN32
        // Avoid warning C4996: Use checked iterators under SECURE_SCL
        std::copy(readBegin, readEnd, stdext::checked_array_iterator<CharType *>(ptr, count));
#else
        std::copy(read_begin, read_end, ptr);
#endif // _WIN32

        if (advance)
        {
            update_current_position(newpos);
        }

        return (size_t) read_size;
    }

    /// Write count characters from the ptr into the stream buffer
    size_t write(const CharType *ptr, size_t count)
    {
        if (!this->can_write() || (count == 0)) return 0;

        auto newSize = current_position_ + count;

        // Allocate space
        resize_for_write(newSize);

        // Copy the data
        std::copy(ptr, ptr + count, begin(data_) + current_position_);

        // Update write head and satisfy pending reads if any
        update_current_position(newSize);

        return count;
    }

    /// Resize the underlying container to match the new write head
    void resize_for_write(size_t newPos)
    {
        // Resize the container if required
        if (newPos > data_.size())
        {
            data_.resize(newPos);
        }
    }

    /// Updates the write head to the new position
    void update_current_position(size_t newPos)
    {
        // The new write head
        current_position_ = newPos;
        assert(current_position_ <= data_.size());
    }

    // The actual data store
    CollectionType data_;

    // Read/write head
    size_t current_position_;
};

}}

#endif /* CONTAINER_BUFFER_H_ */
