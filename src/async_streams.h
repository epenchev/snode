//
// async_streams.h
// Copyright (C) 2015  Emil Penchev, Bulgaria

#ifndef ASYNC_STREAMS_H_
#define ASYNC_STREAMS_H_

#include <ios>
#include <set>
#include <sstream>
#include <exception>
#include <stdexcept>
#include <memory>
#include <type_traits>

#include "async_task.h"

namespace snode
{
/// Library for asynchronous streams.
namespace streams
{
    /// Custom char_traits
    /// CharType - The data type of the basic element of the stream.
    template<typename CharType>
    struct char_traits : std::char_traits<CharType>
    {
        /// Some synchronous functions will return this value if the operation requires an asynchronous call in a given situation.
        /// Returns an int_type value which implies that an asynchronous call is required.
        static typename std::char_traits<CharType>::int_type requires_async()
        {
            return std::char_traits<CharType>::eof()-1;
        }
    };

    namespace utils
    {
        /// Value to string formatters
        template <typename CharType>
        struct value_string_formatter
        {
            template <typename T>
            static std::basic_string<CharType> format(const T &val)
            {
                std::basic_ostringstream<CharType> ss;
                ss << val;
                return ss.str();
            }
        };

        template <>
        struct value_string_formatter <uint8_t>
        {
            template <typename T>
            static std::basic_string<uint8_t> format(const T &val)
            {
                std::basic_ostringstream<char> ss;
                ss << val;
                return reinterpret_cast<const uint8_t *>(ss.str().c_str());
            }
        };

        static const char* s_in_stream_msg = "stream not set up for input of data";
        static const char* s_in_streambuf_msg = "stream buffer not set up for input of data";
        static const char* s_out_stream_msg = "stream not set up for output of data";
        static const char* s_out_streambuf_msg = "stream buffer not set up for output of data";
    }

    /// Helper macros for easy completion handlers binding
    #define BIND_HANDLER(func, ...)  boost::bind(func, this, _1, ##__VA_ARGS__)
    #define BIND_OBJ_HANDLER(func, obj, ...)  boost::bind(func, obj, _1, ##__VA_ARGS__)
    #define BIND_SHARED_HANDLER(func, ...) boost::bind(func, this->shared_from_this(), _1, ##__VA_ARGS__)

    /// Base class for all async_streambuf completion operations.
    /// A function pointer is used instead of virtual functions to avoid the associated overhead.
    template<typename CharType>
    class async_streambuf_op_base
    {
    public:
        typedef snode::streams::char_traits<CharType> traits;
        typedef typename traits::int_type int_type;

        /// Executes handler with expected size parameter.
        void complete_size(std::size_t size)
        {
            func_size_(this, size);
        }

        /// Executes handler with expected char parameter.
        void complete_ch(int_type ch)
        {
            func_ch_(this, ch);
        }

    protected:
        typedef void (*func_ch_type)(async_streambuf_op_base*, int_type);
        typedef void (*func_size_type)(async_streambuf_op_base*, std::size_t);

        async_streambuf_op_base(func_ch_type func_ch, func_size_type func_size)
         : func_ch_(func_ch), func_size_(func_size)
        {
        }

    private:
        func_ch_type func_ch_;
        func_size_type func_size_;
    };

    /// Wraps handlers into async_streambuf completion operations.
    /// A handler (Handler) is functor or function pointer to be called when an asynchronous operation completes.
    /// Also handler must be copy-constructible.
    template<typename CharType, typename Handler>
    class async_streambuf_op : public async_streambuf_op_base<CharType>
    {
    public:
        typedef async_streambuf_op_base<CharType> streambuf_op_base;
        typedef snode::streams::char_traits<CharType> traits;
        typedef typename traits::int_type int_type;

        async_streambuf_op(Handler h)
          : streambuf_op_base(&async_streambuf_op::do_complete_ch, &async_streambuf_op::do_complete_size), handler_(h)
        {}

        static void do_complete_ch(streambuf_op_base* base, int_type ch)
        {
            async_streambuf_op* op(static_cast<async_streambuf_op*>(base));
            op->handler_(ch);
            delete op;
        }

        static void do_complete_size(streambuf_op_base* base, std::size_t size)
        {
            async_streambuf_op* op(static_cast<async_streambuf_op*>(base));
            op->handler_(size);
            delete op;
        }
        private:
            Handler handler_;
    };

    // Forward declarations
    template<typename CharType, typename Impl> class async_istream;
    template<typename CharType, typename Impl> class async_ostream;


    /// Asynchronous stream buffer base class.
    /// CharType is the data type of the basic element of the async_streambuf
    /// and Impl is the actual buffer internal implementation.
    template<typename CharType, typename Impl>
    class async_streambuf
    {
    public:
        typedef CharType char_type;
        typedef snode::streams::char_traits<CharType> traits;
        typedef typename traits::int_type int_type;
        typedef typename traits::pos_type pos_type;
        typedef typename traits::off_type off_type;
        typedef async_ostream<CharType, Impl> ostream_type;
        typedef async_istream<CharType, Impl> istream_type;

        /// Default constructor, (mode) the I/O mode (in or out) of the buffer.
        async_streambuf(std::ios_base::openmode mode)
        {
            stream_can_read_ = (mode & std::ios_base::in) != 0;
            stream_can_write_ = (mode & std::ios_base::out) != 0;
            stream_read_eof_ = false;
            alloced_ = false;
        }

        /// Constructs an input stream head for this stream buffer.
        istream_type create_istream()
        {
            if (!can_read())
                throw std::runtime_error("stream buffer not set up for input of data");
            return istream_type(this);
        }

