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

namespace snode
{
/// Library for asynchronous streams.
namespace streams
{
    /// The producer_consumer_buffer class serves as a memory-based steam buffer that supports both writing and reading
    /// sequences of characters. It can be used as a consumer/producer buffer.
    template<typename CharType>
    class producer_consumer_buffer : public async_streambuf<CharType, producer_consumer_buffer<CharType>>
    {
    public:
        typedef async_streambuf<CharType, producer_consumer_buffer> base_stream_type;
        typedef typename producer_consumer_buffer::traits traits;
        typedef typename producer_consumer_buffer::pos_type pos_type;
        typedef typename producer_consumer_buffer::int_type int_type;
        typedef typename producer_consumer_buffer::off_type off_type;

        /// Constructor
        producer_consumer_buffer(size_t alloc_size) : base_stream_type(std::ios_base::out | std::ios_base::in),
            alloc_size_(alloc_size),
            allocblock_(nullptr),
            total_(0), total_read_(0), total_written_(0),
            synced_(0)
        {
        }

        /// Destructor
        ~producer_consumer_buffer()
        {
        }

        /// internal implementation of can_seek() from async_streambuf
        bool can_seek_impl() const { return false; }

        /// internal implementation of has_size() from async_streambuf
        bool has_size_impl() const { return false; }

        /// internal implementation of buffer_size() from async_streambuf
        size_t buffer_size_impl(std::ios_base::openmode = std::ios_base::in) const
        {
            return 0;
        }

        /// internal implementation of in_avail() from async_streambuf
        size_t in_avail_impl() const { return total_; }

        /// internal implementation of getpos() from async_streambuf
        pos_type getpos_impl(std::ios_base::openmode mode) const
        {
            if ( ((mode & std::ios_base::in) && !this->can_read()) ||
                 ((mode & std::ios_base::out) && !this->can_write()))
                 return static_cast<pos_type>(traits::eof());

            if (mode == std::ios_base::in)
                return (pos_type)total_read_;
            else if (mode == std::ios_base::out)
                return (pos_type)total_written_;
            else
                return (pos_type)traits::eof();
        }

        // Seeking is not supported
        pos_type seekpos_impl(pos_type, std::ios_base::openmode) { return (pos_type)traits::eof(); }
        pos_type seekoff_impl(off_type , std::ios_base::seekdir , std::ios_base::openmode ) { return (pos_type)traits::eof(); }

        /// internal implementation of alloc() from async_streambuf
        CharType* alloc_impl(size_t count)
        {
            if (!this->can_write())
            {
                return nullptr;
            }

            // We always allocate a new block even if the count could be satisfied by
            // the current write block. While this does lead to wasted space it allows for
            // easier book keeping

            assert(!allocblock_);
            allocblock_ = std::make_shared<_block>(count);
            return allocblock_->wbegin();
        }

        /// internal implementation of commit() from async_streambuf
        void commit_impl(size_t count)
        {
            // add lock
            // pplx::extensibility::scoped_critical_section_t l(m_lock);

            // The count does not reflect the actual size of the block.
            // Since we do not allow any more writes to this block it would suffice.
            // If we ever change the algorithm to reuse blocks then this needs to be revisited.

            assert((bool)allocblock_);
            allocblock_->update_write_head(count);
            blocks_.push_back(allocblock_);
            allocblock_ = nullptr;

            update_write_head(count);
        }

