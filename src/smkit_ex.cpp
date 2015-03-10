//
// smkit_ex.cpp
// Copyright (C) 2013  Emil Penchev, Bulgaria

#include <server_app.h>

int main(int argc, char* argv[])
{
    smkit::server_app& g_app = smkit::server_app::instance();
    if (argc > 1)
    {
        std::string filename = argv[1];
        g_app.init(filename);
        g_app.run();
    }

    return 0;
}