        /// Constructs an output stream for this stream buffer.
        ostream_type create_ostream()
        {
            if (!can_write())
                throw std::runtime_error("stream buffer not set up for output of data");
            return ostream_type(this);
        }

        /// can_seek() is used to determine whether a stream buffer supports seeking.
        bool can_seek() const
        {
            return get_impl()->can_seek_impl();
        }

        /// has_size() is used to determine whether a stream buffer supports size().
        bool has_size() const
        {
            return get_impl()->has_size_impl();
        }

        /// can_read() is used to determine whether a stream buffer will support read operations (get).
        bool can_read() const
        {
            return stream_can_read_;
        }

        /// can_write() is used to determine whether a stream buffer will support write operations (put).
        bool can_write() const
        {
            return stream_can_write_;
        }

        /// Checks if the stream buffer is open.
        bool is_open() const
        {
            return can_read() || can_write();
        }

        /// Gets the stream buffer size for in or out direction, if one has been set.
        size_t buffer_size(std::ios_base::openmode direction = std::ios_base::in) const
        {
            return get_impl()->buffer_size_impl(direction);
        }

        /// Sets the stream buffer implementation to buffer or not buffer for the given direction (in or out).
        void set_buffer_size(size_t size, std::ios_base::openmode direction = std::ios_base::in)
        {
            get_impl()->set_buffer_size_impl(size, direction);
        }

        /// For any input stream, returns the number of characters that are immediately available to be consumed without blocking
        /// May be used in conjunction with sbumpc() method to read data without using async tasks.
        size_t in_avail()
        {
            return get_impl()->in_avail_impl();
        }

        /// Gets the current read or write position in the stream for the given (direction).
        /// Returns the current position. EOF if the operation fails.
        /// Some streams may have separate write and read cursors.
        /// For such streams, the direction parameter defines whether to move the read or the write cursor.</remarks>
        pos_type getpos(std::ios_base::openmode direction) //const
        {
            return get_impl()->getpos_impl();
        }

        /// Gets the size of the stream, if known. Calls to has_size() will determine whether the result of size can be relied on.
        size_t size() const
        {
            return get_impl()->size_impl();
        }

        /// Seeks to the given position (pos is offset from beginning of the stream) for the given (direction).
        /// Returns the position. EOF if the operation fails.
        /// Some streams may have separate write and read cursors. For such streams, the direction parameter defines whether to move the read or the write cursor.
        pos_type seekpos(pos_type pos, std::ios_base::openmode direction)
        {
            return get_impl()->seekpos_impl(pos, direction);
        }

        /// Seeks to a position given by a relative (offset) with starting point (way beginning, end, current) for the seek and with I/O direction (pos in/out).
        /// Returns the position. EOF if the operation fails.
        /// Some streams may have separate write and read cursors and for such streams,
        /// the mode parameter defines whether to move the read or the write cursor.
        pos_type seekoff(off_type offset, std::ios_base::seekdir way, std::ios_base::openmode mode)
        {
            return get_impl()->seekof_impl(offset, way, mode);
        }

        /// Closes the stream buffer for the I/O mode (in or out), preventing further read or write operations.
        void close(std::ios_base::openmode mode = std::ios_base::in | std::ios_base::out)
        {
            if ((mode & std::ios_base::in) && can_read())
                get_impl()->close_read_impl();

            if ((mode & std::ios_base::out) && can_write())
                get_impl()->close_write_impl();
        }

        /// Closes the stream buffer for the I/O mode (in or out) with an exception.
        void close(std::ios_base::openmode mode, std::exception_ptr eptr)
        {
            if (current_ex_ == nullptr)
                current_ex_ = eptr;
            return close(mode);
        }

        /// is_eof() is used to determine whether a read head has reached the end of the buffer.
        bool is_eof() const
        {
            return stream_read_eof_;
        }

        /// Writes a single character (ch) to the stream buffer,
        /// (handler) is the handler to be called when the write operation completes.
        /// Copies will be made of the handler as required. The function signature of the handler must be:
        /// void handler(int_type ch) where ch is character value (EOF if the write operation failed).
        template<typename WriteHandler>
        void putc(CharType ch, WriteHandler handler)
        {
            if (!can_write())
                throw std::runtime_error(utils::s_in_streambuf_msg);

            get_impl()->putc_impl(ch, handler);
        }

        /// Writes a number (count) of characters to the stream buffer from source memory (ptr).
        /// (handler) is the handler to be called when the write operation completes.
        /// Copies will be made of the handler as required. The function signature of the handler must be:
        /// void handler(size_t count) where count is the byte count written or 0 if the write operation failed.
        template<typename WriteHandler>
        void putn(const CharType* ptr, size_t count, WriteHandler handler)
        {
            if (!can_write())
                throw std::runtime_error(utils::s_in_streambuf_msg);

            if (count == 0)
                async_task::connect(handler, 0);
            else
                get_impl()->putn_impl(ptr, count, handler);
        }

        /// Reads a single character from the stream and advances the read position.
        /// (handler) is the handler to be called when the read operation completes.
        /// Copies will be made of the handler as required. The function signature of the handler must be:
        /// void handler(int_type ch) where ch is the resulting character or EOF if the read operation failed.
        template<typename ReadHandler>
        void bumpc(ReadHandler handler)
        {
            if (!can_read())
                throw std::runtime_error(utils::s_out_streambuf_msg);

            get_impl()->bumpc_impl(handler);
        }

