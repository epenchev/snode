#include <iostream>
#include <string>
#include <functional>
#include <algorithm>
#include <chrono>
#include <thread>

#include "snode_core.h"
#include "async_task.h"
#include "async_streams.h"
#include "producer_consumer_buf.h"

#define BOOST_TEST_LOG_LEVEL all
#define BOOST_TEST_BUILD_INFO yes
#include <boost/test/included/unit_test.hpp>
using namespace boost::unit_test;

/*
 * shell compile
 *  g++ -std=c++11 -g -Wall -I../ streambuf_test.cpp ../config_reader.o ../http_helpers.o ../http_msg.o ../http_service.o ../snode_core.o ../uri_utils.o
   -o streambuf_test -lpthread -lboost_system -lboost_thread
 *
 */

typedef void (*test_func_type)(void);
typedef uint8_t char_type;
typedef snode::streams::producer_consumer_buffer<char_type> prod_cons_buf_type;
typedef std::shared_ptr<prod_cons_buf_type> prod_cons_buf_ptr;
typedef std::shared_ptr<uint8_t> buf_ptr;

static bool s_block = true;
void inline wait_test()
{
    s_block = true;
}

void inline finish_test()
{
    s_block = false;
}

int async_streambuf_test_base(test_func_type func)
{
    auto threads = snode::snode_core::instance().get_threadpool().threads();
    auto thread = threads.begin()->get();
    snode::async_task::connect(func, thread->get_id());
    wait_test();
    while (s_block)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    return 0;
}

prod_cons_buf_ptr create_producer_consumer_buffer_with_data(const std::vector<uint8_t> & s)
{
    prod_cons_buf_ptr buf = std::make_shared<prod_cons_buf_type>(512);
    auto target = buf->alloc(s.size());
    BOOST_ASSERT(target != nullptr);
    std::copy(s.begin(), s.end(), target);
    buf->commit(s.size());
    buf->close(std::ios_base::out);
    return buf;
}

template<typename StreamBufferTypePtr>
void test_streambuf_putn_getn(StreamBufferTypePtr rwbuf)
{
    BOOST_CHECK_EQUAL(true, rwbuf->is_open());
    BOOST_CHECK_EQUAL(true, rwbuf->can_read());
    BOOST_CHECK_EQUAL(true, rwbuf->can_write());
    BOOST_CHECK_EQUAL(false, rwbuf->is_eof());

    typedef typename StreamBufferTypePtr::element_type::int_type int_type;
    typedef typename StreamBufferTypePtr::element_type::char_type ch_type;
    std::basic_string<ch_type> s;
    s.push_back((ch_type)0);
    s.push_back((ch_type)1);
    s.push_back((ch_type)2);
    s.push_back((ch_type)3);

    auto handler_write = [](size_t count, StreamBufferTypePtr wbuf, std::basic_string<ch_type> content)
    {
        BOOST_CHECK_EQUAL(count, content.size());
        wbuf->close(std::ios_base::out);
    };
    rwbuf->putn(s.data(), s.size(), std::bind(handler_write, std::placeholders::_1, rwbuf, s));

    typedef std::shared_ptr<std::vector<ch_type> > vector_ptr;
    vector_ptr ptr = std::make_shared< std::vector<ch_type> >(4);
    auto handler_read = [](size_t count, StreamBufferTypePtr rbuf, std::basic_string<ch_type> content, vector_ptr ptr)
    {
        BOOST_CHECK_EQUAL(count, content.size());
        for (size_t i = 0; i < count; i++)
        {
            BOOST_CHECK_EQUAL(content[i], ptr->at(i));
        }
        BOOST_CHECK_EQUAL(false, rbuf->is_eof());

        auto handler_fin = [](int_type ch, StreamBufferTypePtr rbuf)
        {
            BOOST_CHECK_EQUAL(ch, snode::streams::char_traits<ch_type>::eof());
            BOOST_CHECK_EQUAL(true, rbuf->is_eof());
            finish_test();
        };
        rbuf->getc(std::bind(handler_fin, std::placeholders::_1, rbuf));
    };
    rwbuf->getn(ptr->data(), ptr->size() * sizeof(ch_type), std::bind(handler_read, std::placeholders::_1, rwbuf, s, ptr));
}

