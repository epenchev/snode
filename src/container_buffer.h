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

/// The basic_container_buffer class serves as a memory-based stream buffer that supports writing or reading
/// sequences of characters.
/// A class to allow users to create input and output streams based on STL collections.
/// The sole purpose of this class to avoid users from having to know anything about stream buffers.
/// TCollection - The type of the STL collection (ex. std::string, std::vector ..).
template<typename TCollection>
class container_buffer : public async_streambuf<typename TCollection::value_type, container_buffer<TCollection> >
{
public:
    typedef typename TCollection::value_type char_type;
    typedef async_streambuf<char_type, container_buffer<TCollection> > base_streambuf_type;
    typedef typename container_buffer<TCollection>::traits traits;
    typedef typename container_buffer<TCollection>::int_type int_type;
    typedef typename container_buffer<TCollection>::pos_type pos_type;
    typedef typename container_buffer<TCollection>::off_type off_type;

    /// Returns the underlying data container
    TCollection& collection()
    {
        return data_;
    }


    /// Constructor
    container_buffer(std::ios_base::openmode mode) : base_streambuf_type(mode), current_position_(0)
    {
        validate_mode(mode);
    }

    /// Constructor
    container_buffer(TCollection data, std::ios_base::openmode mode)
      : base_streambuf_type(std::ios_base::out | std::ios_base::in), data_(std::move(data)), current_position_(0)
              //current_position_((mode & std::ios_base::in) ? 0 : data_.size())
    {
        validate_mode(mode);
    }

    /// Destructor
    ~container_buffer()
    {
        // Invoke the synchronous versions since we need to
        // purge the request queue before deleting the buffer
        this->close_read();
        this->close_write();
    }

    /// checks if stream buffer supports seeking.
    bool can_seek() { return this->is_open(); }

    /// checks whether a stream buffer supports size().
    bool has_size() { return this->is_open(); }

    /// Gets the size of the stream, if known. Calls to has_size() will determine whether
    /// the result of size can be relied on.
    uint64_t size()
    {
        return uint64_t(data_.size());
    }

    /// Gets the stream buffer size for in or out direction, if one has been set.
    size_t buffer_size(std::ios_base::openmode = std::ios_base::in) const
    {
        return 0;
    }

    /// Sets the stream buffer implementation to buffer or not buffer for the given direction.
    void set_buffer_size(size_t , std::ios_base::openmode = std::ios_base::in)
    {
        return;
    }

    /// For any input stream,
    /// returns the number of characters that are immediately available to be consumed without blocking.
    /// For details see async_streambuf::in_avail()
    size_t in_avail() const
    {
        // See the comment in seek around the restriction that we do not allow read head to
        // seek beyond the current write_end.
        assert(current_position_ <= data_.size());

        size_t readhead(current_position_);
        size_t write_end(data_.size());
        return (size_t)(write_end - readhead);
    }

    /// For output streams, flush any internally buffered data to the underlying medium.
    bool sync()
    {
        return (true);
    }

    /// Writes a single character to the stream buffer.
    /// For details see async_streambuf::putc()
    template<typename THandler>
    void putc(char_type ch, THandler handler)
    {
        int_type res = (this->write(&ch, 1) == 1) ? static_cast<int_type>(ch) : traits::eof();
        async_task::connect(handler, res);
    }

    /// Writes a number of characters to the stream buffer from memory.
    /// For details see async_streambuf::putn()
    template<typename THandler>
    void putn(char_type* ptr, size_t count, THandler handler)
    {
        size_t res = this->write(ptr, count);
        async_task::connect(handler, res);
    }

    /// Allocates a contiguous block of memory of (count) bytes and returns it.
    /// For details see async_streambuf::alloc()
    char_type* alloc(size_t count)
    {
        if (!this->can_write())
            return nullptr;
        // Allocate space
        resize_for_write(current_position_+count);

        // Let the caller copy the data
        return (char_type*)&data_[current_position_];
    }

    /// Submits a block already allocated by the stream buffer.
    /// For details see async_streambuf::commit()
    void commit(size_t count)
    {
        // Update the write position and satisfy any pending reads
        update_current_position(current_position_ + count);
    }

    /// Gets a pointer to the next already allocated contiguous block of data.
    /// For details see async_streambuf::acquire()
    bool acquire(char_type*& ptr, size_t& count)
    {
        ptr = nullptr;
        count = 0;

        if (!this->can_read()) return false;

        count = in_avail();

        if (count > 0)
        {
            ptr = (char_type*)&data_[current_position_];
            return true;
        }
        else
        {
            // Can only be open for read OR write, not both. If there is no data then
            // we have reached the end of the stream so indicate such with true.
            return true;
        }
    }

    /// Releases a block of data acquired using acquire() method
    /// For details see async_streambuf::release()
    void release(char_type* ptr, size_t count)
    {
        if (ptr != nullptr)
        {
            update_current_position(current_position_ + count);
        }
    }

    /// Reads up to a given number of characters from the stream buffer to memory.
    /// For details see async_streambuf::getn()
    template<typename THandler>
    void getn(char_type* ptr, size_t count, THandler handler)
    {
        int_type res = this->read(ptr, count);
        async_task::connect(handler, res);
    }