        /// Reads a single character from the stream and advances the read position.
        /// Returns the value of the character, (-1) if the read fails. (-2) if an asynchronous read is required
        /// This is a synchronous operation, but is guaranteed to never block.
        int_type sbumpc()
        {
            if ( !(current_ex_ == nullptr) )
                std::rethrow_exception(current_ex_);
            if (!can_read())
                return traits::eof();
            return check_sync_read_eof(get_impl()->sbumpc_impl());
        }

        /// Reads a single character from the stream without advancing the read position.
        /// (handler) is the handler to be called when the read operation completes.
        /// Copies will be made of the handler as required. The function signature of the handler must be:
        /// void handler(int_type ch) where ch is character value (EOF if the read operation failed).
        template<typename ReadHandler>
        void getc(ReadHandler handler)
        {
            if (!can_read())
                throw std::runtime_error(utils::s_out_streambuf_msg);

            get_impl()->getc_impl(handler);
        }

        /// Reads a single character from the stream without advancing the read position.
        /// Returns the value of the character. EOF if the read fails, check requires_async() method if an asynchronous read is required.
        /// This is a synchronous operation, but is guaranteed to never block.
        int_type sgetc()
        {
            if ( !(current_ex_ == nullptr) )
                std::rethrow_exception(current_ex_);
            if (!can_read())
                return traits::eof();
            return check_sync_read_eof(get_impl()->sgetc_impl());
        }

        /// Advances the read position, then returns the next character without advancing again.
        /// (handler) is the handler to be called when the read operation completes.
        /// Copies will be made of the handler as required. The function signature of the handler must be:
        /// void handler(int_type ch) where ch is character value (EOF if the read operation failed).
        template<typename ReadHandler>
        void nextc(ReadHandler handler)
        {
            if (!can_read())
                throw std::runtime_error(utils::s_out_streambuf_msg);

            get_impl()->nextc_impl(handler);
        }

        /// Retreats the read position, then returns the current character without advancing.
        /// (handler) is the handler to be called when the read operation completes.
        /// Copies will be made of the handler as required. The function signature of the handler must be:
        /// void handler(int_type ch) where ch is character value (EOF if the read operation failed).
        template<typename ReadHandler>
        void ungetc(ReadHandler handler)
        {
            if (!can_read())
                throw std::runtime_error(utils::s_out_streambuf_msg);

            get_impl()->ungetc_impl(handler);
        }

        /// Reads up to a given number (count) of characters from the stream from source memory (ptr).
        /// (handler) is the handler to be called when the read operation completes.
        /// Copies will be made of the handler as required. The function signature of the handler must be:
        /// void handler(size_t count) where count is the character count read or 0 if the end of the stream is reached.
        template<typename ReadHandler>
        void getn(CharType* ptr, size_t count, ReadHandler handler)
        {
            if (!can_read())
                throw std::runtime_error(utils::s_out_stream_msg);

            if (count == 0)
                async_task::connect(handler, 0);
            else
                get_impl()->getn_impl(ptr, count, handler);
        }

        /// Copies up to a given number (count) of characters from the stream synchronously, from source memory (ptr).
        /// Returns the number of characters copied. O if the end of the stream is reached or an asynchronous read is required.
        /// This is a synchronous operation, but is guaranteed to never block.
        size_t scopy(CharType* ptr, size_t count)
        {
            if ( !(current_ex_ == nullptr) )
                std::rethrow_exception(current_ex_);
            if (!can_read())
                return 0;

            return get_impl()->scopy_impl(ptr, count);
        }

        /// For output streams, flush any internally buffered data to the underlying medium.
        void sync()
        {
            if (!can_write())
            {
                if (current_ex_ == nullptr)
                    throw std::runtime_error(utils::s_out_streambuf_msg);
                else
                    throw current_ex_;
            }
            get_impl()->sync_impl();
        }

        /// Retrieves the stream buffer exception_ptr if it has been set otherwise nullptr will be returned.
        std::exception_ptr exception() const
        {
            return current_ex_;
        }

        //
        // Efficient read and write.
        //
        // The following routines are intended to be used for more efficient, copy-free, reading and
        // writing of data from/to the stream. Rather than having the caller provide a buffer into which
        // data is written or from which it is read, the stream buffer provides a pointer directly to the
        // internal data blocks that it is using. Since not all stream buffers use internal data structures
        // to copy data, the functions may not be supported by all. An application that wishes to use this
        // functionality should therefore first try them and check for failure to support. If there is
        // such failure, the application should fall back on the copying interfaces (putn / getn)
        //

        /// Allocates a contiguous block of memory of (count) bytes and returns it. Returns nullptr on error or if method is not supported.
        CharType* alloc(size_t count)
        {
            if (alloced_)
                throw std::logic_error("The buffer is already allocated, this maybe caused by overlap of stream read or write");

            CharType* alloc_result = get_impl()->alloc_impl(count);

            if (alloc_result)
                alloced_ = true;

            return alloc_result;
        }

        /// Submits a block already allocated by the stream buffer.
        /// (count) The number of characters to be committed.
        void commit(size_t count)
        {
            if (!alloced_)
                throw std::logic_error("The buffer needs to allocate first");

            get_impl()->commit_impl(count);
            alloced_ = false;
        }


        /// Gets a pointer to the next already allocated contiguous block of data.
        /// (ptr) A reference to a pointer variable that will hold the address of the block on success.
        /// (count) The number of contiguous characters available at the address in 'ptr.'
        /// Returns true if the operation succeeded, false otherwise.
        /// A return of false does not necessarily indicate that a subsequent read operation would fail, only that
        /// there is no block to return immediately or that the stream buffer does not support the operation.
        /// The stream buffer may not de-allocate the block until ::release() method is called.
        /// If the end of the stream is reached, the function will return true, a null pointer, and a count of zero;
        /// a subsequent read will not succeed.
        bool acquire(CharType*& ptr, size_t& count)
        {
            return get_impl()->acquire_impl(ptr, count);
        }