template<typename StreamBufferTypePtr>
void test_streambuf_putn(StreamBufferTypePtr wbuf)
{
    BOOST_CHECK_EQUAL(true, wbuf->can_write());
    typedef typename StreamBufferTypePtr::element_type::char_type ch_type;

    std::basic_string<ch_type> s;
    s.push_back((ch_type)0);
    s.push_back((ch_type)1);
    s.push_back((ch_type)2);
    s.push_back((ch_type)3);

    auto handler_putn = [](size_t count, StreamBufferTypePtr wbuf, std::basic_string<ch_type> contents)
    {
        BOOST_CHECK_EQUAL(count, contents.size());
        BOOST_CHECK_EQUAL(count, wbuf->in_avail());

        auto handler_loop = [] (size_t count, int loop_count, StreamBufferTypePtr wbuf, std::basic_string<ch_type> contents)
        {
            if (!loop_count)
            {
                BOOST_CHECK_EQUAL(contents.size() * 10, wbuf->in_avail());
                wbuf->close();
                BOOST_CHECK_EQUAL(false, wbuf->can_write());
                auto handler_fin = [](size_t count, StreamBufferTypePtr buf)
                {
                    BOOST_CHECK_EQUAL(0, count);
                    finish_test();
                };
                wbuf->putn(contents.data(), contents.size(), std::bind(handler_fin, std::placeholders::_1, wbuf));
            }
        };
        for (int idx = 8; idx >= 0; idx--)
            wbuf->putn(contents.data(), contents.size(), std::bind(handler_loop, std::placeholders::_1, idx, wbuf, contents));
    };
    wbuf->putn(s.data(), s.size(), std::bind(handler_putn, std::placeholders::_1, wbuf, s));
}

template<typename StreamBufferTypePtr>
void test_streambuf_putc(StreamBufferTypePtr wbuf)
{
    BOOST_CHECK_EQUAL(true, wbuf->can_write()) ;
    typedef typename StreamBufferTypePtr::element_type::char_type ch_type;
    typedef typename StreamBufferTypePtr::element_type::int_type int_type;

    std::basic_string<ch_type> s;
    s.push_back((ch_type)0);
    s.push_back((ch_type)1);
    s.push_back((ch_type)2);
    s.push_back((ch_type)3);

    auto handler_putc = [](ch_type ch, prod_cons_buf_ptr wbuf, std::basic_string<ch_type> contents)
    {
        BOOST_CHECK_NE((ch_type)prod_cons_buf_type::traits::eof(), ch);
        if (contents[3] == ch)
        {
            BOOST_CHECK_EQUAL(wbuf->in_avail(), contents.size());
            wbuf->close();
            BOOST_CHECK_EQUAL(false, wbuf->can_write());
            auto handler_fin = [](ch_type ch, prod_cons_buf_ptr wbuf)
            {
                BOOST_CHECK_EQUAL((ch_type)snode::streams::char_traits<ch_type>::eof(), ch);
                finish_test();
            };
            wbuf->putc(contents[0], std::bind(handler_fin, std::placeholders::_1, wbuf));
        }
    };

    wbuf->putc(s[0], std::bind(handler_putc, std::placeholders::_1, wbuf, s));
    wbuf->putc(s[1], std::bind(handler_putc, std::placeholders::_1, wbuf, s));
    wbuf->putc(s[2], std::bind(handler_putc, std::placeholders::_1, wbuf, s));
    wbuf->putc(s[3], std::bind(handler_putc, std::placeholders::_1, wbuf, s));
}


