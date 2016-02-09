//
// producer_consumer_buf.h
// Copyright (C) 2015  Emil Penchev, Bulgaria


#ifndef _PRODUCER_CONSUMER_BUF_H_
#define _PRODUCER_CONSUMER_BUF_H_

#include <vector>
#include <queue>
#include <algorithm>
#include <iterator>
#include <assert.h>
#include "async_streams.h"
#include "async_task.h"
#include "async_op.h"
#include "thread_wrapper.h"

namespace snode
{
/// Library for asynchronous streams.
namespace streams
{
    /// Serves as a memory-based stream buffer that supports both writing
    /// and reading sequences of characters at the same time. It can be used as a consumer/producer buffer.
    template<typename TChar>
    class producer_consumer_buffer : public async_streambuf<TChar, producer_consumer_buffer<TChar> >
    {
    public:
        typedef TChar char_type;
        typedef async_streambuf<TChar, producer_consumer_buffer<TChar> > base_streambuf_type;
        typedef typename producer_consumer_buffer::traits traits;
        typedef typename producer_consumer_buffer::pos_type pos_type;
        typedef typename producer_consumer_buffer::int_type int_type;
        typedef typename producer_consumer_buffer::off_type off_type;

        /// Default constructor accepts allocation size in bytes for the internal memory blocks.
        producer_consumer_buffer(size_t alloc_size = 512) : base_streambuf_type(std::ios_base::out | std::ios_base::in),
            alloc_size_(alloc_size),
            allocblock_(nullptr),
            total_(0), total_read_(0), total_written_(0),
            synced_(0)
        {}

        /// Destructor
        virtual ~producer_consumer_buffer()
        {
            this->close();
            blocks_.clear();
        }

        /// helper function for instance creation
        static async_streambuf<TChar, producer_consumer_buffer<TChar> >*
        create_instance(size_t alloc_size = 512)
        {
            return new producer_consumer_buffer(alloc_size);
        }

        /// helper function for shared instance creation
        static std::shared_ptr<async_streambuf<TChar, producer_consumer_buffer<TChar> > >
        create_shared_instance(size_t alloc_size = 512)
        {
            return std::make_shared<producer_consumer_buffer<TChar> >(alloc_size);
        }

        /// checks if stream buffer supports seeking.
        bool can_seek() const { return false; }

        /// checks whether a stream buffer supports size().
        bool has_size() const { return false; }

        /// For any input stream,
        /// returns the number of characters that are immediately available to be consumed without blocking.
        /// For details see async_streambuf::in_avail()
        size_t in_avail() const { return total_; }

        /// Gets the current read or write position in the stream for the given (direction).
        /// For details see async_streambuf::getpos()
        pos_type getpos(std::ios_base::openmode mode) const
        {
            if ( ((mode & std::ios_base::in) && !this->can_read()) ||
                 ((mode & std::ios_base::out) && !this->can_write()))
            {
                return static_cast<pos_type>(traits::eof());
            }

            if (mode == std::ios_base::in)
                return (pos_type)total_read_;
            else if (mode == std::ios_base::out)
                return (pos_type)total_written_;
            else
                return (pos_type)traits::eof();
        }

        /// Allocates a contiguous block of memory of (count) bytes and returns it.
        /// For details see async_streambuf::alloc()
        char_type* alloc(size_t count)
        {
            if (!this->can_write())
            {
                return nullptr;
            }

            // We always allocate a new block even if the count could be satisfied by
            // the current write block. While this does lead to wasted space it allows for
            // easier book keeping

            assert(!allocblock_);
            allocblock_ = std::make_shared<mem_block>(count);
            return allocblock_->wbegin();
        }

        /// Submits a block already allocated by the stream buffer.
        /// For details see async_streambuf::commit()
        void commit(size_t count)
        {
            // The count does not reflect the actual size of the block.
            // Since we do not allow any more writes to this block it would suffice.
            // If we ever change the algorithm to reuse blocks then this needs to be revisited.

            assert((bool)allocblock_);
            allocblock_->update_write_head(count);
            blocks_.push_back(allocblock_);
            allocblock_ = nullptr;

            update_write_head(count);
        }

        /// Gets a pointer to the next already allocated contiguous block of data.
        /// For details see async_streambuf::acquire()
        bool acquire(char_type*& ptr, size_t& count)
        {
            count = 0;
            ptr = nullptr;

            if (!this->can_read()) return false;

            if (blocks_.empty())
            {
                // If the write head has been closed then have reached the end of the
                // stream (return true), otherwise more data could be written later (return false).
                return !this->can_write();
            }
            else
            {
                auto block = blocks_.front();

                count = block->rd_chars_left();
                ptr = block->rbegin();

                assert(ptr != nullptr);
                return true;
            }
        }

