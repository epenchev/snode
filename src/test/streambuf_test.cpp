#include <iostream>
#include <string>
#include <functional>
#include <algorithm>

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

typedef uint8_t char_type;
typedef snode::streams::producer_consumer_buffer<char_type> prod_cons_buf_type;
typedef std::shared_ptr<prod_cons_buf_type> prod_cons_buf_ptr;
typedef std::shared_ptr<uint8_t> buf_ptr;

void test_streambuf_putn()
{
    uint8_t data[] = {'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd'};
    prod_cons_buf_ptr wbuf = std::make_shared<prod_cons_buf_type>(512);

    BOOST_TEST_MESSAGE( "test_streambuf_putn start" );

    auto handler_putn = [](size_t count, prod_cons_buf_ptr buf)
    {
        const uint8_t data_template[] = {'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd'};
        uint8_t target[sizeof(data_template) + 1] = {0};
        BOOST_REQUIRE_NE( count, 0 );
        size_t rdbytes = buf->scopy(target, sizeof(target) - 1);
        BOOST_REQUIRE_NE( rdbytes, 0 );
        BOOST_CHECK_EQUAL(target, data_template);
        BOOST_TEST_MESSAGE( "test_streambuf_putn end" );

        snode::snode_core& server = snode::snode_core::instance();
        server.stop();
    };
    wbuf->putn(data, sizeof(data), std::bind(handler_putn, std::placeholders::_1, wbuf));
}

void test_streambuf_getn()
{
    uint8_t data[] = {'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd'};
    auto size = sizeof(data);

    BOOST_TEST_MESSAGE( "test_streambuf_getn start" );

    prod_cons_buf_ptr rbuf = std::make_shared<prod_cons_buf_type>(512);
    buf_ptr ptarget = std::make_shared<uint8_t>(size + 1);
    std::fill(ptarget.get(), ptarget.get() + (size + 1), 0);

    auto target = rbuf->alloc(size);
    BOOST_ASSERT( target == nullptr );

    std::copy(data, data + size, target);
    rbuf->commit(size);

    auto handler_getn = [](size_t count, buf_ptr target, prod_cons_buf_ptr buf)
    {
        const uint8_t data_template[] = {'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd'};
        BOOST_REQUIRE_NE( count, 0 );

        BOOST_CHECK_EQUAL( target.get(), data_template );

        BOOST_TEST_MESSAGE( "test_streambuf_getn end" );
        snode::snode_core& server = snode::snode_core::instance();
        server.stop();
    };
    rbuf->getn(ptarget.get(), size, std::bind(handler_getn, std::placeholders::_1, ptarget, rbuf));

}

int async_streambuf_test_function()
{
    const char* config_path = "/home/emo/workspace/snode/src/conf.xml";
    BOOST_TEST_MESSAGE( "Starting test" );

    snode::snode_core& server = snode::snode_core::instance();
    server.init(config_path);

    if (!server.get_config().error())
    {
        auto threads = snode::snode_core::instance().get_threadpool().threads();
        auto th = threads.begin()->get();
        snode::async_task::connect(test_streambuf_putn, th->get_id());
        server.run();
        snode::async_task::connect(test_streambuf_getn, th->get_id());
        server.run();
    }
    else
    {
        BOOST_ERROR( server.get_config().error().message() );
    }

    return 0;
}

// unit test entry point
test_suite*
init_unit_test_suite( int argc, char* argv[] )
{
    boost::unit_test::unit_test_log_t::instance().set_threshold_level( boost::unit_test::log_successful_tests );
    framework::master_test_suite().add( BOOST_TEST_CASE( &async_streambuf_test_function ) );
    return 0;
}