template<typename StreamBufferTypePtr, typename CharType>
void test_streambuf_getn(StreamBufferTypePtr rbuf, const std::vector<CharType>& contents)
{
    BOOST_CHECK_EQUAL(true, rbuf->can_read());

    std::size_t size = contents.size();
    auto ptr = new CharType[size];
    auto handler_getn = [](size_t count, StreamBufferTypePtr buf, const std::vector<CharType> contents, CharType* ptr)
    {
        BOOST_CHECK_NE(count, 0);
        BOOST_CHECK_EQUAL(std::equal(contents.begin(), contents.end(), ptr), true);

        auto handler_nodata = [](size_t count, StreamBufferTypePtr buf, CharType* ptr)
        {
            BOOST_CHECK_EQUAL(count, 0);
            buf->close();
            BOOST_CHECK_EQUAL(false, buf->can_read());
            auto handler_fin = [](size_t count, StreamBufferTypePtr buf, CharType* ptr)
            {
                BOOST_CHECK_EQUAL(count, 0);
                delete [] ptr;
                finish_test();
            };
            buf->getn(ptr, 1, std::bind(handler_fin, std::placeholders::_1, buf, ptr));
        };
        buf->getn(ptr, contents.size(), std::bind(handler_nodata, std::placeholders::_1, buf, ptr));
    };
    rbuf->getn(ptr, size, std::bind(handler_getn, std::placeholders::_1, rbuf, contents, ptr));
}

/// specialized producer_consumer_buffer test function
template<>
void test_streambuf_getn<prod_cons_buf_ptr, uint8_t>(prod_cons_buf_ptr rbuf, const std::vector<uint8_t>& contents)
{
    BOOST_CHECK_EQUAL(true, rbuf->can_read());

    std::size_t size = contents.size();
    auto ptr = new uint8_t[size];
    auto handler_getn = [](size_t count, prod_cons_buf_ptr buf, const std::vector<uint8_t> contents, uint8_t* ptr)
    {
        BOOST_CHECK_NE(count, 0);
        BOOST_CHECK_EQUAL(std::equal(contents.begin(), contents.end(), ptr), true);
        buf->close();
        BOOST_CHECK_EQUAL(false, buf->can_read());

        auto handler_fin = [](size_t count, prod_cons_buf_ptr buf, uint8_t* ptr)
        {
            BOOST_CHECK_EQUAL(count, 0);
            delete [] ptr;
            finish_test();
        };
        buf->getn(ptr, contents.size(), std::bind(handler_fin, std::placeholders::_1, buf, ptr));
    };
    rbuf->getn(ptr, size, std::bind(handler_getn, std::placeholders::_1, rbuf, contents, ptr));
}

template<typename StreamBufferTypePtr, typename CharType>
void test_streambuf_getc(StreamBufferTypePtr rbuf, CharType contents)
{
    BOOST_CHECK_EQUAL(true, rbuf->can_read());

    auto handler_getc = [](CharType ch, CharType contents, StreamBufferTypePtr rbuf, bool done)
    {
        BOOST_CHECK_EQUAL(contents, ch);
        if (done)
        {
            rbuf->close();
            BOOST_CHECK_EQUAL(false, rbuf->can_read());
            auto handler_fin = [](CharType ch, StreamBufferTypePtr rbuf)
            {
                BOOST_CHECK_EQUAL(ch, (CharType)snode::streams::char_traits<CharType>::eof());
                finish_test();
            };
            rbuf->getc(std::bind(handler_fin, std::placeholders::_1, rbuf));
        }
    };
    rbuf->getc(std::bind(handler_getc, std::placeholders::_1, contents, rbuf, false));
    rbuf->getc(std::bind(handler_getc, std::placeholders::_1, contents, rbuf, true));
}