        /// Releases a block of data acquired using acquire() method
        /// For details see async_streambuf::release()
        void release(char_type* ptr, size_t count)
        {
            if (ptr == nullptr)
                return;

            auto block = blocks_.front();

            assert(block->rd_chars_left() >= count);
            block->read_ += count;

            update_read_head(count);
        }

        /// For output streams, flush any internally buffered data to the underlying medium.
        void sync()
        {
            synced_ = this->in_avail();
            fulfill_outstanding();
        }

        /// Writes a single character to the stream buffer.
        /// For details see async_streambuf::putc()
        template<typename THandler>
        void putc(char_type ch, THandler handler)
        {
            auto op = new async_streambuf_op<char_type, THandler>(handler);
            auto write_fn = [](char_type ch,
                               async_streambuf_op_base<char_type>* op,
                               producer_consumer_buffer<char_type>* buf)
            {
                int_type res = (buf->write(&ch, 1) ? static_cast<int_type>(ch) : traits::eof());
                op->complete_ch(res);
            };
            async_task::connect(write_fn, ch, op, this);
        }

        /// Writes a number of characters to the stream buffer from memory.
        /// For details see async_streambuf::putn()
        template<typename THandler>
        void putn(const char_type* ptr, size_t count, THandler handler)
        {
            auto op = new async_streambuf_op<char_type, THandler>(handler);
            // make a copy of the data
            auto cpbuf = std::make_shared<std::vector<char_type> >(count);
            std::copy(ptr, ptr + count, cpbuf->data());

            auto write_fn = [](std::shared_ptr<std::vector<char_type> > cpbuf,
                               async_streambuf_op_base<char_type>* op,
                               producer_consumer_buffer<char_type>* buf)
            {
                size_t res = buf->write(cpbuf->data(), cpbuf->size());
                op->complete_size(res);
            };
            async_task::connect(write_fn, cpbuf, op, this);
        }

        /// Writes a number of characters to the stream buffer from memory.
        /// For details see async_streambuf::putn_nocopy()
        template<typename THandler>
        void putn_nocopy(const char_type* ptr, size_t count, THandler handler)
        {
            auto op = new async_streambuf_op<char_type, THandler>(handler);
            auto write_fn = [](const char_type* ptr, size_t count,
                               async_streambuf_op_base<char_type>* op, producer_consumer_buffer<char_type>* buf)
            {
                size_t res = buf->write(ptr, count);
                op->complete_size(res);
            };
            async_task::connect(write_fn, ptr, count, op, this);
        }

        /// Reads up to a given number of characters from the stream buffer to memory.
        /// For details see async_streambuf::getn()
        template<typename THandler>
        void getn(char_type* ptr, size_t count, THandler handler)
        {
            auto op = new async_streambuf_op<char_type, THandler>(handler);
            enqueue_request(ev_request(*this, op, ptr, count));
        }

        /// Reads up to a given number of characters from the stream buffer to memory synchronously.
        /// For details see async_streambuf::sgetn()
        size_t sgetn(char_type* ptr, size_t count)
        {
            return can_satisfy(count) ? this->read(ptr, count) : (size_t)traits::requires_async();
        }

        /// Copies up to a given number of characters from the stream buffer to memory synchronously.
        /// For details see async_streambuf::scopy()
        size_t scopy(char_type* ptr, size_t count)
        {
            return can_satisfy(count) ? this->read(ptr, count, false) : (size_t)traits::requires_async();
        }

        /// Reads a single character from the stream and advances the read position.
        /// For details see async_streambuf::bumpc()
        template<typename THandler>
        void bumpc(THandler handler)
        {
            auto op = new async_streambuf_op<char_type, THandler>(handler);
            enqueue_request(ev_request(*this, op));
        }

        /// Reads a single character from the stream and advances the read position.
        /// For details see async_streambuf::sbumpc()
        int_type sbumpc()
        {
            return can_satisfy(1) ? this->read_byte(true) : traits::requires_async();
        }

        /// Reads a single character from the stream without advancing the read position.
        /// For details see async_streambuf::getc()
        template<typename THandler>
        void getc(THandler handler)
        {
            auto op = new async_streambuf_op<char_type, THandler>(handler);
            enqueue_request(ev_request(*this, op, 1, ev_request::NoAdvance));
        }

        /// Reads a single character from the stream without advancing the read position.
        /// For details see async_streambuf::sgetc()
        int_type sgetc()
        {
            return can_satisfy(1) ? this->read_byte(false) : traits::requires_async();
        }

