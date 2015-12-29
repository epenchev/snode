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
int async_streambuf_test_base(test_func_type func)
{
    auto threads = snode::snode_core::instance().get_threadpool().threads();
    auto thread = threads.begin()->get();
    snode::async_task::connect(func, thread->get_id());
    s_block = true;
    while (s_block)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return 0;
}

prod_cons_buf_ptr create_producer_consumer_buffer_with_data(const std::vector<uint8_t> & s)
{
    prod_cons_buf_ptr buf = std::make_shared<prod_cons_buf_type>(512);
    auto target = buf->alloc(s.size());
    BOOST_ASSERT( target != nullptr );
    std::copy(s.begin(), s.end(), target);
    buf->commit(s.size());
    buf->close(std::ios_base::out);
    return buf;
}


void test_streambuf_putn()
{
    uint8_t data[] = {'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd'};
    prod_cons_buf_ptr wbuf = std::make_shared<prod_cons_buf_type>(512);

    BOOST_MESSAGE("Start test_streambuf_putn");
    auto handler_putn = [](size_t count, prod_cons_buf_ptr buf)
    {
        const uint8_t data_template[] = {'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd'};
        uint8_t target[sizeof(data_template) + 1] = {0};
        BOOST_CHECK_NE( count, 0 );
        size_t rdbytes = buf->scopy(target, sizeof(target) - 1);
        BOOST_CHECK_NE( rdbytes, 0 );
        BOOST_CHECK_EQUAL( std::equal(data_template, data_template + sizeof(data_template), target), true );
        BOOST_MESSAGE("End test_streambuf_putn");
        s_block = false;
    };
    wbuf->putn(data, sizeof(data), std::bind(handler_putn, std::placeholders::_1, wbuf));
}

void test_streambuf_getn()
{
    uint8_t data[] = {'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd'};
    auto size = sizeof(data);

    BOOST_MESSAGE("Start test_streambuf_getn");

    prod_cons_buf_ptr rbuf = std::make_shared<prod_cons_buf_type>(512);
    buf_ptr ptarget = std::make_shared<uint8_t>(size + 1);
    std::fill(ptarget.get(), ptarget.get() + (size + 1), 0);
    auto target = rbuf->alloc(size);
    BOOST_ASSERT( target != nullptr );
    std::copy(data, data + size, target);
    rbuf->commit(size);

    auto handler_getn = [](size_t count, buf_ptr target, prod_cons_buf_ptr buf)
    {
        const uint8_t data_template[] = {'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd'};

        BOOST_CHECK_NE( count, 0 );
        BOOST_CHECK_EQUAL( std::equal(data_template, data_template + sizeof(data_template), target.get()), true );
        BOOST_MESSAGE("End test_streambuf_getn");
        s_block = false;
    };
    rbuf->getn(ptarget.get(), size, std::bind(handler_getn, std::placeholders::_1, ptarget, rbuf));
}

void test_streambuf_putc()
{
    BOOST_MESSAGE("Start test_streambuf_putc");

    prod_cons_buf_ptr wbuf = std::make_shared<prod_cons_buf_type>(512);
    auto handler_putc = [](prod_cons_buf_type::char_type ch,  prod_cons_buf_ptr buf)
    {
        BOOST_CHECK_NE( prod_cons_buf_type::traits::eof(), ch );
        if ('e' == ch )
        {
            BOOST_CHECK_EQUAL( buf->in_avail(), 5 );
            BOOST_MESSAGE("End test_streambuf_putc");
            s_block = false;
        }
    };

    wbuf->putc('a', std::bind(handler_putc, std::placeholders::_1, wbuf));
    wbuf->putc('b', std::bind(handler_putc, std::placeholders::_1, wbuf));
    wbuf->putc('c', std::bind(handler_putc, std::placeholders::_1, wbuf));
    wbuf->putc('d', std::bind(handler_putc, std::placeholders::_1, wbuf));
    wbuf->putc('e', std::bind(handler_putc, std::placeholders::_1, wbuf));
}

