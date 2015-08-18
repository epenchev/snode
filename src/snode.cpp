#include <iostream>
#include "threadpool.h"
#include "async_task.h"
#include "async_streams.h"
#include "producer_consumer_buf.h"

int main()
{
    // just for test
    snode::streams::producer_consumer_buffer<char> buf(512);
    //
    snode::snode_core& snode = snode::snode_core::instance();
    snode.run();
    return 0;
}
