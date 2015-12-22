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
//#define BOOST_TEST_NO_MAIN

#include <boost/test/included/unit_test.hpp>
using namespace boost::unit_test;

/*
 * shell compile
 *  g++ -std=c++11 -g -Wall -I../ streambuf_test.cpp ../config_reader.o ../http_helpers.o ../http_msg.o ../http_service.o ../snode_core.o ../uri_utils.o
 *  -o streambuf_test -lpthread -lboost_system -lboost_thread
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

void test_streambuf_putn()
{
    uint8_t data[] = {'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd'};
    prod_cons_buf_ptr wbuf = std::make_shared<prod_cons_buf_type>(512);

    BOOST_MESSAGE("Start test_streambuf_putn");
    auto handler_putn = [](size_t count, prod_cons_buf_ptr buf)
    {
        const uint8_t data_template[] = {'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd'};
        uint8_t target[sizeof(data_template) + 1] = {0};
        BOOST_REQUIRE_NE( count, 0 );
        size_t rdbytes = buf->scopy(target, sizeof(target) - 1);
        BOOST_REQUIRE_NE( rdbytes, 0 );
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

        BOOST_REQUIRE_NE( count, 0 );
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
        BOOST_MESSAGE("End test_streambuf_putc");
        if ('e' == ch )
        {
            BOOST_CHECK_EQUAL( buf->in_avail(), 5 );
            s_block = false;
        }
    };

    wbuf->putc('a', std::bind(handler_putc, std::placeholders::_1, wbuf));
    wbuf->putc('b', std::bind(handler_putc, std::placeholders::_1, wbuf));
    wbuf->putc('c', std::bind(handler_putc, std::placeholders::_1, wbuf));
    wbuf->putc('d', std::bind(handler_putc, std::placeholders::_1, wbuf));
    wbuf->putc('e', std::bind(handler_putc, std::placeholders::_1, wbuf));
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
    framework::master_test_suite().add( BOOST_TEST_CASE( boost::bind(&async_streambuf_test_base, test_streambuf_putn) ) );
    framework::master_test_suite().add( BOOST_TEST_CASE( boost::bind(&async_streambuf_test_base, test_streambuf_getn) ) );
    framework::master_test_suite().add( BOOST_TEST_CASE( boost::bind(&async_streambuf_test_base, test_streambuf_putc) ) );

    return 0;
}


