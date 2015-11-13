//
// snode_main.h
// Copyright (C) 2015  Emil Penchev, Bulgaria

#include "snode_core.h"
#include <iostream>

// main entry point
int main(int argc, char* argv[])
{
    snode::snode_core& server = snode::snode_core::instance();
    server.init(argv[1]);
    if (!server.get_config().error())
        server.run();
    else
        std::cout << server.get_config().error().message() << std::endl;

    return 0;
}
