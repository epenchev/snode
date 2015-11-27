#include <iostream>
#include <string>
#include <functional>

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

// Globals
static const char* s_config_path = "../conf.xml";
typedef snode::streams::producer_consumer_buffer<uint8_t> prod_cons_buf_type;
typedef prod_cons_buf_type::traits buff_traits;
const size_t bufsize = 512;
const prod_cons_buf_type::char_type sample_buf[bufsize] = {"HI producer_consumer buffer, just testing here."};
prod_cons_buf_type::char_type io_buf[bufsize] = {0};
prod_cons_buf_type buf(bufsize);
prod_cons_buf_type target(bufsize);

void handler_getn(size_t count);
void handler_putn(size_t count);

void handler_putn(size_t count)
{
    BOOST_TEST_MESSAGE( "putn() completion handler" );
    BOOST_CHECK_EQUAL( count, bufsize );
    buf.getn(io_buf, bufsize, handler_getn);
}

void handler_getn(size_t count)
{
    BOOST_TEST_MESSAGE( "getn() completion handler" );
    BOOST_CHECK_EQUAL( count, bufsize );
    snode::snode_core::instance().stop();
}

void test_streambuf_putn_getn()
{
    BOOST_TEST_MESSAGE( "test_streambuf_putn_getn start" );
    buf.putn(sample_buf, bufsize, handler_putn);
}

int async_streambuf_test_function()
{
    BOOST_TEST_MESSAGE( "Starting test" );

    snode::snode_core& server = snode::snode_core::instance();
    server.init(s_config_path);

    if (!server.get_config().error())
    {
        auto threads = snode::snode_core::instance().get_threadpool().threads();
        auto thread_count = threads.size();
        snode::async_task::connect(test_streambuf_putn_getn, threads[0]->get_id());
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