        /// Releases a block of data acquired using ::acquire() method. This frees the stream buffer to de-allocate the
        /// memory, if it so desires. Move the read position ahead by the count.
        ///(ptr) A pointer to the block of data to be released.
        /// (count) The number of characters that were read.
        void release(CharType* ptr, size_t count)
        {
            get_impl()->release_impl(ptr, count);
        }

    protected:
        std::exception_ptr current_ex_;

        // The in/out mode for the buffer
        bool stream_can_read_, stream_can_write_, stream_read_eof_, alloced_;

        inline Impl* get_impl()
        {
            return static_cast<Impl*>(this);
        }

        /// The real read head close operation, implementation should override it if there is any resource to be released.
        void close_read()
        {
            stream_can_read_ = false;
        }

        /// The real write head close operation, implementation should override it if there is any resource to be released.
        void close_write()
        {
            stream_can_write_ = false;
        }

        /// Set EOF states for sync read
        int_type check_sync_read_eof(int_type ch)
        {
            stream_read_eof_ = ch == traits::eof();
            return ch;
        }
    };

    /// Base class for all asynchronous output streams.
    template<typename CharType, typename Impl>
    class async_ostream
    {
    public:
        typedef ::snode::streams::char_traits<CharType> traits;
        typedef typename traits::int_type int_type;
        typedef typename traits::pos_type pos_type;
        typedef typename traits::off_type off_type;
        typedef typename ::snode::streams::async_streambuf<CharType, Impl> streambuf_type;

        /// Default constructor
        async_ostream() {}

        /// Copy constructor
        async_ostream(const async_ostream<CharType, Impl>& other) : requests_(other.requests_), buffer_(other.buffer_) {}

        /// Assignment operator
        async_ostream& operator =(const async_ostream<CharType, Impl>& other)
        {
            buffer_ = other.buffer_;
            requests_ = other.requests_;
            return *this;
        }

        /// Constructor
        async_ostream(async_streambuf<CharType, Impl>* buffer) : buffer_(buffer)
        {
            verify_and_throw(utils::s_out_streambuf_msg);
        }

        /// Close the stream, preventing further write operations.
        void close()
        {
            if (is_valid())
                buffer_->close(std::ios_base::out);
        }

        /// Close the stream with exception (eptr), preventing further write operations.
        void close(std::exception_ptr eptr)
        {
            if (is_valid())
                buffer_->close(std::ios_base::out, eptr);
        }

        /// Put a single character (ch) into the stream.
        /// (handler) is the handler to be called when the write operation completes.
        /// Copies will be made of the handler as required. The function signature of the handler must be:
        /// void handler(int_type ch) where ch is character value (EOF if the write operation failed).
        template<typename WriteHandler>
        void write(CharType ch, WriteHandler handler)
        {
            verify_and_throw(utils::s_out_streambuf_msg);
            buffer_->putc(ch, handler);
        }

        /// Write a number (count) of characters from a given stream buffer (source) into the stream.
        /// (handler) is the handler to be called when the write operation completes.
        /// Copies will be made of the handler as required. The function signature of the handler must be:
        /// void handler(size_t count) where count is the byte count written or 0 if the write operation failed.
        template<typename WriteHandler, typename BufferImpl>
        void write(async_streambuf<CharType, BufferImpl>& source, size_t count, WriteHandler handler)
        {
            verify_and_throw(utils::s_out_streambuf_msg);
            if ( !source.can_read() )
                throw std::runtime_error("source buffer not set up for input of data");

            if (count == 0)
                return;

            async_streambuf_op<CharType, WriteHandler> handler_op(handler);
            std::shared_ptr<write_context> context(std::make_shared<write_context>(handler_op, count));
            requests_.insert(context);

            auto data = buffer_->alloc(count);
            if ( data != nullptr )
            {
                source.getn(data, count, boost::bind(&async_ostream::write_context::post_read, _1, this, data));
            }
            else
            {
                size_t available = 0;
                const bool acquired = source.acquire(data, available);
                if ( available >= count )
                {
                    buffer_->putn(data, count, boost::bind(&async_ostream::write_context::post_write<BufferImpl>, _1, this, source));
                }
                else
                {
                    // Always have to release if acquire returned true.
                    if (acquired)
                        source.release(data, 0);

                    std::shared_ptr<CharType> buf(new CharType[count], [](CharType* buf) { delete [] buf; });
                    source.getn(buf.get(), count, boost::bind(&async_ostream::write_context::post_read, _1, this, buf));
                }
            }
        }

        /// Write the specified string (str) to the output stream.
        /// (handler) is the handler to be called when the write operation completes.
        /// Copies will be made of the handler as required. The function signature of the handler must be:
        /// void handler(size_t count) where count is the character count read or 0 if the end of the stream is reached.
        template<typename WriteHandler>
        void print(const std::basic_string<CharType>& str, WriteHandler handler)
        {
            verify_and_throw(utils::s_out_streambuf_msg);
            buffer_->putn(str.c_str(), str.size(), handler);
        }

        /// Write a value of type T to the output stream.
        /// (handler) is the handler to be called when the write operation completes.
        /// Copies will be made of the handler as required. The function signature of the handler must be:
        /// void handler(size_t count) where count is the character count read or 0 if the end of the stream is reached.
        template<typename T, typename WriteHandler>
        void print(const T& val, WriteHandler handler)
        {
            verify_and_throw(utils::s_out_stream_msg);
            return print(utils::value_string_formatter<CharType>::format(val), handler);
        }