void test_streambuf_alloc_commit()
{
    BOOST_MESSAGE("Start test_streambuf_alloc_commit");

    prod_cons_buf_ptr wbuf = std::make_shared<prod_cons_buf_type>(512);
    BOOST_CHECK_EQUAL(true, wbuf->can_write());
    BOOST_CHECK_EQUAL(0,    wbuf->in_avail());

    size_t allocSize = 10;
    size_t commitSize = 2;

    for (size_t i = 0; i < allocSize/commitSize; i++)
    {
        // Allocate space for 10 chars
        auto data = wbuf->alloc(allocSize);
        BOOST_ASSERT( data != nullptr );

        // commit 2
        wbuf->commit(commitSize);
        BOOST_CHECK_EQUAL( (i+1)*commitSize, wbuf->in_avail() );
    }

    BOOST_CHECK_EQUAL(allocSize, wbuf->in_avail());
    wbuf->close();
    BOOST_ASSERT(wbuf->can_write());
    BOOST_MESSAGE("End test_streambuf_alloc_commit");
    s_block = false;
}

void test_streambuf_seek_write()
{
    BOOST_MESSAGE("Start test_streambuf_seek_write");

    prod_cons_buf_type wbuf(512);
    BOOST_CHECK_EQUAL(true, wbuf.can_write());
    BOOST_CHECK_EQUAL(true, wbuf.can_seek());

    // auto beg = wbuf.seekoff(0, std::ios_base::beg, std::ios_base::out);з
    // auto cur = wbuf.seekoff(0, std::ios_base::cur, std::ios_base::out);

    // current should be at the begining
    //BOOST_CHECK_EQUAL(beg, cur);

    //auto end = wbuf.seekoff(0, std::ios_base::end, std::ios_base::out);
    //BOOST_CHECK_EQUAL(end, wbuf.seekpos(end, std::ios_base::out));

    wbuf.close();
    BOOST_MESSAGE("End test_streambuf_seek_write");
    //VERIFY_IS_FALSE(wbuf.can_write());
    //VERIFY_IS_FALSE(wbuf.can_seek());
}

template<class StreamBufferTypePtr, class CharType>
void test_streambuf_getc(StreamBufferTypePtr rbuf, CharType contents)
{
    BOOST_MESSAGE("Start test_streambuf_getc");
    BOOST_CHECK_EQUAL(true, rbuf->can_read());

    auto handler_getc = [](CharType ch, CharType contents, StreamBufferTypePtr rbuf)
    {
        BOOST_CHECK_NE( snode::streams::char_traits<CharType>::eof(), ch );
        BOOST_CHECK_EQUAL( contents, ch );
        BOOST_CHECK_EQUAL( contents, rbuf->sgetc() );
        rbuf->close();
        BOOST_CHECK_EQUAL( false, rbuf->can_read());
        BOOST_CHECK_EQUAL( true, rbuf->can_read());
        BOOST_CHECK_EQUAL( snode::streams::char_traits<CharType>::eof(), rbuf->sgetc() );
        BOOST_MESSAGE("End test_streambuf_alloc_commit");
        s_block = false;
    };
    rbuf->getc(std::bind(handler_getc, std::placeholders::_1, contents, rbuf));
}

void test_producer_consumer_getc()
{
    uint8_t data[] = {'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd'};
    std::vector<uint8_t> s(std::begin(data), std::end(data));
    prod_cons_buf_ptr buf = create_producer_consumer_buffer_with_data(s);
    test_streambuf_getc(buf, s[0]);
}

template<class StreamBufferType>
void streambuf_sgetc(StreamBufferType& rbuf, typename StreamBufferType::char_type contents)
{
    VERIFY_IS_TRUE(rbuf.can_read());

    auto c = rbuf.sgetc();

    VERIFY_ARE_EQUAL(c, contents);

    // Calling getc again should return the same character (getc do not advance read head)
    VERIFY_ARE_EQUAL(c, rbuf.sgetc());

    rbuf.close().get();
    VERIFY_IS_FALSE(rbuf.can_read());

    // sgetc should return eof after close
    VERIFY_ARE_EQUAL(StreamBufferType::traits::eof(), rbuf.sgetc());
}

template<class StreamBufferType>
void streambuf_bumpc(StreamBufferType& rbuf, const std::vector<typename StreamBufferType::char_type>& contents)
{
    VERIFY_IS_TRUE(rbuf.can_read());

    auto c = rbuf.bumpc().get();

    VERIFY_ARE_EQUAL(c, contents[0]);

    // Calling bumpc again should return the next character
    // Read till eof
    auto d = rbuf.bumpc().get();

    size_t index = 1;

    while (d != StreamBufferType::traits::eof())
    {
        VERIFY_ARE_EQUAL(d, contents[index]);
        d = rbuf.bumpc().get();
        index++;
    }

    rbuf.close().get();
    VERIFY_IS_FALSE(rbuf.can_read());

    // operation should return eof after close
    VERIFY_ARE_EQUAL(StreamBufferType::traits::eof(), rbuf.bumpc().get());
}