    /// Reads up to a given number of characters from the stream buffer to memory synchronously.
    /// For details see async_streambuf::sgetn()
    size_t sgetn(char_type* ptr, size_t count)
    {
        return this->read(ptr, count);
    }

    /// Copies up to a given number of characters from the stream buffer to memory synchronously.
    /// For details see async_streambuf::scopy()
    size_t scopy(char_type* ptr, size_t count)
    {
        return this->read(ptr, count, false);
    }

    /// Reads a single character from the stream and advances the read position.
    /// For details see async_streambuf::bumpc()
    template<typename THandler>
    void bumpc(THandler handler)
    {
        int_type res = this->read_byte(true);
        async_task::connect(handler, res);
    }

    /// Reads a single character from the stream and advances the read position.
    /// For details see async_streambuf::sbumpc()
    int_type sbumpc()
    {
        return this->read_byte(true);
    }

    /// Reads a single character from the stream without advancing the read position.
    /// For details see async_streambuf::getc()
    template<typename THandler>
    void getc(THandler handler)
    {
        int_type res = this->read_byte(false);
        async_task::connect(handler, res);
    }

    /// Reads a single character from the stream without advancing the read position.
    /// For details see async_streambuf::sgetc()
    int_type sgetc()
    {
        return this->read_byte(false);
    }

    /// Advances the read position, then returns the next character without advancing again.
    /// For details see async_streambuf::nextc()
    template<typename THandler>
    void nextc(THandler handler)
    {
        // TODO move read pointer in advance
        int_type res = this->read_byte(true);
        async_task::connect(handler, res);
    }

    /// Retreats the read position, then returns the current character without advancing.
    /// For details see async_streambuf::ungetc()
    template<typename THandler>
    void ungetc(THandler handler)
    {
        /*
        auto pos = seekoff(-1, std::ios_base::cur, std::ios_base::in);
        if ( pos == (pos_type)traits::eof())
            async_task::connect(handler, static_cast<int_type>(traits::eof()));
        int_type res = this->getc();
        */
    }

    /// Gets the current read or write position in the stream for the given direction.
    /// For details see async_streambuf::getpos()
    pos_type getpos(std::ios_base::openmode mode) const
    {
        if ( ((mode & std::ios_base::in) && !this->can_read()) ||
             ((mode & std::ios_base::out) && !this->can_write()))
        {
             return static_cast<pos_type>(traits::eof());
        }

        return static_cast<pos_type>(current_position_);
    }

    /// Seeks to the given position for the given (direction).
    /// For details see async_streambuf::seekpos()
    pos_type seekpos(pos_type position, std::ios_base::openmode mode)
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

    /// Seeks to a position given by a relative offset.
    /// For details see async_streambuf::seekoff()
    pos_type seekoff(off_type offset, std::ios_base::seekdir way, std::ios_base::openmode mode)
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

    void close_read()
    {
        this->stream_can_read_ = false;
    }

    void close_write()
    {
        this->stream_can_write_ = false;
    }

private:

    static void validate_mode(std::ios_base::openmode mode)
    {
        // Disallow simultaneous use of the stream buffer for writing and reading.
        if ((mode & std::ios_base::in) && (mode & std::ios_base::out))
            throw std::invalid_argument("this combination of modes on container stream not supported");
    }

    /// Determine if the request can be satisfied.
    bool can_satisfy(size_t)
    {
        // We can always satisfy a read, at least partially, unless the
        // read position is at the very end of the buffer.
        return (in_avail() > 0);
    }

    /// Reads a byte from the stream and returns it as int_type.
    /// If advance is set move the read head.
    /// Note: This routine shall only be called if can_satisfy() returned true.
    int_type read_byte(bool advance = true)
    {
        char_type value;
        auto read_size = this->read(&value, 1, advance);
        return read_size == 1 ? static_cast<int_type>(value) : traits::eof();
    }


    /// Reads up to count characters into ptr and returns the count of characters copied.
    /// The return value (actual characters copied) could be <= count.
    /// If advance is set move the read head.
    /// Note: This routine shall only be called if can_satisfy() returned true.
    size_t read(char_type *ptr, size_t count, bool advance = true)
    {
        if (!can_satisfy(count))
        {
            return 0;
        }

        size_t request_size(count);
        size_t read_size = std::min(request_size, in_avail());

        size_t newpos = current_position_ + read_size;

        auto read_begin = begin(data_) + current_position_;
        auto read_end = begin(data_) + newpos;

#ifdef _WIN32
        // Avoid warning C4996: Use checked iterators under SECURE_SCL
        std::copy(readBegin, readEnd, stdext::checked_array_iterator<char_type *>(ptr, count));
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
    size_t write(const char_type *ptr, size_t count)
    {
        if (!this->can_write() || (count == 0))
        {
            return 0;
        }

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
    TCollection data_;

    // Read/write head
    size_t current_position_;
};

}}

#endif /* CONTAINER_BUFFER_H_ */
