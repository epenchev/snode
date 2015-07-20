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
    snode::server_app& smkit = snode::server_app::instance();
    smkit.run();
    return 0;
}
