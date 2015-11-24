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

uint8_t     sample_char = 'A';
uint8_t     sample_membuf[1024] = {0};
std::string sample_string = "HI producer_consumer buffer, just testing here.";
/*
 * shell compile
 *  g++ -std=c++11 -g -Wall -I../ streambuf_test.cpp ../config_reader.o ../http_helpers.o ../http_msg.o ../http_service.o ../snode_core.o ../uri_utils.o
 *  -o streambuf_test -lpthread -lboost_system -lboost_thread
 *
 */

// global initialization


//BOOST_AUTO_TEST_SUITE( test_suite_async_streambuf )

#if 0
BOOST_AUTO_TEST_CASE( async_streambuf_putc )
{
    // write char
    boost::function<void (char)> handler_putc = [](char ch) {
                                                              BOOST_TEST_MESSAGE( "Start test putc()" );
                                                              BOOST_CHECK_EQUAL( ch, sample_char );
                                                              BOOST_TEST_MESSAGE( "End test putc()" );
                                                              server.stop();
                                                            };
    buf.putc(sample_char, handler_putc);
    server.run();
}

BOOST_AUTO_TEST_CASE( async_streambuf_getc )
{
    // read char
    boost::function<void (char)> handler_getc = [](char ch) {
                                                              BOOST_TEST_MESSAGE( "Start test getc()" );
                                                              BOOST_CHECK_NE( ch, buff_traits::eof() );
                                                              BOOST_CHECK_EQUAL( ch, sample_char );
                                                              BOOST_TEST_MESSAGE( "End test getc()" );
                                                              server.stop();
                                                            };
    buf.getc(handler_getc);
    server.run();
}

BOOST_AUTO_TEST_CASE( async_streambuf_bumpc )
{
    // bump char
    boost::function<void (char)> handler_bumpc = [](char ch) {
                                                               BOOST_TEST_MESSAGE( "Start test bumpc()" );
                                                               BOOST_CHECK_EQUAL( ch, sample_char );
                                                               BOOST_TEST_MESSAGE( "End test bumpc()" );
                                                               server.stop();
                                                             };
    buf.bumpc(handler_bumpc);
    server.run();
}

BOOST_AUTO_TEST_CASE( async_streambuf_putn )
{
    // write data
    boost::function<void (size_t)> handler_putn = [](size_t count) {
                                                                     BOOST_TEST_MESSAGE( "Start test putn()" );
                                                                     BOOST_CHECK_EQUAL( count, sample_string.size() );
                                                                     BOOST_TEST_MESSAGE( "End test putn()" );
                                                                     //server.stop();
                                                                   };
    buf.putn((const unsigned char*)(sample_string.c_str()), sample_string.size(), handler_putn);
    server.run();
}

BOOST_AUTO_TEST_CASE( async_streambuf_getn )
{
    // read data
    boost::function<void (size_t)> handler_getn = [](size_t count) {
                                                                     BOOST_TEST_MESSAGE( "Start test getn()" );
                                                                     BOOST_CHECK_EQUAL( count, sample_string.size() );
                                                                     BOOST_TEST_MESSAGE( "End test getn()" );
                                                                     server.stop();
                                                                   };
    buf.getn(sample_membuf, sample_string.size(), handler_getn);
    server.run();
}

//#if 0











    // move to next char
    boost::function<void (char)> handler_nextc_eof = [](char ch) {
                                                                   BOOST_TEST_MESSAGE( "Start test nextc() 1" );
                                                                   BOOST_CHECK_EQUAL( ch, buff_traits::eof() );
                                                                   BOOST_TEST_MESSAGE( "End test nextc() 1" );
                                                                 };
    buf.nextc(handler_nextc_eof);
    buf.putc(sample_char, handler_putc);
    boost::function<void (char)> handler_nextc = [](char ch) {
                                                               BOOST_TEST_MESSAGE( "Start test nextc() 2" );
                                                               BOOST_CHECK_NE( ch, buff_traits::eof() );
                                                               BOOST_TEST_MESSAGE( "End test nextc() 2" );
                                                             };
    buf.nextc(handler_nextc);

    // Retreat the read position
    boost::function<void (char)> handler_ungetc = [](char ch) {
                                                                BOOST_TEST_MESSAGE( "Start test ungetc()" );
                                                                BOOST_CHECK_EQUAL( ch, sample_char );
                                                                BOOST_TEST_MESSAGE( "End test ungetc()" );
                                                              };
    buf.ungetc(handler_ungetc);



    boost::function<void (char, snode::snode_core*)> handler_putc_stop = [](char ch, snode::snode_core* snode)
                                                                   {
                                                                     BOOST_TEST_MESSAGE( "Start test putc()" );
                                                                     BOOST_CHECK_EQUAL( ch, sample_char );
                                                                     BOOST_TEST_MESSAGE( "End test putc()" );
                                                                     snode->stop();
                                                                   };
    buf.putc(sample_char, boost::bind(handler_putc_stop, _1, &snode));
    snode.run();
}
BOOST_AUTO_TEST_SUITE_END()
#endif