        /// Write a value of type T to the output stream and append a newline character.
        /// (handler) is the handler to be called when the write operation completes.
        /// Copies will be made of the handler as required. The function signature of the handler must be:
        /// void handler(size_t count) where count is the character count read or 0 if the end of the stream is reached.
        template<typename T, typename WriteHandler>
        void print_line(const T& val, WriteHandler handler)
        {

            verify_and_throw(utils::s_out_stream_msg);
            auto str = utils::value_string_formatter<CharType>::format(val);
            str.push_back(CharType('\n'));
            return print(str, handler);
        }

        /// Flush any buffered output data.
        void flush()
        {
            verify_and_throw(utils::s_out_streambuf_msg);
            buffer_->sync();
        }

        /// Seeks to the specified write position (pos) an offset relative to the beginning of the stream.
        /// returns the new position in the stream.</returns>
        pos_type seek(pos_type pos) const
        {
            verify_and_throw(utils::s_out_stream_msg);
            return buffer_->seekpos(pos, std::ios_base::out);
        }


        /// Seeks to the specified write position with an offset (off) relative to the beginning, current write position, or the end of the stream.
        /// Parameter (way) is the starting point (beginning, current, end) for the seek.
        /// Returns the new position in the stream.
        pos_type seek(off_type off, std::ios_base::seekdir way)
        {
            verify_and_throw(utils::s_out_stream_msg);
            return buffer_->seekoff(off, way, std::ios_base::out);
        }

        /// Get the current write position, i.e. the offset from the beginning of the stream.
        pos_type tell() const
        {
            verify_and_throw(utils::s_out_stream_msg);
            return buffer_->getpos(std::ios_base::out);
        }

        /// can_seek() is used to determine whether the stream supports seeking.
        /// Returns true if the stream supports seeking, false otherwise.
        bool can_seek() const { return is_valid() && buffer_->can_seek(); }

        /// Test whether the stream has been initialized with a valid stream buffer.
        /// Returns true if the stream has been initialized with a valid stream buffer, false otherwise.
        bool is_valid() const { return (buffer_ != nullptr); }

        /// Test whether the stream has been initialized or not.
        operator bool() const { return is_valid(); }

        /// Test whether the stream is open for writing.
        /// Returns true if the stream is open for writing, false otherwise.
        bool is_open() const { return is_valid() && buffer_->can_write(); }

        /// Get the underlying stream buffer.
        /// Returns the underlying stream buffer.
        streambuf_type& streambuf() { return *buffer_; }

    protected:
        /// Internal completion handler to handle the internal streambuf async calls.
        class write_context : public std::enable_shared_from_this<write_context>
        {
        public:
            write_context(async_streambuf_op_base<CharType> op, size_t count) : callback_op_(op), count_(count)
            {
            }

            template<typename BuffImpl>
            void post_write(size_t count, async_ostream<CharType, Impl>* parent, async_streambuf<CharType, BuffImpl>* source)
            {
                CharType* data;
                source->acquire(data, count);
                if (data != nullptr)
                    source->release(data, count);

                // signal parent, operation is complete
                async_task::connect(&async_ostream::post_write_complete, parent, this->shared_from_this());
            }

            void post_read(size_t count, async_ostream<CharType, Impl>* parent, std::shared_ptr<CharType> buf)
            {
                if (count)
                {
                    CharType* data;
                    const bool acquired = parent->buffer_->acquire(data, count);
                    // data is already allocated in the source, just committing.
                    if (acquired)
                        parent->buffer_->commit(count);
                    else if (buf)
                        parent->buffer_->putn(buf.get(), count, BIND_HANDLER(&write_context::on_write_complete, parent));
                }
            }

            void on_write_complete(size_t count, async_ostream<CharType, Impl>* parent)
            {
                // signal parent, write operation is complete
                if (parent && count)
                    async_task::connect(&async_ostream::post_write_complete, parent, this->shared_from_this());
            }

            // bytes to be written to the stream
            size_t count_;
            // user provided handler executed when write is complete
            async_streambuf_op_base<CharType> callback_op_;
        };

        void post_write_complete(std::shared_ptr<write_context> context)
        {
            context->callback_op_(context->count_);
            requests_.erase(context);
        }

        void verify_and_throw(const char* msg) const
        {
            if ( !(buffer_->exception() == nullptr) ) // check the instructions set, this is the original !!!
                std::rethrow_exception(buffer_->exception());
            if ( !buffer_->can_write() )
                throw std::runtime_error(msg);
        }

        friend class write_context;
        std::set<std::shared_ptr<write_context>> requests_;
        async_streambuf<CharType, Impl>* buffer_;
    };

    /// Base interface for all asynchronous input streams.
    template<typename CharType, typename Impl>
    class async_istream
    {
    public:
        typedef ::snode::streams::char_traits<CharType> traits;
        typedef typename traits::int_type int_type;
        typedef typename traits::pos_type pos_type;
        typedef typename traits::off_type off_type;

        /// Default constructor
        async_istream() {}

        /// Constructor
        async_istream(async_streambuf<CharType, Impl>* buffer) : buffer_(buffer)
        {
            verify_and_throw(utils::s_in_streambuf_msg);
        }

        /// Copy constructor
        async_istream(const async_istream& other) : buffer_(other.buffer_) {}

        /// Assignment operator
        async_istream& operator =(const async_istream& other)
        {
            buffer_ = other.buffer_;
            return *this;
        }

        /// Close the stream, preventing further read operations.
        void close()
        {
            if (is_valid())
                buffer_->close(std::ios_base::in);
        }

        /// Close the stream with exception, preventing further read operations.
        void close(std::exception_ptr eptr)
        {
            if (is_valid())
                buffer_->close(std::ios_base::in, eptr);
        }

