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
    /// TChar - The data type of the basic element of the stream.
    template<typename TChar>
    struct char_traits : std::char_traits<TChar>
    {
        /// Some synchronous functions will return this value if the operation requires an asynchronous call in a given situation.
        /// Returns an int_type value which implies that an asynchronous call is required.
        static typename std::char_traits<TChar>::int_type requires_async()
        {
            return std::char_traits<TChar>::eof()-1;
        }
    };

    namespace utils
    {
        /// Value to string formatters
        template <typename TChar>
        struct value_string_formatter
        {
            template <typename T>
            static std::basic_string<TChar> format(const T &val)
            {
                std::basic_ostringstream<TChar> ss;
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
    template<typename TChar>
    class async_streambuf_op_base
    {
    public:
        typedef snode::streams::char_traits<TChar> traits;
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
    template<typename TChar, typename Handler>
    class async_streambuf_op : public async_streambuf_op_base<TChar>
    {
    public:
        typedef async_streambuf_op_base<TChar> streambuf_op_base;
        typedef snode::streams::char_traits<TChar> traits;
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
    template<typename TBuf> class async_istream;
    template<typename TBuf> class async_ostream;


    /// Asynchronous stream buffer base class.
    /// TChar is the data type of the basic element of the async_streambuf
    /// and TImpl is the actual buffer internal implementation.
    template<typename TChar, typename TImpl>
    class async_streambuf : public std::enable_shared_from_this<async_streambuf<TChar,TImpl> >
    {
    public:
        typedef TChar char_type;
        typedef streams::char_traits<TChar> traits;
        typedef typename traits::int_type int_type;
        typedef typename traits::pos_type pos_type;
        typedef typename traits::off_type off_type;
        typedef async_ostream<async_streambuf<TChar,TImpl> > ostream_type;
        typedef async_istream<async_streambuf<TChar,TImpl> > istream_type;

    protected:
        // The in/out mode for the buffer
        bool stream_can_read_, stream_can_write_, stream_read_eof_, alloced_;

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

        /// Get the internal implementation of the async_streambuf core functions.
        /// Returns the object holding the current implementation.
        inline TImpl* get_impl()
        {
            return static_cast<TImpl*>(this);
        }

        inline const TImpl* get_impl() const
        {
            return static_cast<const TImpl*>(this);
        }

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
            return istream_type(this->shared_from_this());
        }

        /// Constructs an output stream for this stream buffer.
        ostream_type create_ostream()
        {
            if (!can_write())
                throw std::runtime_error("stream buffer not set up for output of data");
            return ostream_type(this->shared_from_this());
        }

        /// can_seek() is used to determine whether a stream buffer supports seeking.
        bool can_seek() const
        {
            return get_impl()->can_seek();
        }

        /// has_size() is used to determine whether a stream buffer supports size().
        bool has_size() const
        {
            return get_impl()->has_size();
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
            if (has_size())
                return get_impl()->buffer_size(direction);
            return 0;
        }

        /// Sets the stream buffer implementation to buffer (size) or not buffer for the given direction (in or out).
        void set_buffer_size(size_t size, std::ios_base::openmode direction = std::ios_base::in)
        {
            if (has_size())
                get_impl()->set_buffer_size(size, direction);
        }

        /// For any input stream, returns the number of characters that are immediately available to be consumed without blocking
        /// May be used in conjunction with sbumpc() method to read data without using async tasks.
        size_t in_avail() const
        {
            return get_impl()->in_avail();
        }

        /// Gets the current read or write position in the stream for the given (direction).
        /// Returns the current position. EOF if the operation fails.
        /// Some streams may have separate write and read cursors.
        /// For such streams, the direction parameter defines whether to move the read or the write cursor.
        pos_type getpos(std::ios_base::openmode direction) const
        {
            if (can_seek())
                return get_impl()->getpos(direction);
            else
                return traits::eof();
        }

        /// Gets the size (count characters) of the stream, if known.
        /// Calls to has_size() will determine whether the result of size can be relied on.
        size_t size() const
        {
            if (has_size())
                return get_impl()->size();
            else
                return 0;
        }

        /// Seeks to the given position (pos is offset from beginning of the stream) for the given (direction).
        /// Returns the position. EOF if the operation fails.
        /// Some streams may have separate write and read cursors.
        /// For such streams the direction parameter defines whether to move the read or the write cursor.
        pos_type seekpos(pos_type pos, std::ios_base::openmode direction)
        {
            if (can_seek())
                return get_impl()->seekpos(pos, direction);
            return traits::eof();
        }

        /// Seeks to a position given by a relative (offset), 
        /// with starting point (way beginning, end, current) for the seek and with I/O direction (pos in/out).
        /// Returns the position. EOF if the operation fails.
        /// Some streams may have separate write and read cursors and for such streams,
        /// the mode parameter defines whether to move the read or the write cursor.
        pos_type seekoff(off_type offset, std::ios_base::seekdir way, std::ios_base::openmode mode)
        {
            if (can_seek())
                return get_impl()->seekof(offset, way, mode);
            return traits::eof();
        }

        /// Closes the stream buffer for the I/O mode (in or out), preventing further read or write operations.
        void close(std::ios_base::openmode mode = std::ios_base::in | std::ios_base::out)
        {
            if ((mode & std::ios_base::in) && can_read())
            {
                close_read();
                get_impl()->close_read();
            }

            if ((mode & std::ios_base::out) && can_write())
            {
                close_write();
                get_impl()->close_write();
            }
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
        template<typename THandler>
        void putc(char_type ch, THandler handler)
        {
            if (!can_write())
                async_task::connect(handler, traits::eof());
            else
                get_impl()->putc(ch, handler);
        }

        /// Writes a number (count) of characters to the stream buffer from source memory (ptr),
        /// the contents of the memory pointed by ptr is copied not the pointer itself.
        /// (handler) is the handler to be called when the write operation completes.
        /// Copies will be made of the handler as required. The function signature of the handler must be:
        /// void handler(size_t count) where count is the byte count written or 0 if the write operation failed.
        template<typename THandler>
        void putn(const char_type* ptr, size_t count, THandler handler)
        {
            if (count)
            {
                if (!can_write())
                    async_task::connect(handler, 0);
                else
                    get_impl()->putn(ptr, count, handler);
            }
        }

        /// Writes a number (count) of characters to the stream from source memory (ptr).
        /// Note: callers must make sure the data to be written is valid until the returned task completes.
        /// (handler) is the handler to be called when the write operation completes.
        /// Copies will be made of the handler as required. The function signature of the handler must be:
        /// void handler(size_t count) where count is the byte count written or 0 if the write operation failed.
        template<typename THandler>
        void putn_nocopy(const char_type* ptr, size_t count, THandler handler)
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
        template<typename THandler>
        void bumpc(THandler handler)
        {
            if (!can_read())
            	async_task::connect(handler, traits::eof());
            else
            	get_impl()->bumpc(handler);
        }

        /// Reads a single character from the stream and advances the read position.
        /// Returns the value of the character, (-1) if the read fails. (-2) if an asynchronous read is required
        /// This is a synchronous operation, but is guaranteed to never block.
        int_type sbumpc()
        {
            if (!can_read())
                return traits::eof();
            return check_sync_read_eof(get_impl()->sbumpc());
        }

        /// Reads a single character from the stream without advancing the read position.
        /// (handler) is the handler to be called when the read operation completes.
        /// Copies will be made of the handler as required. The function signature of the handler must be:
        /// void handler(int_type ch) where ch is character value (EOF if the read operation failed).
        template<typename THandler>
        void getc(THandler handler)
        {
            if (!can_read())
                async_task::connect(handler, traits::eof());
            else
                get_impl()->getc(handler);
        }

        /// Reads a single character from the stream without advancing the read position.
        /// Returns the value of the character. EOF if the read fails, check requires_async() method if an asynchronous read is required.
        /// This is a synchronous operation, but is guaranteed to never block.
        int_type sgetc()
        {
            if (!can_read())
                return traits::eof();
            return check_sync_read_eof(get_impl()->sgetc());
        }

        /// Advances the read position, then returns the next character without advancing again.
        /// (handler) is the handler to be called when the read operation completes.
        /// Copies will be made of the handler as required. The function signature of the handler must be:
        /// void handler(int_type ch) where ch is character value (EOF if the read operation failed).
        template<typename THandler>
        void nextc(THandler handler)
        {
            if (!can_read())
                throw std::runtime_error(utils::s_out_streambuf_msg);

            get_impl()->nextc(handler);
        }

        /// Retreats the read position, then returns the current character without advancing.
        /// (handler) is the handler to be called when the read operation completes.
        /// Copies will be made of the handler as required. The function signature of the handler must be:
        /// void handler(int_type ch) where ch is character value (EOF if the read operation failed).
        template<typename THandler>
        void ungetc(THandler handler)
        {
            if (!can_read())
                throw std::runtime_error(utils::s_out_streambuf_msg);

            get_impl()->ungetc(handler);
        }

        /// Reads up to a given number (count) of characters from the stream buffer to source memory (ptr).
        /// (handler) is the handler to be called when the read operation completes.
        /// Copies will be made of the handler as required. The function signature of the handler must be:
        /// void handler(size_t count) where count is the character count read or 0 if the end of the stream is reached.
        template<typename THandler>
        void getn(char_type* ptr, size_t count, THandler handler)
        {
            if (count)
            {
                if (!can_read())
                    async_task::connect(handler, 0);
                else
                    get_impl()->getn(ptr, count, handler);
            }
        }

        /// Reads up to a given number (count) of characters from the stream buffer to memory (ptr) synchronously.
        /// Returns the number of characters copied. 0 if the end of the stream is reached or an asynchronous
        /// (traits::requires_async) read is required.
        /// This is a synchronous operation, but is guaranteed to never block.
        size_t sgetn(char_type* ptr, size_t count)
        {
            if (count)
            {
                if (!can_read())
                    return 0;
                else
                    return get_impl()->sgetn(ptr, count);
            }
            return 0;
        }

        /// Copies up to a given number (count) of characters from the stream buffer to memory (ptr),
        /// this is done synchronously without advancing the read head.
        /// Returns the number of characters copied.
        /// 0 if the end of the stream is reached or an asynchronous (traits::requires_async) read is required.
        /// This is a synchronous operation, but is guaranteed to never block.
        size_t scopy(char_type* ptr, size_t count)
        {
            if (!can_read())
                return 0;
            return get_impl()->scopy(ptr, count);
        }

        /// For output streams, flush any internally buffered data to the underlying medium.
        void sync()
        {
            if (!can_write())
                throw std::runtime_error(utils::s_out_streambuf_msg);
            get_impl()->sync();
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
        char_type* alloc(size_t count)
        {
            if (!this->can_write())
                return nullptr;

            if (alloced_)
                throw std::logic_error("The buffer is already allocated, this maybe caused by overlap of stream read or write");

            char_type* alloc_result = get_impl()->alloc(count);

            if (alloc_result)
                alloced_ = true;

            return alloc_result;
        }

        /// Submits a block already allocated by the stream buffer.
        /// (count) The number of characters to be committed.
        /// Throws exception if buffer is not allocated
        void commit(size_t count)
        {
            if (can_write())
            {
                if (!alloced_)
                    throw std::logic_error("The buffer needs to allocate first");

                get_impl()->commit(count);
                alloced_ = false;
            }
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
        bool acquire(char_type*& ptr, size_t& count)
        {
            if (can_write())
                return get_impl()->acquire(ptr, count);
            else
                return false;
        }

        /// Releases a block of data acquired using ::acquire() method. This frees the stream buffer to de-allocate the
        /// memory, if it so desires. Move the read position ahead by the count.
        ///(ptr) A pointer to the block of data to be released.
        /// (count) The number of characters that were read.
        void release(char_type* ptr, size_t count)
        {
            if (can_write())
                get_impl()->release(ptr, count);
        }
    };


    /// Base class for all asynchronous output streams.
    /// Perform stream associated write operations.
    /// Template argument TBuf is the streambuf implementation of associated with the stream.
    template<typename TBuf>
    class async_ostream
    {
    public:
        typedef TBuf streambuf_type;
        typedef typename streambuf_type::char_type char_type;
        typedef typename streambuf_type::traits traits;
        typedef typename streambuf_type::int_type int_type;
        typedef typename streambuf_type::pos_type pos_type;
        typedef typename streambuf_type::off_type off_type;

    protected:
        void verify_and_throw(const char* msg) const
        {
            if ( !buffer_->can_write() )
                throw std::runtime_error(msg);
        }

        /// async_ostream's write completion handler to be executed
        /// when a write operation on the underlying buffer is complete.
        /// Template parameter THandler is the user supplied handler, to be executed
        /// when write() to the async_ostream object is complete.
        template<typename THandler>
        struct write_handler
        {
            write_handler(THandler handler) : handler_(handler)
            {}

            template<typename TBufSc>
            void on_write(size_t count, TBufSc* sourcebuf)
            {
                char_type* data;
                sourcebuf->acquire(data, count);
                if (data != nullptr)
                    sourcebuf->release(data, count);
                // execute user's completion handler
                handler_(count);
            }

            THandler handler_;
        };

        /// async_ostream's read completion handler to be executed
        /// when a read operation on a source buffer is complete.
        /// Template parameter THandler is the user supplied handler, to be executed
        /// when write() to the async_ostream object is complete.
        template<typename THandler>
        struct read_handler
        {
            read_handler(THandler handler) : handler_(handler)
            {}

            void on_read(size_t count, streambuf_type* sbuf, std::shared_ptr<char_type> buf)
            {
                if (count)
                {
                    char_type* data;
                    const bool acquired = sbuf->acquire(data, count);
                    // data is already allocated in the source, just committing.
                    if (acquired)
                    {
                        sbuf->commit(count);
                        handler_(count);
                    }
                    else if (buf)
                    {
                        sbuf->putn(buf.get(), count,
                          std::bind(&read_handler<THandler>::on_write, *this, count, buf));
                    }
                }
            }

            void on_write(size_t count, std::shared_ptr<char_type> buf)
            {
                // execute user's completion handler and release buf
                handler_(count);
            }

            THandler handler_;
        };

        /// stream buffer object associated with this stream
        std::shared_ptr<streambuf_type> buffer_;

    public:

        /// Default constructor
        async_ostream() : buffer_(nullptr)
        {}

        /// Copy constructor
        async_ostream(const async_ostream<TBuf>& other) : buffer_(other.buffer_)
        {}

        /// Assignment operator
        async_ostream& operator =(const async_ostream<TBuf>& other)
        {
            buffer_ = other.buffer_;
            return *this;
        }

        /// Constructor
        async_ostream(std::shared_ptr<streambuf_type> buffer) : buffer_(buffer)
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
        template<typename THandler>
        void write(char_type ch, THandler handler)
        {
            verify_and_throw(utils::s_out_streambuf_msg);
            buffer_->putc(ch, handler);
        }

        /// Write a number (count) of characters from a given stream buffer (sourcebuf) into the stream.
        /// (handler) is the handler to be called when the write operation completes.
        /// Copies will be made of the handler as required. The function signature of the handler must be:
        /// void handler(size_t count) where count is the byte count written or 0 if the write operation failed.
        template<typename THandler, typename TBufSc>
        void write(TBufSc& sourcebuf, size_t count, THandler handler)
        {
            verify_and_throw(utils::s_out_streambuf_msg);
            if ( !sourcebuf.can_read() )
                throw std::runtime_error("source buffer not set up for input of data");

            if (count == 0)
                return;

            auto data = buffer_->alloc(count);
            if ( data != nullptr )
            {
                read_handler<THandler> completion_handler(handler);
                sourcebuf.getn(data, count,
                  std::bind(&read_handler<THandler>::on_read, completion_handler, std::placeholders::_1, buffer_, nullptr));
            }
            else
            {
                size_t available = 0;
                const bool acquired = sourcebuf.acquire(data, available);
                if ( available >= count )
                {
                    write_handler<THandler> completion_handler(handler);
                    buffer_->putn(data, count,
                      std::bind(&write_handler<THandler>::on_write, completion_handler, std::placeholders::_1, sourcebuf));
                }
                else
                {
                    // Always have to release if acquire returned true.
                    if (acquired)
                        sourcebuf.release(data, 0);

                    auto buf = std::make_shared<char_type>((size_t)count);
                    read_handler<THandler> completion_handler(handler);
                    sourcebuf.getn(buf.get(), count,
                      std::bind(&read_handler<THandler>::on_read, completion_handler, std::placeholders::_1, buffer_, buf));
                }
            }
        }

        /// Write the specified string (str) to the output stream.
        /// (handler) is the handler to be called when the write operation completes.
        /// Copies will be made of the handler as required. The function signature of the handler must be:
        /// void handler(size_t count) where count is the character count read or 0 if the end of the stream is reached.
        template<typename THandler>
        void print(const std::basic_string<char_type>& str, THandler handler)
        {
            verify_and_throw(utils::s_out_streambuf_msg);
            buffer_->putn(str.c_str(), str.size(), handler);
        }

        /// Write a value of type T to the output stream.
        /// (handler) is the handler to be called when the write operation completes.
        /// Copies will be made of the handler as required. The function signature of the handler must be:
        /// void handler(size_t count) where count is the character count read or 0 if the end of the stream is reached.
        template<typename T, typename THandler>
        void print(const T& val, THandler handler)
        {
            verify_and_throw(utils::s_out_stream_msg);
            return print(utils::value_string_formatter<char_type>::format(val), handler);
        }

        /// Write a value of type T to the output stream and append a newline character.
        /// (handler) is the handler to be called when the write operation completes.
        /// Copies will be made of the handler as required. The function signature of the handler must be:
        /// void handler(size_t count) where count is the character count read or 0 if the end of the stream is reached.
        template<typename T, typename THandler>
        void print_line(const T& val, THandler handler)
        {

            verify_and_throw(utils::s_out_stream_msg);
            auto str = utils::value_string_formatter<char_type>::format(val);
            str.push_back(char_type('\n'));
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
        /// Returns a reference to the underlying stream buffer.
        streambuf_type& streambuf() { return *buffer_; }

        /// Get a pointer to the underlying stream buffer.
        /// Returns the underlying stream buffer.
        std::shared_ptr<streambuf_type> streambuf_ptr() { return buffer_; }
    };

    /// Base class for all asynchronous input streams.
    /// Perform stream associated read operations.
    /// Template argument TBuf is the streambuf implementation of the associated with the stream.
    template<typename TBuf>
    class async_istream
    {
    public:
        typedef TBuf streambuf_type;
        typedef typename streambuf_type::char_type char_type;
        typedef typename streambuf_type::traits traits;
        typedef typename streambuf_type::int_type int_type;
        typedef typename streambuf_type::pos_type pos_type;
        typedef typename streambuf_type::off_type off_type;

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
            char_type outbuf[buf_size_];
            size_t write_pos;

            bool is_full() const { return write_pos == buf_size_; }
        };

        /// stream buffer object associated with this stream
        std::shared_ptr<streambuf_type> buffer_;

        /// async_istream's write completion handler to be executed
        /// when a write operation on a target buffer is complete.
        /// Template parameter THandler is the user supplied handler to be executed
        /// when read() to the async_istream object is complete.
        template<typename THandler>
        struct write_handler
        {
            write_handler(THandler handler) : handler_(handler)
            {}

            void on_write(size_t count, streambuf_type* sbuf)
            {
                char_type* data;
                sbuf->acquire(data, count);
                if (data != nullptr)
                    sbuf->release(data, count);
                // execute user's completion handler
                handler_(count);
            }

            THandler handler_;
        };

        /// async_istream's read completion handler to be executed when a read operation on the underlying buffer is complete.
        /// Template parameter TBufTr is the type of the destination buffer where data will be stored.
        /// Template parameter THandler is the user supplied handler to be executed when read() to the async_istream object is complete.
        template<typename THandler, typename TBufTr>
        struct read_handler
        {
            read_handler(THandler handler, TBufTr& targetbuf)
             : handler_(handler), targetbuf_(&targetbuf)
            {}

            void on_read(size_t count, std::shared_ptr<char_type> buf)
            {
                if (count)
                {
                    char_type* data;
                    bool acquired = targetbuf_->acquire(data, count);
                    // data is already written to target, just committing.
                    if (acquired)
                    {
                        targetbuf_->commit(count);
                        handler_(count);
                    }
                    else if (buf)
                    {
                        targetbuf_->putn(buf.get(), count,
                          std::bind(&read_handler<THandler, TBufTr>::on_write, *this, count, buf));
                    }
                }
            }

            void on_write(size_t count, std::shared_ptr<char_type> buf)
            {
                // execute user's completion handler and release buf
                handler_(count);
            }

            THandler handler_;
            TBufTr* targetbuf_;
        };

        /// read_to_delim() internal implementation, encapsulates all asynchronous logic and streambuf function calls
        /// performed in the background.
        /// Template parameter TBufTr is the destination buffer where data will be stored.
        /// Template parameter THandler is the user supplied handler to be executed when the operation is complete.
        template<typename THandler, typename TBufTr>
        struct delim_read_impl
        {
            delim_read_impl(THandler handler,
                            streambuf_type& sourcebuf,
                            TBufTr& targetbuf)
              : eof_or_delim_(false), handler_(handler), helper_(std::make_shared<read_helper>()),
                sourcebuf_(&sourcebuf), targetbuf_(&targetbuf)
            {}

            void read_to_delim(int_type delim)
            {
                if (!sourcebuf_->in_avail())
                {
                    sourcebuf_->bumpc(std::bind(&delim_read_impl<THandler,TBufTr>::on_read, *this, std::placeholders::_1, delim));
                }
                else
                {
                    int_type req_async = traits::requires_async();

                    while (sourcebuf_->in_avail() > 0)
                    {
                        int_type ch = sourcebuf_->sbumpc();
                        if (ch == req_async)
                        {
                            sourcebuf_->bumpc(std::bind(&delim_read_impl<THandler,TBufTr>::on_read, *this, std::placeholders::_1, delim));
                            break;
                        }

                        if (ch == traits::eof() || ch == delim)
                        {
                            eof_or_delim_ = true;
                            targetbuf_->putn(helper_->outbuf, helper_->write_pos,
                              std::bind(&delim_read_impl<THandler,TBufTr>::on_write, *this, std::placeholders::_1, delim));
                            break;
                        }

                        helper_->outbuf[helper_->write_pos] = static_cast<char_type>(ch);
                        helper_->write_pos += 1;

                        if (helper_->is_full())
                        {
                            targetbuf_->putn(helper_->outbuf, helper_->write_pos,
                              std::bind(&delim_read_impl<THandler,TBufTr>::on_write, *this, std::placeholders::_1, delim));
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
                    targetbuf_->putn(helper_->outbuf, helper_->write_pos,
                      std::bind(&delim_read_impl<THandler,TBufTr>::on_write, *this, std::placeholders::_1, delim));
                }
                else
                {
                    helper_->outbuf[helper_->write_pos] = static_cast<char_type>(ch);
                    helper_->write_pos += 1;

                    if (helper_->is_full())
                    {
                        targetbuf_->putn(helper_->outbuf, helper_->write_pos,
                          std::bind(&delim_read_impl<THandler,TBufTr>::on_write, *this, std::placeholders::_1, delim));
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
                    targetbuf_->sync();

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
            THandler handler_;
            std::shared_ptr<read_helper> helper_;
            streambuf_type* sourcebuf_;
            TBufTr* targetbuf_;
        };

        /// read_line() internal implementation, encapsulates all the asynchronous logic and 
        /// streambuf function calls performed in the background.
        /// Template parameter TBufTr is the destination buffer where data will be stored.
        /// Template parameter THandler is the user supplied handler to be executed
        /// when the operation is complete.
        template<typename THandler, typename TBufTr>
        struct line_read_impl
        {
            line_read_impl(THandler handler,
                           streambuf_type& sourcebuf,
                           TBufTr& targetbuf)
                : eof_or_crlf_(false), handler_(handler), helper_(std::make_shared<read_helper>()),
                  sourcebuf_(&sourcebuf), targetbuf_(&targetbuf)
            {}

            void read_line()
            {
                if (!sourcebuf_->in_avail())
                {
                    sourcebuf_->bumpc(std::bind(&line_read_impl<THandler,TBufTr>::on_read, *this, std::placeholders::_1));
                }
                else
                {
                    int_type req_async = traits::requires_async();

                    while (sourcebuf_->in_avail() > 0)
                    {
                        int_type ch = sourcebuf_->sbumpc();
                        if (ch == req_async)
                        {
                            sourcebuf_->bumpc(std::bind(&line_read_impl<THandler,TBufTr>::on_read, *this, std::placeholders::_1));
                            break;
                        }

                        if (traits::eof() == ch)
                        {
                            eof_or_crlf_ = true;
                            targetbuf_->putn(helper_->outbuf, helper_->write_pos,
                              std::bind(&line_read_impl<THandler,TBufTr>::on_write, *this, std::placeholders::_1));
                            break;
                        }

                        if (ch == '\r')
                        {
                            update_after_cr();
                        }
                        else
                        {
                            helper_->outbuf[helper_->write_pos] = static_cast<char_type>(ch);
                            helper_->write_pos += 1;

                            if (helper_->is_full())
                            {
                                targetbuf_->putn(helper_->outbuf, helper_->write_pos,
                                  std::bind(&line_read_impl<THandler,TBufTr>::on_write, *this, std::placeholders::_1));
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
                    targetbuf_->putn(helper_->outbuf, helper_->write_pos,
                      std::bind(&line_read_impl<THandler,TBufTr>::on_write, *this, std::placeholders::_1));
                }
                else
                {
                    if (ch == '\r')
                    {
                        update_after_cr();
                    }
                    else
                    {
                        helper_->outbuf[helper_->write_pos] = static_cast<char_type>(ch);
                        helper_->write_pos += 1;

                        if (helper_->is_full())
                        {
                            targetbuf_->putn(helper_->outbuf, helper_->write_pos,
                              std::bind(&line_read_impl<THandler,TBufTr>::on_write, *this, std::placeholders::_1));
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
                    targetbuf_->sync();

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
                int_type ch = sourcebuf_->sgetc();
                if (ch == '\n')
                    sourcebuf_->sbumpc();

                eof_or_crlf_ = true;
                targetbuf_->putn(helper_->outbuf, helper_->write_pos,
                  std::bind(&line_read_impl<THandler,TBufTr>::on_write, *this, std::placeholders::_1));
            }

            bool eof_or_crlf_;
            THandler handler_;
            std::shared_ptr<read_helper> helper_;
            streambuf_type* sourcebuf_;
            TBufTr* targetbuf_;
        };

        /// read_to_end() internal implementation, encapsulates all asynchronous logic and
        /// streambuf function calls performed in the background.
        /// Template parameter TBufTr is the destination buffer where data will be stored.
        /// Template parameter THandler is the user supplied handler to be executed
        /// when the operation is complete.
        template<typename THandler, typename TBufTr>
        struct end_read_impl
        {
            end_read_impl(THandler handler,
                          streambuf_type& sourcebuf,
                          TBufTr& targetbuf)
                : handler_(handler), helper_(std::make_shared<read_helper>()),
                  sourcebuf_(&sourcebuf), targetbuf_(&targetbuf)
            {}

            void read_to_end()
            {
                sourcebuf_->getn(helper_->outbuf, buf_size_,
                  std::bind(&end_read_impl<THandler,TBufTr>::on_read, *this, std::placeholders::_1));
            }

            void on_read(size_t count)
            {
                if (count)
                {
                    targetbuf_->putn(helper_->outbuf, count,
                      std::bind(&end_read_impl<THandler,TBufTr>::on_write, *this, std::placeholders::_1));
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
                    targetbuf_->sync();
                    sourcebuf_->getn(helper_->outbuf, buf_size_,
                      std::bind(&end_read_impl<THandler,TBufTr>::on_read, *this, std::placeholders::_1));
                }
                else
                {
                    handler_(helper_->total);
                }
            }

            THandler handler_;
            std::shared_ptr<read_helper> helper_;
            streambuf_type* sourcebuf_;
            TBufTr* targetbuf_;
        };

    public:
        /// Default constructor
        async_istream() : buffer_(nullptr)
        {}

        /// Constructor
        async_istream(std::shared_ptr<streambuf_type> buffer) : buffer_(buffer)
        {
            verify_and_throw(utils::s_in_streambuf_msg);
        }

        /// Copy constructor
        async_istream(const async_istream<TBuf>& other) : buffer_(other.buffer_) {}

        /// Assignment operator
        async_istream<TBuf>& operator =(const async_istream<TBuf>& other)
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
        template<typename THandler>
        int_type read(THandler handler)
        {
            verify_and_throw(utils::s_in_streambuf_msg);
            return buffer_->bumpc(handler);
        }

        /// Reads up to (count) characters and place them into the provided buffer (targetbuf).
        /// (handler) is the handler to be called when the read operation completes.
        /// Copies will be made of the handler as required. The function signature of the handler must be:
        /// void handler(size_t count) where count is the byte count read or 0 if the read operation failed.
        template<typename THandler, typename TBufTr>
        void read(TBufTr& targetbuf, size_t count, THandler handler)
        {
            verify_and_throw(utils::s_in_streambuf_msg);
            if ( !targetbuf.can_write() )
                throw std::runtime_error("target not set up for output of data");

            if (count == 0)
                return;

            auto data = targetbuf.alloc(count);
            if ( data != nullptr )
            {
                read_handler<THandler,TBufTr> completion_handler(handler, targetbuf);
                buffer_->getn(data, count,
                  std::bind(&read_handler<THandler,TBufTr>::on_read, completion_handler, std::placeholders::_1, nullptr));
            }
            else
            {
                size_t available = 0;
                const bool acquired = buffer_->acquire(data, available);
                if (available >= count)
                {
                    write_handler<THandler> completion_handler(handler);
                    targetbuf.putn(data, count,
                      std::bind(&write_handler<THandler>::on_write, completion_handler, std::placeholders::_1, buffer_));
                }
                else
                {
                    // Always have to release if acquire returned true.
                    if (acquired)
                        buffer_->release(data, 0);

                    auto buf = std::make_shared<char_type>((size_t)count);
                    read_handler<THandler,TBufTr> completion_handler(handler, targetbuf);
                    buffer_->getn(buf.get(), count,
                      std::bind(&read_handler<THandler,TBufTr>::on_read, completion_handler, std::placeholders::_1, buf));
                }
            }
        }

        /// Get the next character and return it as an int_type. Do not advance the read position.
        /// (handler) is called with the result widened to an integer. This character is EOF when the peek operation fails.
        /// Copies will be made of the handler as required. The function signature of the handler must be:
        /// void handler(int_type ch) where ch is the resulting character or EOF if the operation failed.
        template<typename THandler>
        void peek(THandler handler)
        {
            verify_and_throw(utils::s_in_streambuf_msg);
            buffer_->getc(handler);
        }

        /// Read characters until a delimiter (delim) or EOF is found, and place them into the target buffer (targetbuf).
        /// Proceed past the delimiter, but don't include it in the target buffer.
        /// (handler) is the handler to be called when the read operation completes.
        /// Copies will be made of the handler as required. The function signature of the handler must be:
        /// void handler(size_t count) where count is the character count read or 0 if the end of the stream is reached.
        template<typename THandler, typename TBufTr>
        void read_to_delim(TBufTr& targetbuf, int_type delim, THandler handler)
        {
            verify_and_throw(utils::s_in_streambuf_msg);
            if ( !targetbuf.can_write() )
                throw std::runtime_error("target not set up for output of data");

            delim_read_impl<THandler,TBufTr> impl(handler, this->streambuf(), targetbuf);
            impl.read_to_delim(delim);
        }

        /// Read until reaching a newline character and place the result into the (targetbuf), the newline is not included.
        /// (handler) is the handler to be called when the read operation completes.
        /// Copies will be made of the handler as required. The function signature of the handler must be:
        /// void handler(size_t count) where count is the character count read or 0 if the end of the stream is reached.
        template<typename THandler, typename TBufTr>
        void read_line(TBufTr& targetbuf, THandler handler)
        {
            verify_and_throw(utils::s_in_streambuf_msg);
            if ( !targetbuf.can_write() )
                throw std::runtime_error("target not set up for receiving data");

            line_read_impl<THandler,TBufTr> impl(handler, this->streambuf(), targetbuf);
            impl.read_line();
        }

        /// Read until reaching the end of the stream.
        /// Parameter (targetbuf) is an asynchronous stream buffer supporting write operations.
        /// (handler) is executed with the number of characters read as input parameter to the post function.
        /// Copies will be made of the handler as required. The function signature of the handler must be:
        /// void handler(size_t count) where count is the character count read or 0 if the end of the stream is reached.
        template<typename THandler, typename TBufTr>
        void read_to_end(TBufTr& targetbuf, THandler handler)
        {
            verify_and_throw(utils::s_in_streambuf_msg);
            if ( !targetbuf.can_write() )
                throw std::runtime_error("target not set up for receiving data");

            end_read_impl<THandler,TBufTr> impl(handler, this->streambuf(), targetbuf);
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

        /// Test whether the stream is open for reading.
        /// Returns> true if the stream is open for reading, false otherwise.
        bool is_open() const { return is_valid() && buffer_->can_read(); }

        /// Get the underlying stream buffer.
        streambuf_type& streambuf() const { return *buffer_; }

        /// Get a pointer to the underlying stream buffer.
        /// Returns the underlying stream buffer.
        std::shared_ptr<streambuf_type> streambuf_ptr() { return buffer_; }

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