        /// Advances the read position, then returns the next character without advancing again.
        /// For details see async_streambuf::nextc()
        template<typename THandler>
        void nextc(THandler handler)
        {
            auto op = new async_streambuf_op<char_type, THandler>(handler);
            enqueue_request(ev_request(*this, op, 1, ev_request::AdvanceBefore));
        }

        /// Retreats the read position, then returns the current character without advancing.
        /// For details see async_streambuf::ungetc()
        template<typename THandler>
        void ungetc(THandler handler)
        {
            async_task::connect(handler, static_cast<int_type>(traits::eof()));
        }

        /// Close for reading
        void close_read()
        {
            this->stream_can_read_ = false;
        }

        /// Close for writing
        void close_write()
        {
            // First indicate that there could be no more writes.
            // Fulfill outstanding relies on that to flush all the
            // read requests.
            this->stream_can_write_ = false;

            // This runs on the thread that called close.
            this->fulfill_outstanding();
        }

    private:
        /// Represents a memory block
        class mem_block
        {
        public:
            mem_block(size_t size)
                : read_(0), pos_(0), size_(size), data_(new char_type[size])
            {
            }

            ~mem_block()
            {
                delete [] data_;
            }

            // Read head
            size_t read_;

            // Write head
            size_t pos_;

            // Allocation size (of m_data)
            size_t size_;

            // The data store
            char_type* data_;

            /// Pointer to the read head
            char_type* rbegin()
            {
                return data_ + read_;
            }

            /// Pointer to the write head
            char_type* wbegin()
            {
                return data_ + pos_;
            }

            /// Read up to count characters from the block
            size_t read(char_type* dest, size_t count, bool advance = true)
            {
                size_t avail = rd_chars_left();
                auto readcount = std::min(count, avail);

                char_type* beg = rbegin();
                char_type* end = rbegin() + readcount;

#ifdef _WIN32
                    // Avoid warning C4996: Use checked iterators under SECURE_SCL
                    std::copy(beg, end, stdext::checked_array_iterator<char_type*>(dest, count));
#else
                    std::copy(beg, end, dest);
#endif // _WIN32
                    if (advance)
                        read_ += readcount;

                    return readcount;
            }

            /// Write count characters into the block
            size_t write(const char_type* src, size_t count)
            {
                size_t avail = wr_chars_left();
                auto wrcount = std::min(count, avail);

                const char_type* src_end = src + wrcount;

#ifdef _WIN32
                // Avoid warning C4996: Use checked iterators under SECURE_SCL
                std::copy(src, src_end, stdext::checked_array_iterator<char_type *>(wbegin(), static_cast<size_t>(avail)));
#else
                std::copy(src, src_end, wbegin());
#endif // _WIN32

                update_write_head(wrcount);
                return wrcount;
            }

            void update_write_head(size_t count)
            {
                pos_ += count;
            }

            size_t rd_chars_left() const { return pos_ - read_; }
            size_t wr_chars_left() const { return size_ - pos_; }

        private:

            // Copy is not supported
            mem_block(const mem_block&);
            mem_block& operator=(const mem_block&);
        };


        /// Represents a request on the stream buffer - typically reads
        class ev_request
        {
        public:
            enum AdvanceAction { AdvanceOnce = 1, AdvanceBefore = 2, NoAdvance = 3 };

            ev_request(producer_consumer_buffer<char_type>& streambuf,
                       async_streambuf_op_base<char_type>* op,
                       char_type* ptr = nullptr,
                       size_t count = 1,
                       AdvanceAction advance = AdvanceOnce)
            : count_(count), advance_act_(advance), bufptr_(ptr), streambuf_(streambuf), completion_op_(op)
            {}

            size_t size() const
            {
                return count_;
            }

            void complete()
            {
                if (count_ > 1 && bufptr_ != nullptr)
                {
                    bool advance = true;
                    if (NoAdvance == advance_act_)
                        advance = false;

                    size_t countread = streambuf_.read(bufptr_, count_, advance);
                    async_task::connect(&async_streambuf_op_base<char_type>::complete_size, completion_op_, countread);
                }
                else
                {
                    bool advance = true;
                    if (NoAdvance == advance_act_ || AdvanceBefore == advance_act_)
                        advance = false;
                    if (AdvanceBefore == advance_act_)
                        streambuf_.read_byte(true);

                    int_type value = streambuf_.read_byte(advance);
                    async_task::connect(&async_streambuf_op_base<char_type>::complete_ch, completion_op_, value);
                }
            }

        protected:
            size_t count_;
            AdvanceAction advance_act_;
            char_type* bufptr_;
            producer_consumer_buffer<char_type>& streambuf_;
            async_streambuf_op_base<char_type>* completion_op_;
        };