        /// Tests whether last read cause the stream reach EOF.
        /// Returns true if the read head has reached the end of the stream, false otherwise.
        bool is_eof() const
        {
            return is_valid() ? buffer_->is_eof() : false;
        }

        /// Get the next character and return it as an int_type. Advance the read position.
        /// (handler) is the handler to be called when the read operation completes.
        /// Copies will be made of the handler as required. The function signature of the handler must be:
        /// void handler(int_type ch) where ch is the resulting character or EOF if the read operation failed.
        template<typename ReadHandler>
        int_type read(ReadHandler handler)
        {
            verify_and_throw(utils::s_in_streambuf_msg);
            return buffer_->bumpc(handler);
        }

        /// Reads up to (count) characters and place them into the provided buffer (target).
        template<typename ReadHandler>
        void read(async_streambuf<CharType, Impl>& target, size_t count, ReadHandler handler)
        {
            verify_and_throw(utils::s_in_streambuf_msg);
            if ( !target.can_write() )
                throw std::runtime_error("target not set up for output of data");

            if (count == 0)
                return;

            async_streambuf_op<CharType, ReadHandler> handler_op(handler);
            std::shared_ptr<read_context> context(std::make_shared<read_context>(handler_op, count));
            requests_.insert(context);

            auto data = target.alloc(count);
            if ( data != nullptr )
            {
                buffer_->getn(data, count, boost::bind(&read_context::post_read, context, this, &target, nullptr));
            }
            else
            {
                size_t available = 0;

                const bool acquired = buffer_->acquire(data, available);
                if (available >= count)
                {
                    target.putn(data, count, boost::bind(&read_context::post_write, _1, this));
                }
                else
                {
                    // Always have to release if acquire returned true.
                    if (acquired)
                    {
                        buffer_->release(data, 0);
                    }

                    std::shared_ptr<CharType> buf(new CharType[count], [](CharType *buf) { delete [] buf; });
                    buffer_->getn(buf.get(), count, boost::bind(&read_context::post_read, context, this, &target, buf));
                }
            }
        }

        /// Get the next character and return it as an int_type. Do not advance the read position.
        /// (handler) is called with the result widened to an integer. This character is EOF when the peek operation fails.
        /// Copies will be made of the handler as required. The function signature of the handler must be:
        /// void handler(int_type ch) where ch is the resulting character or EOF if the operation failed.
        template<typename ReadHandler>
        void peek(ReadHandler handler)
        {
            verify_and_throw(utils::s_in_streambuf_msg);
            buffer_->getc(handler);
        }

        /// Read characters until a delimiter (delim) or EOF is found, and place them into the target buffer (target).
        /// Proceed past the delimiter, but don't include it in the target buffer.
        /// (handler) is the handler to be called when the read operation completes.
        /// Copies will be made of the handler as required. The function signature of the handler must be:
        /// void handler(size_t count) where count is the character count read or 0 if the end of the stream is reached.
        template<typename ReadHandler>
        void read_to_delim(async_streambuf<CharType, Impl>& target, int_type delim, ReadHandler handler)
        {
            verify_and_throw(utils::s_in_streambuf_msg);
            if ( !target.can_write() )
                throw std::runtime_error("target not set up for output of data");

            std::shared_ptr<read_helper> helper = std::make_shared<read_helper>();
            async_streambuf_op<CharType, ReadHandler> handler_op(handler);
            std::shared_ptr<read_context> context(std::make_shared<read_context>(handler_op));
            requests_.insert(context);
            context->do_read_to_delim(delim, this, &target, helper);
        }

        /// Read until reaching a newline character. The newline is not included in the (target) an asynchronous stream buffer supporting write operations.
        /// Handler is called with count bytes read, if this number is 0 the end of the stream is reached.
        /// (handler) is the handler to be called when the read operation completes.
        /// Copies will be made of the handler as required. The function signature of the handler must be:
        /// void handler(size_t count) where count is the character count read or 0 if the end of the stream is reached.
        template<typename ReadHandler>
        void read_line(streams::async_streambuf<CharType, Impl> target, ReadHandler handler)
        {
            verify_and_throw(utils::s_in_streambuf_msg);
            if ( !target.can_write() )
                throw std::runtime_error("target not set up for receiving data");

            std::shared_ptr<read_helper> helper = std::make_shared<read_helper>();
            async_streambuf_op<CharType, ReadHandler> handler_op(handler);
            std::shared_ptr<read_context> context(std::make_shared<read_context>(handler_op));
            requests_.insert(context);
            context->do_read_line(this, &target, helper);
        }

        /// Read until reaching the end of the stream.
        /// Parameter (target) is an asynchronous stream buffer supporting write operations.
        /// (handler) is executed with the number of characters read as input parameter to the post function.
        /// Copies will be made of the handler as required. The function signature of the handler must be:
        /// void handler(size_t count) where count is the character count read or 0 if the end of the stream is reached.
        template<typename ReadHandler>
        void read_to_end(async_streambuf<CharType, Impl> target, ReadHandler handler)
        {
            verify_and_throw(utils::s_in_streambuf_msg);
            if ( !target.can_write() )
                throw std::runtime_error("target not set up for receiving data");

            std::shared_ptr<read_helper> helper = std::make_shared<read_helper>();
            async_streambuf_op<CharType, ReadHandler> handler_op(handler);
            std::shared_ptr<read_context> context(std::make_shared<read_context>(handler_op));
            requests_.insert(context);
            buffer_->getn(helper->outbuf, buf_size_, BIND_OBJ_HANDLER(&read_context::post_read_to_end, context.get(), this, target, helper));
        }

