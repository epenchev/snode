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

//____________________________________________________________________________//

BOOST_AUTO_TEST_CASE( async_streambuf_prod_consumer_test )
{
    // global initialization
    snode::snode_core& snode = snode::snode_core::instance();
    snode::streams::producer_consumer_buffer<uint8_t> buf(512);
    typedef snode::streams::producer_consumer_buffer<uint8_t>::traits buff_traits;

    // write char
    boost::function<void (char)> handler_putc = [](char ch) {
                                                              BOOST_CHECK_EQUAL( ch, sample_char );
                                                              std::cout << "putc() test 1 passed \n";
                                                            };
    buf.putc(sample_char, handler_putc);

    // read char
    boost::function<void (char)> handler_getc = [](char ch) {
                                                              std::cout << "Here \n";
                                                              //BOOST_CHECK_NE( ch, buff_traits::eof() );
                                                              BOOST_CHECK_EQUAL( ch, sample_char );
                                                              std::cout << "getc() test passed \n";
                                                            };
    buf.getc(handler_getc);

#if 0
    // bump char
    boost::function<void (char)> handler_bumpc = [](char ch) {
                                                               BOOST_CHECK_EQUAL( ch, sample_char );
                                                               std::cout << "bumpc() test 1 passed \n";
                                                             };
    buf.bumpc(handler_bumpc);
    boost::function<void (char)> handler_bumpc_eof = [](char ch) {
                                                                   BOOST_CHECK_EQUAL( ch, buff_traits::eof() );
                                                                   std::cout << "bumpc() test 2 passed \n";
                                                                 };
    buf.bumpc(handler_bumpc);

    // move to next char
    boost::function<void (char)> handler_nextc_eof = [](char ch) {
                                                                   BOOST_CHECK_EQUAL( ch, buff_traits::eof() );
                                                                   std::cout << "nextc() test 1 passed \n";
                                                                 };
    buf.nextc(handler_nextc_eof);
    buf.putc(sample_char, handler_putc);
    boost::function<void (char)> handler_nextc = [](char ch) {
                                                               BOOST_CHECK_NE( ch, buff_traits::eof() );
                                                               std::cout << "nextc() test 2 passed \n";
                                                             };
    buf.nextc(handler_nextc);

    // Retreat the read position
    boost::function<void (char)> handler_ungetc = [](char ch) {
                                                                BOOST_CHECK_EQUAL( ch, sample_char );
                                                                std::cout << "ungetc() test passed \n";
                                                              };
    buf.ungetc(handler_ungetc);

    // write data
    boost::function<void (size_t)> handler_putn = [](size_t count) {
                                                                     BOOST_CHECK_EQUAL( count, sample_string.size() );
                                                                     std::cout << "putn() test passed \n";
                                                                   };
    buf.putn((const unsigned char*)(sample_string.c_str()), sample_string.size(), handler_putn);

    // read data
    boost::function<void (size_t)> handler_getn = [](size_t count) {
                                                                     BOOST_CHECK_EQUAL( count, sample_string.size() );
                                                                     std::cout << "getn() test passed \n";
                                                                   };
    buf.getn(sample_membuf, sample_string.size(), handler_getn);
#endif
#if 0
    boost::function<void (char, snode::snode_core*)> handler_putc_stop = [](char ch, snode::snode_core* snode) {
                                                                                                                 BOOST_CHECK_EQUAL( ch, sample_char );
                                                                                                                 std::cout << "putc() test 2 passed \n";
                                                                                                                 snode->stop();
                                                                                                                 std::cout << "async_streambuf_prod_consumer_test finish \n";
                                                                                                                };
    buf.putc(sample_char, boost::bind(handler_putc_stop, _1, &snode));
#endif
    snode.run();
}

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


