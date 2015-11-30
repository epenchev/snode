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

typedef snode::streams::producer_consumer_buffer<uint8_t> prod_cons_buf_type;
typedef prod_cons_buf_type::traits buff_traits;
typedef std::shared_ptr<prod_cons_buf_type> prod_cons_buf_ptr;
typedef std::shared_ptr<uint8_t> buf_ptr;

void test_streambuf_putn_getn()
{
    const size_t bufsize = 512;
    const std::string sample_data("HI producer_consumer buffer, just testing here.");
    prod_cons_buf_ptr buf = std::make_shared<prod_cons_buf_type>(bufsize);

    BOOST_TEST_MESSAGE( "test_streambuf_putn_getn start" );

    std::function<void(size_t, prod_cons_buf_ptr, buf_ptr)> handler_putn = [](size_t count, prod_cons_buf_ptr buf, buf_ptr target)
    {
        buf_ptr io_buf = std::make_shared<uint8_t>(bufsize);
        BOOST_CHECK_EQUAL( count, bufsize );

        std::function<void(size_t, prod_cons_buf_ptr, buf_ptr)> handler_getn = [](size_t count, prod_cons_buf_ptr buf, buf_ptr target)
        {
            BOOST_TEST_MESSAGE( "getn() completion handler" );
            BOOST_CHECK_EQUAL( count, bufsize );
            std::string io_str((const char*)target.get());
            BOOST_CHECK_EQUAL( io_str.compare("HI producer_consumer buffer, just testing here."), 0 );
            snode::snode_core::instance().stop();
        };
        buf->getn(io_buf.get(), bufsize, std::bind(handler_getn, std::placeholders::_1, buf));
    };
    buf->putn((const prod_cons_buf_type::char_type*)sample_data.c_str(), sample_data.size(), std::bind(handler_putn, std::placeholders::_1, buf));
}

int async_streambuf_test_function()
{
    const char* config_path = "../conf.xml";
    BOOST_TEST_MESSAGE( "Starting test" );

    snode::snode_core& server = snode::snode_core::instance();
    server.init(config_path);

    if (!server.get_config().error())
    {
        auto threads = snode::snode_core::instance().get_threadpool().threads();
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


