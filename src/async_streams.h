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
        {}

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

    protected:
        // The in/out mode for the buffer
        bool stream_can_read_, stream_can_write_, stream_read_eof_, alloced_;

        /// Get the internal implementation of the async_streambuf core functions.
        /// Returns the object holding the current implementation.
        inline Impl* get_impl()
        {
            return static_cast<Impl*>(this);
        }

        inline const Impl* get_impl() const
        {
            return static_cast<const Impl*>(this);
        }

        /// The real read head close operation,
        /// implementation should override it if there is any resource to be released.
        void close_read()
        {
            stream_can_read_ = false;
        }

        /// The real write head close operation,
        /// implementation should override it if there is any resource to be released.
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

    public:
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
        size_t in_avail() const
        {
            return get_impl()->in_avail_impl();
        }

        /// Gets the current read or write position in the stream for the given (direction).
        /// Returns the current position. EOF if the operation fails.
        /// Some streams may have separate write and read cursors.
        /// For such streams, the direction parameter defines whether to move the read or the write cursor.</remarks>
        pos_type getpos(std::ios_base::openmode direction) const
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
        /// Some streams may have separate write and read cursors. For such streams the direction parameter defines whether to move the read or the write cursor.
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
                async_task::connect(handler, traits::eof());
            else
                get_impl()->putc_impl(ch, handler);
        }

        /// Writes a number (count) of characters to the stream buffer from source memory (ptr),
        /// the contents of the memory pointed by ptr is copied not the pointer itself.
        /// (handler) is the handler to be called when the write operation completes.
        /// Copies will be made of the handler as required. The function signature of the handler must be:
        /// void handler(size_t count) where count is the byte count written or 0 if the write operation failed.
        template<typename WriteHandler>
        void putn(const CharType* ptr, size_t count, WriteHandler handler)
        {
            if (count)
            {
                if (!can_write())
                    async_task::connect(handler, 0);
                else
                    get_impl()->putn_impl(ptr, count, handler);
            }
        }

        /// Writes a number (count) of characters to the stream from source memory (ptr).
        /// Note: callers must make sure the data to be written is valid until the returned task completes.
        /// (handler) is the handler to be called when the write operation completes.
        /// Copies will be made of the handler as required. The function signature of the handler must be:
        /// void handler(size_t count) where count is the byte count written or 0 if the write operation failed.
        template<typename WriteHandler>
        void putn_nocopy(const CharType* ptr, size_t count, WriteHandler handler)
        {
            if (count)
            {
                if (!can_write())
                    async_task::connect(handler, 0);
                else
                    get_impl()->putn_nocopy(ptr, count);
            }
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
                async_task::connect(handler, traits::eof());
            else
                get_impl()->getc_impl(handler);
        }

        /// Reads a single character from the stream without advancing the read position.
        /// Returns the value of the character. EOF if the read fails, check requires_async() method if an asynchronous read is required.
        /// This is a synchronous operation, but is guaranteed to never block.
        int_type sgetc()
        {
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
            if (count)
            {
                if (!can_read())
                    async_task::connect(handler, 0);
                else
                    get_impl()->getn_impl(ptr, count, handler);
            }
        }

        /// Copies up to a given number (count) of characters from the stream to memory (ptr) synchronously.
        /// Returns the number of characters copied. 0 if the end of the stream is reached or an asynchronous read is required.
        /// This is a synchronous operation, but is guaranteed to never block.
        size_t scopy(CharType* ptr, size_t count)
        {
            if (!can_read())
                return 0;
            return get_impl()->scopy_impl(ptr, count);
        }

        /// For output streams, flush any internally buffered data to the underlying medium.
        void sync()
        {
            if (!can_write())
                throw std::runtime_error(utils::s_out_streambuf_msg);
            get_impl()->sync_impl();
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
        /// Throws exception if buffer is already allocated
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
        /// Throws exception if buffer is not allocated
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
    };

    /// Base class for all asynchronous output streams.
    /// Perform stream associated write operations.
    /// Template argument Impl is the implementation of the stream buffer associated with the stream.
    template<typename CharType, typename Impl>
    class async_ostream
    {
    protected:

        void verify_and_throw(const char* msg) const
        {
            if ( !buffer_->can_write() )
                throw std::runtime_error(msg);
        }

        /// async_ostream's write completion handler to be executed
        /// when a write operation on the underlying buffer is complete.
        /// Template parameter CompletionHandler is the user supplied handler, to be executed
        /// when write() to the async_ostream object is complete.
        template<typename CompletionHandler>
        struct streambuf_write_handler
        {
            streambuf_write_handler(CompletionHandler handler) : handler_(handler)
            {}

            template<typename SourceBuffImpl>
            void on_write(size_t count, async_streambuf<CharType, SourceBuffImpl>* source)
            {
                CharType* data;
                source->acquire(data, count);
                if (data != nullptr)
                    source->release(data, count);
                // execute user's completion handler
                handler_(count);
            }
            // user supplied completion handler
            CompletionHandler handler_;
        };

        /// async_ostream's read completion handler to be executed
        /// when a read operation on a source buffer is complete.
        /// Template parameter CompletionHandler is the user supplied handler, to be executed
        /// when write() to the async_ostream object is complete.
        template<typename CompletionHandler>
        struct streambuf_read_handler
        {
            streambuf_read_handler(CompletionHandler handler) : handler_(handler)
            {}

            void on_read(size_t count, async_ostream<CharType, Impl>* ostream, std::shared_ptr<CharType> buf)
            {
                if (count)
                {
                    CharType* data;
                    const bool acquired = ostream->streambuf()->acquire(data, count);
                    // data is already allocated in the source, just committing.
                    if (acquired)
                    {
                        ostream->streambuf()->commit(count);
                        handler_(count);
                    }
                    else if (buf)
                    {
                        ostream->streambuf()->putn(buf.get(), count,
                                std::bind(&streambuf_read_handler<CompletionHandler>::on_write, *this, count, buf));
                    }
                }
            }

            void on_write(size_t count, std::shared_ptr<CharType> buf)
            {
                // execute user's completion handler and release buf
                handler_(count);
            }
            // user supplied completion handler
            CompletionHandler handler_;
        };

        /// stream buffer object associated with this stream
        async_streambuf<CharType, Impl>* buffer_;

    public:
        typedef ::snode::streams::char_traits<CharType> traits;
        typedef typename traits::int_type int_type;
        typedef typename traits::pos_type pos_type;
        typedef typename traits::off_type off_type;
        typedef typename ::snode::streams::async_streambuf<CharType, Impl> streambuf_type;

        /// Default constructor
        async_ostream() : buffer_(nullptr)
        {}

        /// Copy constructor
        async_ostream(const async_ostream<CharType, Impl>& other) : buffer_(other.buffer_)
        {}

        /// Assignment operator
        async_ostream& operator =(const async_ostream<CharType, Impl>& other)
        {
            buffer_ = other.buffer_;
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

            auto data = buffer_->alloc(count);
            if ( data != nullptr )
            {
                streambuf_read_handler<WriteHandler> completion_handler(handler);
                source.getn(data, count,
                        std::bind(&streambuf_read_handler<WriteHandler>::on_read, completion_handler, std::placeholders::_1, this, nullptr));
            }
            else
            {
                size_t available = 0;
                const bool acquired = source.acquire(data, available);
                if ( available >= count )
                {
                    streambuf_write_handler<WriteHandler> completion_handler(handler);
                    buffer_->putn(data, count,
                            std::bind(&streambuf_write_handler<WriteHandler>::on_write, completion_handler, std::placeholders::_1, source));
                }
                else
                {
                    // Always have to release if acquire returned true.
                    if (acquired)
                        source.release(data, 0);

                    auto buf = std::make_shared<CharType>((size_t)count);
                    streambuf_read_handler<WriteHandler> completion_handler(handler);
                    source.getn(buf.get(), count,
                            std::bind(&streambuf_read_handler<WriteHandler>::on_read, completion_handler, std::placeholders::_1, this, buf));
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
    };

    /// Base class for all asynchronous input streams.
    /// Perform stream associated read operations.
    /// Template argument Impl is the implementation of the stream buffer associated with the stream.
    template<typename CharType, typename Impl>
    class async_istream
    {
    public:
        typedef ::snode::streams::char_traits<CharType> traits;
        typedef typename traits::int_type int_type;
        typedef typename traits::pos_type pos_type;
        typedef typename traits::off_type off_type;

    protected:
        void verify_and_throw(const char *msg) const
        {
            if ( !buffer_->can_read() )
                throw std::runtime_error(msg);
        }

        static const size_t buf_size_ = 16*1024;
        struct read_helper
        {
            read_helper() : total(0), write_pos(0)
            {}

            size_t total;
            CharType outbuf[buf_size_];
            size_t write_pos;

            bool is_full() const { return write_pos == buf_size_; }
        };

        /// stream buffer object associated with this stream
        async_streambuf<CharType, Impl>* buffer_;

        /// async_istream's write completion handler to be executed
        /// when a write operation on a target buffer is complete.
        /// Template parameter CompletionHandler is the user supplied handler, to be executed
        /// when read() to the async_istream object is complete.
        template<typename CompletionHandler>
        struct streambuf_write_handler
        {
            streambuf_write_handler(CompletionHandler handler) : handler_(handler)
            {}

            void on_write(size_t count, async_istream<CharType, Impl>* istream)
            {
                CharType* data;
                istream->streambuf().acquire(data, count);
                if (data != nullptr)
                    istream->streambuf().release(data, count);
                // execute user's completion handler
                handler_(count);
            }
            // user supplied completion handler
            CompletionHandler handler_;
        };

        /// async_istream's read completion handler to be executed when a read operation on the underlying buffer is complete.
        /// Template parameter TargetBuffImpl is the (target) destination buffer implementation where data will be stored.
        /// Template parameter CompletionHandler is the user supplied (handler) to be executed
        /// when read() to the async_istream object is complete.
        template<typename CompletionHandler, typename TargetBuffImpl>
        struct streambuf_read_handler
        {
            streambuf_read_handler(CompletionHandler handler, async_streambuf<CharType, TargetBuffImpl>& target)
             : handler_(handler), target_(&target)
            {}

            void on_read(size_t count, std::shared_ptr<CharType> buf)
            {
                if (count)
                {
                    CharType* data;
                    bool acquired = target_->acquire(data, count);
                    // data is already written to target, just committing.
                    if (acquired)
                    {
                        target_->commit(count);
                        handler_(count);
                    }
                    else if (buf)
                    {
                        target_->putn(buf.get(), count,
                          std::bind(&streambuf_read_handler<CompletionHandler, TargetBuffImpl>::on_write, *this, count, buf));
                    }
                }
            }

            void on_write(size_t count, std::shared_ptr<CharType> buf)
            {
                // execute user's completion handler and release buf
                handler_(count);
            }

            // user supplied completion handler
            CompletionHandler handler_;
            // destination buffer
            async_streambuf<CharType, TargetBuffImpl>* target_;
        };

        /// read_to_delim() internal implementation, encapsulates all asynchronous logic and streambuf function calls
        /// performed in the background.
        /// Template parameter TargetBuffImpl is the (target) destination buffer implementation where data will be stored.
        /// Template parameter CompletionHandler is the user supplied handler to be executed
        /// when the operation is complete.
        template<typename CompletionHandler, typename TargetBuffImpl>
        struct delim_read_impl
        {
            delim_read_impl(CompletionHandler handler,
                            async_streambuf<CharType, Impl>& source,
                            async_streambuf<CharType, TargetBuffImpl>& target)
              : eof_or_delim_(false), handler_(handler), helper_(std::make_shared<read_helper>()),
                source_(&source), target_(&target)
            {}

            void read_to_delim(int_type delim)
            {
                if (!source_->in_avail())
                {
                    source_->bumpc(std::bind(&delim_read_impl<CompletionHandler, TargetBuffImpl>::on_read, *this, std::placeholders::_1, delim));
                }
                else
                {
                    int_type req_async = traits::requires_async();

                    while (source_->in_avail() > 0)
                    {
                        int_type ch = source_->sbumpc();
                        if (ch == req_async)
                        {
                            source_->bumpc(std::bind(&delim_read_impl<CompletionHandler, TargetBuffImpl>::on_read, *this, std::placeholders::_1, delim));
                            break;
                        }

                        if (ch == traits::eof() || ch == delim)
                        {
                            eof_or_delim_ = true;
                            target_->putn(helper_->outbuf, helper_->write_pos,
                              std::bind(&delim_read_impl<CompletionHandler, TargetBuffImpl>::on_write, *this, std::placeholders::_1, delim));
                            break;
                        }

                        helper_->outbuf[helper_->write_pos] = static_cast<CharType>(ch);
                        helper_->write_pos += 1;

                        if (helper_->is_full())
                        {
                            target_->putn(helper_->outbuf, helper_->write_pos,
                              std::bind(&delim_read_impl<CompletionHandler, TargetBuffImpl>::on_write, *this, std::placeholders::_1, delim));
                            break;
                        }
                    }
                }
            }

            void on_read(int_type ch, int_type delim)
            {
                if (ch == traits::eof() || ch == delim)
                {
                    eof_or_delim_ = true;
                    target_->putn(helper_->outbuf, helper_->write_pos,
                      std::bind(&delim_read_impl<CompletionHandler, TargetBuffImpl>::on_write, *this, std::placeholders::_1, delim));
                }
                else
                {
                    helper_->outbuf[helper_->write_pos] = static_cast<CharType>(ch);
                    helper_->write_pos += 1;

                    if (helper_->is_full())
                    {
                        target_->putn(helper_->outbuf, helper_->write_pos,
                          std::bind(&delim_read_impl<CompletionHandler, TargetBuffImpl>::on_write, *this, std::placeholders::_1, delim));
                    }
                    else
                    {
                        this->read_to_delim(delim);
                    }
                }
            }

            void on_write(size_t count, int_type delim)
            {
                if (count)
                {
                    // flush to target buffer
                    helper_->total += count;
                    helper_->write_pos = 0;
                    target_->sync();

                    if (eof_or_delim_)
                        handler_(count);
                    else
                        this->read_to_delim(delim);
                }
                else
                {
                    handler_(helper_->total);
                }
            }

            bool eof_or_delim_;
            // user supplied completion handler
            CompletionHandler handler_;
            std::shared_ptr<read_helper> helper_;
            // source buffer
            async_streambuf<CharType, Impl>* source_;
            // destination buffer
            async_streambuf<CharType, TargetBuffImpl>* target_;
        };

        /// read_line() internal implementation, encapsulates all the asynchronous logic and streambuf function calls
        /// performed in the background.
        /// Template parameter TargetBuffImpl is the (target) destination buffer implementation where data will be stored.
        /// Template parameter CompletionHandler is the user supplied handler to be executed
        /// when the operation is complete.
        template<typename CompletionHandler, typename TargetBuffImpl>
        struct line_read_impl
        {
            line_read_impl(CompletionHandler handler,
                           async_streambuf<CharType, Impl>& source,
                           async_streambuf<CharType, TargetBuffImpl>& target)
                : eof_or_crlf_(false), handler_(handler), helper_(std::make_shared<read_helper>()),
                  source_(&source), target_(&target)
            {}

            void read_line()
            {
                if (!source_->in_avail())
                {
                    source_->bumpc(std::bind(&line_read_impl<CompletionHandler, TargetBuffImpl>::on_read, *this, std::placeholders::_1));
                }
                else
                {
                    int_type req_async = traits::requires_async();

                    while (source_->in_avail() > 0)
                    {
                        int_type ch = source_->sbumpc();
                        if (ch == req_async)
                        {
                            source_->bumpc(std::bind(&line_read_impl<CompletionHandler, TargetBuffImpl>::on_read, *this, std::placeholders::_1));
                            break;
                        }

                        if (traits::eof() == ch)
                        {
                            eof_or_crlf_ = true;
                            target_->putn(helper_->outbuf, helper_->write_pos,
                              std::bind(&line_read_impl<CompletionHandler, TargetBuffImpl>::on_write, *this, std::placeholders::_1));
                            break;
                        }

                        if (ch == '\r')
                        {
                            update_after_cr();
                        }
                        else
                        {
                            helper_->outbuf[helper_->write_pos] = static_cast<CharType>(ch);
                            helper_->write_pos += 1;

                            if (helper_->is_full())
                            {
                                target_->putn(helper_->outbuf, helper_->write_pos,
                                  std::bind(&line_read_impl<CompletionHandler, TargetBuffImpl>::on_write, *this, std::placeholders::_1));
                                break;
                            }
                        }
                    }
                }
            }

            void on_read(int_type ch)
            {
                if (ch == traits::eof())
                {
                    eof_or_crlf_ = true;
                    target_->putn(helper_->outbuf, helper_->write_pos,
                      std::bind(&line_read_impl<CompletionHandler, TargetBuffImpl>::on_write, *this, std::placeholders::_1));
                }
                else
                {
                    if (ch == '\r')
                    {
                        update_after_cr();
                    }
                    else
                    {
                        helper_->outbuf[helper_->write_pos] = static_cast<CharType>(ch);
                        helper_->write_pos += 1;

                        if (helper_->is_full())
                        {
                            target_->putn(helper_->outbuf, helper_->write_pos,
                              std::bind(&line_read_impl<CompletionHandler, TargetBuffImpl>::on_write, *this, std::placeholders::_1));
                        }
                        else
                        {
                            this->read_line();
                        }
                    }
                }
            }

            void on_write(size_t count)
            {
                if (count)
                {
                    // flush to target buffer
                    helper_->total += count;
                    helper_->write_pos = 0;
                    target_->sync();

                    if (eof_or_crlf_)
                        handler_(count);
                    else
                        this->read_line();
                }
                else
                {
                    handler_(helper_->total);
                }
            }

            void update_after_cr()
            {
                int_type ch = source_->sgetc();
                if (ch == '\n')
                    source_->sbumpc();

                eof_or_crlf_ = true;
                target_->putn(helper_->outbuf, helper_->write_pos,
                  std::bind(&line_read_impl<CompletionHandler, TargetBuffImpl>::on_write, *this, std::placeholders::_1));
            }

            bool eof_or_crlf_;
            // user supplied completion handler
            CompletionHandler handler_;
            std::shared_ptr<read_helper> helper_;
            // source buffer
            async_streambuf<CharType, Impl>* source_;
            // destination buffer
            async_streambuf<CharType, TargetBuffImpl>* target_;
        };

        /// read_to_end() internal implementation, encapsulates all asynchronous logic and streambuf function calls
        /// performed in the background.
        /// Template parameter TargetBuffImpl is the (target) destination buffer implementation where data will be stored.
        /// Template parameter CompletionHandler is the user supplied handler to be executed
        /// when the operation is complete.
        template<typename CompletionHandler, typename TargetBuffImpl>
        struct end_read_impl
        {
            end_read_impl(CompletionHandler handler, async_streambuf<CharType, Impl>& source,
                          async_streambuf<CharType, TargetBuffImpl>& target)
                : handler_(handler), helper_(std::make_shared<read_helper>()), source_(&source), target_(&target)
            {}

            void read_to_end()
            {
                source_->getn(helper_->outbuf, buf_size_,
                  std::bind(&end_read_impl<CompletionHandler, TargetBuffImpl>::on_read, *this, std::placeholders::_1));
            }

            void on_read(size_t count)
            {
                if (count)
                {
                    target_->putn(helper_->outbuf, count,
                      std::bind(&end_read_impl<CompletionHandler, TargetBuffImpl>::on_write, *this, std::placeholders::_1));
                }
                else
                {
                    handler_(helper_->total);
                }
            }

            void on_write(size_t count)
            {
                if (count)
                {
                    // flush to target buffer
                    helper_->total += count;
                    target_->sync();
                    source_->getn(helper_->outbuf, buf_size_,
                      std::bind(&end_read_impl<CompletionHandler, TargetBuffImpl>::on_read, *this, std::placeholders::_1));
                }
                else
                {
                    handler_(helper_->total);
                }
            }

            // user supplied completion handler
            CompletionHandler handler_;
            std::shared_ptr<read_helper> helper_;
            // source buffer
            async_streambuf<CharType, Impl>* source_;
            // destination buffer
            async_streambuf<CharType, TargetBuffImpl>* target_;
        };

    public:
        /// Default constructor
        async_istream() : buffer_(nullptr)
        {}

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
        /// (handler) is the handler to be called when the read operation completes.
        /// Copies will be made of the handler as required. The function signature of the handler must be:
        /// void handler(size_t count) where count is the byte count read or 0 if the read operation failed.
        template<typename ReadHandler, typename BufferImpl>
        void read(async_streambuf<CharType, BufferImpl>& target, size_t count, ReadHandler handler)
        {
            verify_and_throw(utils::s_in_streambuf_msg);
            if ( !target.can_write() )
                throw std::runtime_error("target not set up for output of data");

            if (count == 0)
                return;

            auto data = target.alloc(count);
            if ( data != nullptr )
            {
                streambuf_read_handler<ReadHandler, BufferImpl> completion_handler(handler, target);
                buffer_->getn(data, count,
                  std::bind(&streambuf_read_handler<ReadHandler, BufferImpl>::on_read, completion_handler, std::placeholders::_1, nullptr));
            }
            else
            {
                size_t available = 0;
                const bool acquired = buffer_->acquire(data, available);
                if (available >= count)
                {
                    streambuf_write_handler<ReadHandler> completion_handler(handler);
                    target.putn(data, count,
                      std::bind(&streambuf_write_handler<ReadHandler>::on_write, completion_handler, std::placeholders::_1, this));
                }
                else
                {
                    // Always have to release if acquire returned true.
                    if (acquired)
                        buffer_->release(data, 0);

                    auto buf = std::make_shared<CharType>((size_t)count);
                    streambuf_read_handler<ReadHandler, BufferImpl> completion_handler(handler, target);
                    buffer_->getn(buf.get(), count,
                      std::bind(&streambuf_read_handler<ReadHandler, BufferImpl>::on_read, completion_handler, std::placeholders::_1, buf));
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
        template<typename ReadHandler, typename BufferImpl>
        void read_to_delim(async_streambuf<CharType, BufferImpl>& target, int_type delim, ReadHandler handler)
        {
            verify_and_throw(utils::s_in_streambuf_msg);
            if ( !target.can_write() )
                throw std::runtime_error("target not set up for output of data");

            delim_read_impl<ReadHandler, BufferImpl> impl(handler, this->streambuf(), target);
            impl.read_to_delim(delim);
        }

        /// Read until reaching a newline character. The newline is not included in the (target) an asynchronous stream buffer supporting write operations.
        /// (handler) is the handler to be called when the read operation completes.
        /// Copies will be made of the handler as required. The function signature of the handler must be:
        /// void handler(size_t count) where count is the character count read or 0 if the end of the stream is reached.
        template<typename ReadHandler, typename BufferImpl>
        void read_line(streams::async_streambuf<CharType, BufferImpl>& target, ReadHandler handler)
        {
            verify_and_throw(utils::s_in_streambuf_msg);
            if ( !target.can_write() )
                throw std::runtime_error("target not set up for receiving data");

            line_read_impl<ReadHandler, BufferImpl> impl(handler, this->streambuf(), target);
            impl.read_line();
        }

        /// Read until reaching the end of the stream.
        /// Parameter (target) is an asynchronous stream buffer supporting write operations.
        /// (handler) is executed with the number of characters read as input parameter to the post function.
        /// Copies will be made of the handler as required. The function signature of the handler must be:
        /// void handler(size_t count) where count is the character count read or 0 if the end of the stream is reached.
        template<typename ReadHandler, typename BufferImpl>
        void read_to_end(async_streambuf<CharType, BufferImpl>& target, ReadHandler handler)
        {
            verify_and_throw(utils::s_in_streambuf_msg);
            if ( !target.can_write() )
                throw std::runtime_error("target not set up for receiving data");

            end_read_impl<ReadHandler, BufferImpl> impl(handler, this->streambuf(), target);
            impl.read_to_end();
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
    };
}
}
#endif /* ASYNC_STREAMS_H_ */