        /// internal implementation of acquire() from async_streambuf
        bool acquire_impl(CharType*& ptr, size_t& count)
        {
            count = 0;
            ptr = nullptr;

            if (!this->can_read()) return false;

            // add lock
            // pplx::extensibility::scoped_critical_section_t l(m_lock);

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

        /// internal implementation of release() from async_streambuf
        void release_impl(CharType* ptr, size_t count)
        {
            if (ptr == nullptr)
                return;

            // add lock
            // pplx::extensibility::scoped_critical_section_t l(m_lock);
            auto block = blocks_.front();

            assert(block->rd_chars_left() >= count);
            block->read_ += count;

            update_read_head(count);
        }

        /// internal implementation of sync() from async_streambuf
        void sync_impl()
        {
            // add lock
            // pplx::extensibility::scoped_critical_section_t l(m_lock);

            synced_ = this->in_avail();
            fulfill_outstanding();
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

        /// internal implementation of getn() from async_streambuf
        template<typename ReadHandler>
        void getn_impl(CharType* ptr, size_t count, ReadHandler handler)
        {
            enqueue_request(_request(*this, handler, ptr, count));
        }

        /// internal implementation of sgetn() from async_streambuf
        size_t sgetn_impl(CharType* ptr, size_t count)
        {
            // add lock !!!
            // pplx::extensibility::scoped_critical_section_t l(m_lock);
            return can_satisfy(count) ? this->read(ptr, count) : (size_t)traits::requires_async();
        }

        /// internal implementation of scopy() from async_streambuf
        size_t scopy_impl(CharType* ptr, size_t count)
        {
            // add lock
            // pplx::extensibility::scoped_critical_section_t l(m_lock);
            return can_satisfy(count) ? this->read(ptr, count, false) : (size_t)traits::requires_async();
        }

        /// internal implementation of bumpc() from async_streambuf
        template<typename ReadHandler>
        void bumpc_impl(ReadHandler handler)
        {
            enqueue_request(_request(*this, handler));
        }

        /// internal implementation of sbumpc() from async_streambuf
        int_type sbumpc_impl()
        {
            // add lock
            // pplx::extensibility::scoped_critical_section_t l(m_lock);
            return can_satisfy(1) ? this->read_byte(true) : traits::requires_async();
        }

        /// internal implementation of getc() from async_streambuf
        template<typename ReadHandler>
        void getc_impl(ReadHandler handler)
        {
            enqueue_request(_request(*this, handler));
        }

        /// internal implementation of sgetc() from async_streambuf
        int_type sgetc_impl()
        {
            // add lock
            // pplx::extensibility::scoped_critical_section_t l(m_lock);
            return can_satisfy(1) ? this->read_byte(false) : traits::requires_async();
        }

        /// internal implementation of nextc() from async_streambuf
        template<typename ReadHandler>
        void nextc_impl(ReadHandler handler)
        {
            enqueue_request(_request(*this, handler));
        }

        /// internal implementation of ungetc() from async_streambuf
        template<typename ReadHandler>
        void ungetc_impl(ReadHandler handler)
        {
            async_event_task::connect(handler, static_cast<int_type>(traits::eof()));
        }

    private:

        /// Represents a memory block
        class _block
        {
        public:
            _block(size_t size)
                : read_(0), pos_(0), size_(size), data_(new CharType[size])
            {
            }

            ~_block()
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
            CharType* data_;

            /// Pointer to the read head
            CharType* rbegin()
            {
                return data_ + read_;
            }

            /// Pointer to the write head
            CharType* wbegin()
            {
                return data_ + pos_;
            }

            /// Read up to count characters from the block
            size_t read(CharType* dest, size_t count, bool advance = true)
            {
                size_t avail = rd_chars_left();
                auto readcount = std::min(count, avail);

                CharType* beg = rbegin();
                CharType* end = rbegin() + readcount;

#ifdef _WIN32
                    // Avoid warning C4996: Use checked iterators under SECURE_SCL
                    std::copy(beg, end, stdext::checked_array_iterator<CharType*>(dest, count));
#else
                    std::copy(beg, end, dest);
#endif // _WIN32
                    if (advance)
                        read_ += readcount;

                    return readcount;
            }

            /// Write count characters into the block
            size_t write(const CharType* src, size_t count)
            {
                size_t avail = wr_chars_left();
                auto wrcount = std::min(count, avail);

                const CharType* src_end = src + wrcount;

#ifdef _WIN32
                // Avoid warning C4996: Use checked iterators under SECURE_SCL
                std::copy(src, src_end, stdext::checked_array_iterator<_CharType *>(wbegin(), static_cast<size_t>(avail)));
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
            _block(const _block&);
            _block& operator=(const _block&);
        };


        /// Represents a request on the stream buffer - typically reads
        class _request
        {
        public:
            _request(producer_consumer_buffer<CharType>& parent, async_streambuf_operation<CharType> op, CharType* ptr = nullptr, size_t count = 1)
                : count_(count), bufptr_(ptr), callback_op_(op), parent_(parent)
            {
            }

            size_t size() const
            {
                return count_;
            }

            void complete()
            {
                if (count_ > 1 && bufptr_ != nullptr)
                {
                    size_t countread = parent_.read(bufptr_, count_);
                    async_event_task::connect(callback_op_, countread);
                }
                else
                {
                    int_type value = parent_.read_byte();
                    async_event_task::connect(callback_op_, value);
                }
            }

        protected:
            size_t count_;
            CharType* bufptr_;
            async_streambuf_operation<CharType> callback_op_;
            producer_consumer_buffer<CharType>& parent_;
        };

        /// Close the stream buffer for writing
        void _close_write()
        {
            // First indicate that there could be no more writes.
            // Fulfill outstanding relies on that to flush all the
            // read requests.
            this->stream_can_write_ = false;

            // pplx::extensibility::scoped_critical_section_t l(this->m_lock);

            // This runs on the thread that called close.
            this->fulfill_outstanding();
        }

        /// Updates the write head by an offset specified by count
        /// This should be called with the lock held.
        void update_write_head(size_t count)
        {
            total_ += count;
            total_written_ += count;
            fulfill_outstanding();
        }

        /// Writes count characters from ptr into the stream buffer
        size_t write(const CharType* ptr, size_t count)
        {
            if (!this->can_write() || (count == 0)) return 0;

            // If no one is going to read, why bother?
            // Just pretend to be writing!
            if (!this->can_read())
                return count;

            // add lock
            // pplx::extensibility::scoped_critical_section_t l(m_lock);

            // Allocate a new block if necessary
            if ( blocks_.empty() || blocks_.back()->wr_chars_left() < count )
            {
                size_t alloc_size = std::max(count, alloc_size_);
                blocks_.push_back(std::make_shared<_block>((size_t)alloc_size));
            }

            // The block at the back is always the write head
            auto last = blocks_.back();
            auto written = last->write(ptr, count);
            assert(written == count);

            update_write_head(written);
            return written;
        }

        /// Fulfill pending requests
        /// This should be called with the lock held.
        void fulfill_outstanding()
        {
            while ( !requests_.empty() )
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

        void enqueue_request(_request req)
        {
            // Must use some kind of lock !!!
            // pplx::extensibility::scoped_critical_section_t l(m_lock);

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
        /// This should be called with the lock held.
        int_type read_byte(bool advance = true)
        {
            CharType value;
            auto read_size = this->read(&value, 1, advance);
            return read_size == 1 ? static_cast<int_type>(value) : traits::eof();
        }

        /// Reads up to (count) characters into (ptr) and returns the count of characters copied.
        /// The return value (actual characters copied) could be <= count.
        /// Note: This routine shall only be called if can_satisfy() returned true.
        /// This should be called with the lock held.
        size_t read(CharType* ptr, size_t count, bool advance = true)
        {
            assert(can_satisfy(count));

            size_t totalread = 0;

            for (auto iter = begin(blocks_); iter != std::end(blocks_); ++iter)
            {
                auto block = *iter;
                auto read_from_block = block->read(ptr + read, count - totalread, advance);

                totalread += read_from_block;

                assert(count >= read);
                if (totalread == count)
                    break;
            }

            if (advance)
            {
                update_read_head(totalread);
            }

            return totalread;
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

                // The block has no more data to be read. Relase the block
                blocks_.pop_front();
            }
        }

        // Default block size
        size_t alloc_size_;

        // Block used for alloc/commit
        std::shared_ptr<_block> allocblock_;

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
        // should be sufficient for those purposes.
        // pplx::extensibility::critical_section_t m_lock;

        // Memory blocks
        std::deque<std::shared_ptr<_block>> blocks_;

        // Queue of requests
        std::queue<_request> requests_;
    };

}} // namespaces

#endif /* _PRODUCER_CONSUMER_BUF_H_ */
