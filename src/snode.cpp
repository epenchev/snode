#include <iostream>
#include "threadpool.h"
#include "async_task.h"
#include "async_streams.h"
#include "producer_consumer_buf.h"

void handle_buff_write(char ch)
{
    std::cout << ch << std::endl;
}

int main()
{
    // just for test
    // snode::snode_core& snode = snode::snode_core::instance();

    snode::streams::producer_consumer_buffer<uint8_t> buf(512);
    snode::streams::producer_consumer_buffer<uint8_t>::base_stream_type::istream_type is = buf.create_istream();
    snode::streams::producer_consumer_buffer<uint8_t>::base_stream_type::ostream_type os = buf.create_ostream();
    buf.putc('A', handle_buff_write);
    //
    // just for test
    snode::snode_core& snode = snode::snode_core::instance();
    snode.run();

    return 0;
}