        /// Updates the write head by an offset specified by count
        /// This should be called with the lock held.
        void update_write_head(size_t count)
        {
            total_ += count;
            total_written_ += count;
            fulfill_outstanding();
        }

        /// Writes count characters from ptr into the stream buffer
        size_t write(const char_type* ptr, size_t count)
        {
            if (!this->can_write() || (count == 0)) return 0;

            // If no one is going to read, why bother?
            // Just pretend to be writing!
            if (!this->can_read())
                return count;

            // Allocate a new block if necessary
            if ( blocks_.empty() || blocks_.back()->wr_chars_left() < count )
            {
                size_t alloc_size = std::max(count, alloc_size_);
                blocks_.push_back(std::make_shared<mem_block>((size_t)alloc_size));
            }

            // The block at the back is always the write head
            auto last = blocks_.back();
            auto written = last->write(ptr, count);
            assert(written == count);

            update_write_head(written);
            return written;
        }

        /// Writes count characters from ptr into the stream buffer
        size_t write_locked(const char_type* ptr, size_t count)
        {
            // disabled for now            
            // lib::lock_guard<lib::recursive_mutex> lockg(mutex_);
            return this->write(ptr, count);
        }

        /// Fulfill pending requests
        void fulfill_outstanding()
        {
            while (!requests_.empty())
            {
                auto req = requests_.front();

                // If we cannot satisfy the request then we need
                // to wait for the producer to write data
                if (!can_satisfy(req.size()))
                    return;

                // We have enough data to satisfy this request
                req.complete();

                // Remove it from the request queue
                requests_.pop();
            }
        }

        void enqueue_request(ev_request req)
        {
            if (can_satisfy(req.size()))
            {
                // We can immediately fulfill the request.
                req.complete();
            }
            else
            {
                // We must wait for data to arrive.
                requests_.push(req);
            }
        }

        /// Determine if the request can be satisfied.
        bool can_satisfy(size_t count)
        {
            return (synced_ > 0) || (this->in_avail() >= count) || !this->can_write();
        }

        /// Reads a byte from the stream and returns it as int_type.
        /// Note: This routine shall only be called if can_satisfy() returned true.
        int_type read_byte(bool advance = true)
        {
            char_type value;
            auto read_size = this->read(&value, 1, advance);
            return read_size == 1 ? static_cast<int_type>(value) : traits::eof();
        }

        /// Reads up to (count) characters into (ptr) and returns the count of characters copied.
        /// The return value (actual characters copied) could be <= count.
        /// Note: This routine shall only be called if can_satisfy() returned true.
        size_t read(char_type* ptr, size_t count, bool advance = true)
        {
            assert(can_satisfy(count));

            size_t totalr = 0;

            for (auto iter = begin(blocks_); iter != std::end(blocks_); ++iter)
            {
                auto block = *iter;
                auto read_from_block = block->read(ptr + totalr, count - totalr, advance);

                totalr += read_from_block;

                assert(count >= totalr);
                if (totalr == count)
                    break;
            }

            if (advance)
            {
                update_read_head(totalr);
            }

            return totalr;
        }

        /// Updates the read head by the specified offset
        /// This should be called with the lock held.
        void update_read_head(size_t count)
        {
            total_ -= count;
            total_read_ += count;

            if ( synced_ > 0 )
                synced_ = (synced_ > count) ? (synced_ - count) : 0;

            // The block at the front is always the read head.
            // Purge empty blocks so that the block at the front reflects the read head
            while (!blocks_.empty())
            {
                // If front block is not empty - we are done
                if (blocks_.front()->rd_chars_left() > 0) break;

                // The block has no more data to be read. Release the block
                blocks_.pop_front();
            }
        }

        // Default block size
        size_t alloc_size_;

        // Block used for alloc/commit
        std::shared_ptr<mem_block> allocblock_;

        // Total available data
        size_t total_;

        size_t total_read_;
        size_t total_written_;

        // Keeps track of the number of chars that have been flushed but still
        // remain to be consumed by a read operation.
        size_t synced_;

        // The producer-consumer buffer is intended to be used concurrently by a reader
        // and a writer, who are not coordinating their accesses to the buffer (coordination
        // being what the buffer is for in the first place). Thus, we have to protect
        // against some of the internal data elements against concurrent accesses
        // and the possibility of inconsistent states. A simple non-recursive lock
        // should be sufficient for those purposes if the buffer is to be accessed from different threads.

        // global lock in case of using the write_locked and read_locked functions
        // lib::recursive_mutex mutex_;

        // Memory blocks
        std::deque<std::shared_ptr<mem_block>> blocks_;

        // Queue of requests
        std::queue<ev_request> requests_;

    };

}} // namespaces

#endif /* _PRODUCER_CONSUMER_BUF_H_ */