template<class StreamBufferType>
void streambuf_sbumpc(StreamBufferType& rbuf, const std::vector<typename StreamBufferType::char_type>& contents)
{
    VERIFY_IS_TRUE(rbuf.can_read());

    auto c = rbuf.sbumpc();

    VERIFY_ARE_EQUAL(c, contents[0]);

    // Calling sbumpc again should return the next character
    // Read till eof
    auto d = rbuf.sbumpc();

    size_t index = 1;

    while (d != StreamBufferType::traits::eof())
    {
        VERIFY_ARE_EQUAL(d, contents[index]);
        d = rbuf.sbumpc();
        index++;
    }

    rbuf.close().get();
    VERIFY_IS_FALSE(rbuf.can_read());

    // operation should return eof after close
    VERIFY_ARE_EQUAL(StreamBufferType::traits::eof(), rbuf.sbumpc());
}

template<class StreamBufferType>
void streambuf_nextc(StreamBufferType& rbuf, const std::vector<typename StreamBufferType::char_type>& contents)
{
    VERIFY_IS_TRUE(rbuf.can_read());

    auto c = rbuf.nextc().get();

    VERIFY_ARE_EQUAL(c, contents[1]);

    // Calling getc should return the same contents as before.
    VERIFY_ARE_EQUAL(c, rbuf.getc().get());

    size_t index = 1;

    while (c != StreamBufferType::traits::eof())
    {
        VERIFY_ARE_EQUAL(c, contents[index]);
        c = rbuf.nextc().get();
        index++;
    }

    rbuf.close().get();
    VERIFY_IS_FALSE(rbuf.can_read());

    // operation should return eof after close
    VERIFY_ARE_EQUAL(StreamBufferType::traits::eof(), rbuf.nextc().get());
}

template<class StreamBufferType>
void streambuf_ungetc(StreamBufferType& rbuf, const std::vector<typename StreamBufferType::char_type>& contents)
{
    VERIFY_IS_TRUE(rbuf.can_read());

    // ungetc from the begining should return eof
    VERIFY_ARE_EQUAL(StreamBufferType::traits::eof(), rbuf.ungetc().get());

    VERIFY_ARE_EQUAL(contents[0], rbuf.bumpc().get());
    VERIFY_ARE_EQUAL(contents[1], rbuf.getc().get());

    auto c = rbuf.ungetc().get();

    // ungetc could be unsupported!
    if (c != StreamBufferType::traits::eof())
    {
        VERIFY_ARE_EQUAL(contents[0], c);
    }

    rbuf.close().get();
    VERIFY_IS_FALSE(rbuf.can_read());
}


// unit test entry point
test_suite*
init_unit_test_suite( int argc, char* argv[] )
{
    const char* config_path = "/home/emo/workspace/snode/src/conf.xml";
    BOOST_TEST_MESSAGE( "Starting test" );

    snode::snode_core& server = snode::snode_core::instance();
    server.init(config_path);
    if (server.get_config().error())
    {
        BOOST_THROW_EXCEPTION( std::logic_error(server.get_config().error().message().c_str()) );
    }

    boost::unit_test::unit_test_log_t::instance().set_threshold_level( boost::unit_test::log_successful_tests );
    framework::master_test_suite().add( BOOST_TEST_CASE( std::bind(&async_streambuf_test_base, test_streambuf_putn) ) );
    framework::master_test_suite().add( BOOST_TEST_CASE( std::bind(&async_streambuf_test_base, test_streambuf_getn) ) );
    framework::master_test_suite().add( BOOST_TEST_CASE( std::bind(&async_streambuf_test_base, test_streambuf_putc) ) );
    framework::master_test_suite().add( BOOST_TEST_CASE( std::bind(&async_streambuf_test_base, test_streambuf_alloc_commit) ) );
    framework::master_test_suite().add( BOOST_TEST_CASE( std::bind(&async_streambuf_test_base, test_producer_consumer_getc) ) );

    return 0;
}


