//#include <iostream>
#include <string>
#include <functional>
#include "threadpool.h"
#include "async_task.h"
#include "async_streams.h"
#include "producer_consumer_buf.h"

#define BOOST_TEST_MODULE async_streams_test
#define BOOST_TEST_LOG_LEVEL all
#define BOOST_TEST_BUILD_INFO yes

#include <boost/test/included/unit_test.hpp>
using namespace boost::unit_test;

uint8_t     sample_char = 'A';
uint8_t     sample_membuf[1024] = {0};
std::string sample_string = "HI producer_consumer buffer, just testing here.";

// global initialization
snode::snode_core& server = snode::snode_core::instance();
snode::streams::producer_consumer_buffer<uint8_t> buf(512);
typedef snode::streams::producer_consumer_buffer<uint8_t>::traits buff_traits;

BOOST_AUTO_TEST_SUITE( test_suite_async_streambuf )

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

#if 0











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
#endif
BOOST_AUTO_TEST_SUITE_END()


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