        /// Seeks to the specified write position (pos) an offset relative to the beginning of the stream.
        /// Returns the new position in the stream.
        pos_type seek(pos_type pos)
        {
            verify_and_throw(utils::s_in_stream_msg);
            return buffer_->seekpos(pos, std::ios_base::in);
        }

        /// Seeks to the specified write position with an offset (off) from starting point (way - beginning, current, end).
        /// Returns the new position in the stream.
        pos_type seek(off_type off, std::ios_base::seekdir way)
        {
            verify_and_throw(utils::s_in_stream_msg);
            return buffer_->seekoff(off, way, std::ios_base::in);
        }

        /// Get the current write position, i.e. the offset from the beginning of the stream.
        pos_type tell() const
        {
            verify_and_throw(utils::s_in_stream_msg);
            return buffer_->getpos(std::ios_base::in);
        }

        /// can_seek() is used to determine whether the stream supports seeking.
        /// Returns true if the stream supports seeking, false otherwise.
        bool can_seek() const { return is_valid() && buffer_->can_seek(); }

        /// Test whether the stream has been initialized with a valid stream buffer.
        bool is_valid() const { return (buffer_ != nullptr); }

        /// Test whether the stream has been initialized or not.
        operator bool() const { return is_valid(); }

        /// Test whether the stream is open for writing.
        /// Returns> true if the stream is open for writing, false otherwise.
        bool is_open() const { return is_valid() && buffer_->can_read(); }

        /// Get the underlying stream buffer.
        snode::streams::async_streambuf<CharType, Impl>& streambuf() const
        {
            return *buffer_;
        }

        /// Read a value of type T from the stream.
        /// Supports the C++ primitive types. Can be expanded to additional types
        /// by adding template specializations for type_parser.
        template<typename T, typename ReadHandler>
        void extract(ReadHandler handler)
        {
            // may add a future implementation with a type parser, goes undefind for now.
        }

    protected:
        static const size_t buf_size_ = 16*1024;

        struct read_helper
        {
            size_t total;
            CharType outbuf[buf_size_];
            size_t write_pos;

            bool is_full() const
            {
                return write_pos == buf_size_;
            }

            read_helper() : total(0), write_pos(0)
            {
            }
        };

        /// Internal completion handler to handle the internal streambuf async calls.
        class read_context : public std::enable_shared_from_this<read_context>
        {
        public:
            read_context(async_streambuf_op_base<CharType> op, size_t count = 0)
                : callback_op_(op), read_count_(count), eof_or_delim_(false)
            {
            }

            void post_read(size_t count, async_istream<CharType, Impl>* parent,
                           async_streambuf<CharType, Impl>* target, std::shared_ptr<CharType> buf)
            {
                if (count)
                {
                    CharType* data;
                    bool acquired = target->acquire(data, count);
                    // data is already written to target, just committing.
                    if (acquired)
                    {
                        target->commit(count);
                        on_read_complete(count, parent);
                    }
                    else if (buf)
                    {
                        target->putn(buf.get(), count, BIND_HANDLER(&read_context::on_read_complete, parent));
                    }
                }
            }

            void post_write(size_t count, async_istream<CharType, Impl>* parent)
            {
                CharType* data;
                parent->buffer_->acquire(data, count);
                if (data != nullptr)
                    parent->buffer_->release(data, count);

                // signal to parent, read operation is complete
                on_read_complete(count, parent);
            }

            void on_read_complete(size_t count, async_istream<CharType, Impl>* parent)
            {
                if (parent)
                {
                    read_count_ = count;
                    async_task::connect(&async_istream::post_read_complete, parent, this->shared_from_this());
                }
            }

            // delimiter read functions
            void do_read_to_delim(int_type delim, async_istream<CharType, Impl>* parent,
                                  async_streambuf<CharType, Impl>* target, std::shared_ptr<read_helper> helper)
            {
                while (parent->buffer_->in_avail() > 0)
                {
                    int_type ch = parent->buffer_->sbumpc();

                    int_type req_async = traits::requires_async();

                    if (ch == req_async)
                    {
                        parent->buffer_->bumpc(BIND_HANDLER(&read_context::post_read_delim, delim, parent, target, helper));
                        break;
                    }

                    if (ch == traits::eof() || ch == delim)
                    {
                        eof_or_delim_ = true;
                        target->putn(helper->outbuf, helper->write_pos, BIND_HANDLER(&read_context::post_write_delim, delim, parent, target, helper));
                        break;
                    }

                    helper->outbuf[helper->write_pos] = static_cast<CharType>(ch);
                    helper->write_pos += 1;

                    if (helper->is_full())
                    {
                        target->putn(helper->outbuf, helper->write_pos, BIND_HANDLER(&read_context::post_write_delim, delim, parent, target, helper));
                        break;
                    }
                }
            }

            void post_read_delim(int_type ch, int_type delim, async_istream<CharType, Impl>* parent,
                                 async_streambuf<CharType, Impl>* target, std::shared_ptr<read_helper> helper)
            {
                if (ch == traits::eof() || ch == delim)
                {
                    eof_or_delim_ = true;
                    target->putn(helper->outbuf, helper->write_pos, BIND_HANDLER(&read_context::post_write_delim, delim, parent, target, helper));
                }
                else
                {
                    helper->outbuf[helper->write_pos] = static_cast<CharType>(ch);
                    helper->write_pos += 1;

                    if (helper->is_full())
                        target->putn(helper->outbuf, helper->write_pos, BIND_HANDLER(&read_context::post_write_delim, delim, parent, target, helper));
                    else
                        do_read_to_delim(delim, parent, *target, helper);
                }
            }