#if 0
//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( sync_streambuf_prod_consumer_test )
{
    snode::snode_core& snode = snode::snode_core::instance();

    snode::streams::producer_consumer_buffer<uint8_t> buf(512);
    snode::streams::producer_consumer_buffer<uint8_t>::base_stream_type::istream_type is = buf.create_istream();
    snode::streams::producer_consumer_buffer<uint8_t>::base_stream_type::ostream_type os = buf.create_ostream();

    snode.run();
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( async_ostream_test )
{
    snode::snode_core& snode = snode::snode_core::instance();

    snode::streams::producer_consumer_buffer<uint8_t> buf(512);
    snode::streams::producer_consumer_buffer<uint8_t>::base_stream_type::istream_type is = buf.create_istream();
    snode::streams::producer_consumer_buffer<uint8_t>::base_stream_type::ostream_type os = buf.create_ostream();

    snode.run();
}

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( async_istream_test )
{
    snode::snode_core& snode = snode::snode_core::instance();

    snode::streams::producer_consumer_buffer<uint8_t> buf(512);
    snode::streams::producer_consumer_buffer<uint8_t>::base_stream_type::istream_type is = buf.create_istream();
    snode::streams::producer_consumer_buffer<uint8_t>::base_stream_type::ostream_type os = buf.create_ostream();

    snode.run();
}
#endif

snode::streams::producer_consumer_buffer<uint8_t>* p_buf;
static const char* s_config_path = "../conf.xml";
typedef snode::streams::producer_consumer_buffer<uint8_t>::traits buff_traits;

void handler_getn (size_t count/*, snode::snode_core& server*/)
{
    BOOST_TEST_MESSAGE( "getn() completion handler" );
    //BOOST_CHECK_EQUAL( sample_membuf, sample_string.c_str() );
    //server.stop();
}

void handler_putn (size_t count/*, snode::snode_core& server*/)
{
        BOOST_TEST_MESSAGE( "putn() completion handler" );
        BOOST_CHECK_EQUAL( count, sample_string.size() );
        p_buf->getn(sample_membuf, /*sizeof(sample_membuf)*/0, handler_getn/*boost::bind(&handler_getn, std::placeholders::_1, server)*/);
}

void test_func(snode::streams::producer_consumer_buffer<uint8_t>& buf, snode::snode_core& server)
{
	p_buf = &buf;
    buf.putn((const unsigned char*)(sample_string.c_str()), sample_string.size(), handler_putn /*boost::bind(&handler_putn, std::placeholders::_1, server)*/);
}

void handler_read_istream(size_t count)
{
    // todo
}

int free_test_function()
{
    // test buffer
    snode::streams::producer_consumer_buffer<uint8_t> buf(512);
    BOOST_TEST_MESSAGE( "Starting test" );

    snode::snode_core& server = snode::snode_core::instance();
    server.init(s_config_path);

    snode::streams::producer_consumer_buffer<uint8_t>::istream_type istr = buf.create_istream();
    snode::streams::producer_consumer_buffer<uint8_t> target(512);
    istr.read(target, 100, handler_read_istream);

    istr.read_to_delim(target, '\n', handler_read_istream);
    istr.read_line(target, handler_read_istream);
    //istr.read_to_end(target, handler_read_istream);

/*
    if (!server.get_config().error())
    {
        auto threads = snode::snode_core::instance().get_threadpool().threads();
        auto thread_count = threads.size();

        BOOST_TEST_MESSAGE( "thread : " << threads[0]->get_id() );
        snode::async_task::connect(test_func, std::ref(buf), std::ref(server), threads[0]->get_id());

        // block main thread
        server.run();
        BOOST_TEST_MESSAGE("Server stopped");
    }
    else
    {
        std::cout << server.get_config().error().message() << std::endl;
    }
*/
    return 0;
}

// unit test entry point
test_suite*
init_unit_test_suite( int argc, char* argv[] )
{
    boost::unit_test::unit_test_log_t::instance().set_threshold_level( boost::unit_test::log_successful_tests );
    framework::master_test_suite().add( BOOST_TEST_CASE( &free_test_function ) );
    return 0;
}