template<typename StreamBufferTypePtr, typename CharType>
void test_streambuf_sgetc(StreamBufferTypePtr rbuf, CharType contents)
{
    BOOST_CHECK_EQUAL(true, rbuf->can_read());
    auto c = rbuf->sgetc();
    BOOST_CHECK_EQUAL(c, contents);
    BOOST_CHECK_EQUAL(c, rbuf->sgetc());
    rbuf->close();
    BOOST_CHECK_EQUAL(false, rbuf->can_read());
    BOOST_CHECK_EQUAL((CharType)snode::streams::char_traits<CharType>::eof(), rbuf->sgetc());
}

template<typename StreamBufferTypePtr, typename CharType>
void test_streambuf_bumpc(StreamBufferTypePtr rbuf, const std::vector<CharType>& contents)
{
    BOOST_CHECK_EQUAL(true, rbuf->can_read());
    auto handler = [](CharType ch, CharType contents, StreamBufferTypePtr rbuf)
    {
        if ((CharType)snode::streams::char_traits<CharType>::eof() == ch)
        {
            rbuf->close();
            BOOST_CHECK_EQUAL(false, rbuf->can_read());
            auto handler_fin = [](CharType ch, StreamBufferTypePtr rbuf)
            {
                BOOST_CHECK_EQUAL(ch, (CharType)snode::streams::char_traits<CharType>::eof());
                finish_test();
            };
            rbuf->bumpc(std::bind(handler_fin, std::placeholders::_1, rbuf));
            return;
        }
        BOOST_CHECK_EQUAL(ch, contents);
        BOOST_CHECK_NE(rbuf->sgetc(), contents);
    };
    for (auto& it : contents)
        rbuf->bumpc(std::bind(handler, std::placeholders::_1, it, rbuf));
}

template<typename StreamBufferTypePtr, typename CharType>
void streambuf_sbumpc(StreamBufferTypePtr rbuf, const std::vector<CharType>& contents)
{
    BOOST_CHECK_EQUAL(true, rbuf->can_read());
    auto c = rbuf->sbumpc();
    BOOST_CHECK_EQUAL(c, contents[0]);

    auto d = rbuf->sbumpc();
    size_t index = 1;
    while (d != (CharType)snode::streams::char_traits<CharType>::eof())
    {
        BOOST_CHECK_EQUAL(d, contents[index]);
        d = rbuf->sbumpc();
        index++;
    }

    rbuf->close();
    BOOST_CHECK_EQUAL(false, rbuf->can_read());
    BOOST_CHECK_EQUAL((CharType)snode::streams::char_traits<CharType>::eof(), rbuf->sbumpc()) ;
}

template<typename StreamBufferTypePtr, typename CharType>
void streambuf_nextc(StreamBufferTypePtr rbuf, const std::vector<CharType>& contents)
{
    BOOST_CHECK_EQUAL(true, rbuf->can_read());
    auto handler = [](CharType ch, CharType contents, StreamBufferTypePtr rbuf)
    {
        if ((CharType)snode::streams::char_traits<CharType>::eof() == ch)
        {
            rbuf->close();
            BOOST_CHECK_EQUAL(false, rbuf->can_read());
            auto handler_fin = [](CharType ch, StreamBufferTypePtr rbuf)
            {
                BOOST_CHECK_EQUAL(ch, (CharType)snode::streams::char_traits<CharType>::eof());
                finish_test();
            };
            rbuf->nextc(std::bind(handler_fin, std::placeholders::_1, rbuf));
            return;
        }
        BOOST_CHECK_EQUAL(ch, contents);
        BOOST_CHECK_NE(rbuf->sgetc(), contents);
    };
    for (auto& it : contents)
        rbuf->nextc(std::bind(handler, std::placeholders::_1, it + 1, rbuf));
}