            void post_write_delim(size_t count, int_type delim, async_istream<CharType, Impl>* parent,
                                  async_streambuf<CharType, Impl>* target, std::shared_ptr<read_helper> helper)
            {
                if (count)
                {
                    // flush to target buffer
                    helper->total += count;
                    helper->write_pos = 0;
                    target->sync();

                    if (eof_or_delim_)
                    {
                        read_count_ = count;
                        async_task::connect(&async_istream::post_read_complete, parent, this->shared_from_this());
                    }
                    else
                    {
                        do_read_to_delim(delim, parent, target, helper);
                    }
                }
            }

            // read line functions
            void do_read_line(async_istream<CharType, Impl>* parent,
                              async_streambuf<CharType, Impl>* target, std::shared_ptr<read_helper> helper)
            {
                int_type req_async = traits::requires_async();

                while (parent->buffer_->in_avail() > 0)
                {
                    int_type ch = this->parent_.buffer_->sbumpc();

                    if (ch == req_async)
                    {
                        parent->buffer_->bumpc(BIND_HANDLER(&read_context::post_read_line, parent, target, helper));
                        break;
                    }

                    if (ch == traits::eof())
                    {
                        eof_or_delim_ = true;
                        target->putn(helper->outbuf, helper->write_pos, BIND_HANDLER(&read_context::post_write_line, parent, target, helper));
                        break;
                    }

                    if (ch == '\r')
                    {
                        update_after_cr(parent, target, helper);
                    }
                    else
                    {
                        helper->outbuf[helper->write_pos] = static_cast<CharType>(ch);
                        helper->write_pos += 1;

                        if (helper->is_full())
                        {
                            target->putn(helper->outbuf, helper->write_pos, BIND_HANDLER(&read_context::post_write_line, parent, target, helper));
                            break;
                        }
                    }
                }
            }

            void post_read_line(int_type ch, async_istream<CharType, Impl>* parent,
                                async_streambuf<CharType, Impl>* target, std::shared_ptr<read_helper> helper)
            {
                if (ch == traits::eof())
                {
                    eof_or_delim_ = true;
                    target->putn(helper->outbuf, helper->write_pos, BIND_HANDLER(&read_context::post_write_line, parent, target, helper));
                }
                else
                {
                    if (ch == '\r')
                    {
                        update_after_cr(parent, target, helper);
                    }
                    else
                    {
                        helper->outbuf[helper->write_pos] = static_cast<CharType>(ch);
                        helper->write_pos += 1;

                        if (helper->is_full())
                            target->putn(helper->outbuf, helper->write_pos, BIND_HANDLER(&read_context::post_write_line, parent, target, helper));
                        else
                            do_read_line(parent, target, helper);
                    }
                }
            }

            void post_write_line(size_t count, async_istream<CharType, Impl>* parent,
                                 async_streambuf<CharType, Impl>* target, std::shared_ptr<read_helper> helper)
            {
                if (count)
                {
                    // flush to target buffer
                    helper->total += count;
                    helper->write_pos = 0;
                    target->sync();

                    if (eof_or_delim_)
                    {
                        read_count_ = count;
                        async_task::connect(&async_istream::post_read_complete, parent, this->shared_from_this());
                    }
                    else
                    {
                        do_read_line(parent, target, helper);
                    }
                }
            }

            void update_after_cr(async_istream<CharType, Impl>* parent,
                                 async_streambuf<CharType, Impl>* target, std::shared_ptr<read_helper> helper)
            {
                int_type ch = parent->buffer_->sgetc();
                if (ch == '\n')
                    parent->buffer_->sbumpc();

                eof_or_delim_ = true;
                target->putn(helper->outbuf, helper->write_pos, BIND_HANDLER(&read_context::post_write_line, parent, target, helper));
            }

            // read to end functions
            void post_read_to_end(size_t count, async_istream<CharType, Impl>* parent,
                                  async_streambuf<CharType, Impl>* target, std::shared_ptr<read_helper> helper)
            {
                if (count)
                {
                    target->putn(helper->outbuf, helper->write_pos, BIND_HANDLER(&read_context::post_write_to_end, parent, target, helper));
                }
                else
                {
                    read_count_ = helper->total;;
                    async_task::connect(&async_istream::post_read_complete, parent, this->shared_from_this());
                }
            }

            void post_write_to_end(size_t count, async_istream<CharType, Impl>* parent,
                                   async_streambuf<CharType, Impl>* target, std::shared_ptr<read_helper> helper)
            {
                if (count)
                {
                    // flush to target buffer
                    helper->total += count;
                    target->sync();
                    parent->buffer_->getn(helper->outbuf, buf_size_, BIND_HANDLER(&read_context::post_read_to_end, parent, target, helper));
                }
                else
                {
                    read_count_ = helper->total;
                    async_task::connect(&async_istream::post_read_complete, parent, this->shared_from_this());
                }
            }

            size_t read_count_;
            async_streambuf_op_base<CharType> callback_op_;
            bool eof_or_delim_;
        };

        friend class read_context;
        void post_read_complete(std::shared_ptr<read_context> context)
        {
            context->callback_op_(context->read_count_);
            requests_.erase(context);
        }

        void verify_and_throw(const char *msg) const
        {
            if ( buffer_->exception() != nullptr )
                std::rethrow_exception(buffer_->exception());
            if ( !buffer_->can_read() )
                throw std::runtime_error(msg);
        }

        std::set<std::shared_ptr<read_context>> requests_;
        async_streambuf<CharType, Impl>* buffer_;
    };
}
}
#endif /* ASYNC_STREAMS_H_ */