template<typename StreamBufferTypePtr, typename CharType>
void streambuf_ungetc(StreamBufferTypePtr rbuf, const std::vector<CharType>& contents)
{
    BOOST_CHECK_EQUAL(true, rbuf->can_read());

    auto handler = [](CharType ch, std::vector<CharType> contents, StreamBufferTypePtr rbuf)
    {
        // ungetc from the begining should return eof
        BOOST_CHECK_EQUAL(ch , (CharType)snode::streams::char_traits<CharType>::eof());
        VERIFY_ARE_EQUAL(contents[0], rbuf->sbumpc());
        VERIFY_ARE_EQUAL(contents[1], rbuf->sgetc());

        auto handler_fin = [](CharType ch, std::vector<CharType> contents, StreamBufferTypePtr rbuf)
        {
            // ungetc could be unsupported!
            if (ch != (CharType)snode::streams::char_traits<CharType>::eof())
            {
                BOOST_CHECK_EQUAL(contents[0], ch);
            }
            rbuf->close();
            BOOST_CHECK_EQUAL(false, rbuf->can_read());
            finish_test();
        };
        rbuf->ungetc(std::bind(handler_fin, std::placeholders::_1, contents, rbuf));
    };
    rbuf->ungetc(std::bind(handler, std::placeholders::_1, contents, rbuf));
}

template<typename StreamBufferTypePtr>
void test_streambuf_alloc_commit(StreamBufferTypePtr wbuf)
{
    BOOST_CHECK_EQUAL(true, wbuf->can_write());
    BOOST_CHECK_EQUAL(0, wbuf->in_avail());

    size_t allocSize = 10;
    size_t commitSize = 2;

    for (size_t i = 0; i < allocSize/commitSize; i++)
    {
        // Allocate space for 10 chars
        auto data = wbuf->alloc(allocSize);
        BOOST_ASSERT(data != nullptr);

        // commit 2
        wbuf->commit(commitSize);
        BOOST_CHECK_EQUAL((i+1)*commitSize, wbuf->in_avail());
    }

    BOOST_CHECK_EQUAL(allocSize, wbuf->in_avail());
    wbuf->close();
    BOOST_ASSERT(wbuf->can_write());
    finish_test();
}

template<typename StreamBufferTypePtr>
void test_streambuf_seek_write(StreamBufferTypePtr wbuf)
{
    BOOST_CHECK_EQUAL(true, wbuf.can_write());
    BOOST_CHECK_EQUAL(true, wbuf.can_seek());

    auto beg = wbuf->seekoff(0, std::ios_base::beg, std::ios_base::out);
    auto cur = wbuf->seekoff(0, std::ios_base::cur, std::ios_base::out);

    // current should be at the beginning
    BOOST_CHECK_EQUAL(beg, cur);

    auto end = wbuf->seekoff(0, std::ios_base::end, std::ios_base::out);
    BOOST_CHECK_EQUAL(end, wbuf->seekpos(end, std::ios_base::out));

    wbuf.close();
    BOOST_CHECK_EQUAL(false, wbuf->can_write());
    BOOST_CHECK_EQUAL(false, wbuf->can_seek());
    finish_test();
}

void test_producer_consumer_getc()
{
    uint8_t data[] = {'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd'};
    std::vector<uint8_t> s(std::begin(data), std::end(data));
    prod_cons_buf_ptr buf = create_producer_consumer_buffer_with_data(s);
    test_streambuf_getc(buf, s[0]);
}

void test_producer_consumer_sgetc()
{
    uint8_t data[] = {'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd'};
    std::vector<uint8_t> s(std::begin(data), std::end(data));
    prod_cons_buf_ptr buf = create_producer_consumer_buffer_with_data(s);
    test_streambuf_sgetc(buf, s[0]);
}

void test_producer_consumer_getn()
{
    uint8_t data[] = {'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd'};
    std::vector<uint8_t> s(std::begin(data), std::end(data));
    prod_cons_buf_ptr buf = create_producer_consumer_buffer_with_data(s);
    test_streambuf_getn(buf, s);
}

void test_producer_consumer_putn()
{
    prod_cons_buf_ptr buf = std::make_shared<prod_cons_buf_type>(512);
    test_streambuf_putn(buf);
}

void test_producer_consumer_putn_getn()
{
    prod_cons_buf_ptr buf = std::make_shared<prod_cons_buf_type>(512);
    test_streambuf_putn_getn(buf);
}

void test_producer_consumer_putc()
{
    prod_cons_buf_ptr buf = std::make_shared<prod_cons_buf_type>(512);
    test_streambuf_putc(buf);
}

void test_producer_consumer_bumpc()
{
    uint8_t data[] = {'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd'};
    std::vector<uint8_t> s(std::begin(data), std::end(data));
    prod_cons_buf_ptr buf = create_producer_consumer_buffer_with_data(s);
    test_streambuf_bumpc(buf, s);
}

void test_producer_consumer_sbumpc()
{
    uint8_t data[] = {'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd'};
    std::vector<uint8_t> s(std::begin(data), std::end(data));
    prod_cons_buf_ptr buf = create_producer_consumer_buffer_with_data(s);
    test_streambuf_bumpc(buf, s);
}

void test_producer_consumer_nextc()
{
    uint8_t data[] = {'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd'};
    std::vector<uint8_t> s(std::begin(data), std::end(data));
    prod_cons_buf_ptr buf = create_producer_consumer_buffer_with_data(s);
    test_streambuf_bumpc(buf, s);
}

void test_producer_consumer_alloc_commt()
{
    prod_cons_buf_ptr buf = std::make_shared<prod_cons_buf_type>(512);
    test_streambuf_alloc_commit(buf);
}

// unit test entry point
test_suite*
init_unit_test_suite( int argc, char* argv[] )
{
    const char* config_path = "/home/emo/workspace/snode/src/conf.xml";
    BOOST_TEST_MESSAGE( "Starting tests" );

    snode::snode_core& server = snode::snode_core::instance();
    server.init(config_path);
    if (server.get_config().error())
    {
        BOOST_THROW_EXCEPTION( std::logic_error(server.get_config().error().message().c_str()) );
    }

    // boost::unit_test::unit_test_log_t::instance().set_threshold_level( boost::unit_test::log_successful_tests );

    auto test_case_producer_consumer_putn = std::bind(&async_streambuf_test_base, test_producer_consumer_putn);
    auto test_case_producer_consumer_putc = std::bind(&async_streambuf_test_base, test_producer_consumer_putc);
    auto test_case_producer_consumer_getn = std::bind(&async_streambuf_test_base, test_producer_consumer_getn);
    auto test_case_producer_consumer_putn_getn = std::bind(&async_streambuf_test_base, test_producer_consumer_putn_getn);
    auto test_case_producer_consumer_getc = std::bind(&async_streambuf_test_base, test_producer_consumer_getc);
    auto test_case_producer_consumer_sgetc = std::bind(&async_streambuf_test_base, test_producer_consumer_sgetc);
    auto test_case_producer_consumer_bumpc = std::bind(&async_streambuf_test_base, test_producer_consumer_bumpc);
    auto test_case_producer_consumer_alloc_commt = std::bind(&async_streambuf_test_base, test_producer_consumer_alloc_commt);

    framework::master_test_suite().add(BOOST_TEST_CASE(test_case_producer_consumer_putn));
    framework::master_test_suite().add(BOOST_TEST_CASE(test_case_producer_consumer_putc));
    framework::master_test_suite().add(BOOST_TEST_CASE(test_case_producer_consumer_getn));
    framework::master_test_suite().add(BOOST_TEST_CASE(test_case_producer_consumer_putn_getn));
    framework::master_test_suite().add(BOOST_TEST_CASE(test_case_producer_consumer_getc));
    framework::master_test_suite().add(BOOST_TEST_CASE(test_case_producer_consumer_sgetc));
    framework::master_test_suite().add(BOOST_TEST_CASE(test_case_producer_consumer_bumpc));
    framework::master_test_suite().add(BOOST_TEST_CASE(test_case_producer_consumer_alloc_commt));

    return 0;
}


