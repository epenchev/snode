/*
 * reg_factory_template_test.cpp
 *
 *  Created on: Sep 29, 2015
 *      Author: emo
 */

#include "../reg_factory.h"
#include <iostream>

// compile
// g++ -Wall reg_factory_template_test.cpp -o reg_factory_template_test

class service_base
{
public:
    static service_base* create_object() { return NULL; }

    void on_request(int req)
    {
        func_(this, req);
    }
protected:

    typedef void (*base_handler_func)(service_base*, int);
    service_base(base_handler_func handler_func) : func_(handler_func)
    {}

    base_handler_func func_;
};

template <typename Handler>
class service : public service_base
{
public:

    static void handle_req(service_base* base, int req)
    {
        service<Handler>* handler(static_cast<service<Handler>*>(base));
        handler->handler_.handle_request(req);
    }

    service(Handler h) : service_base(&service::handle_req), handler_(h)
    {}
private:
    Handler handler_;
};

// user implementation for the service
class some_service
{
public:
    void handle_request(int req)
    {
        std::cout << "handle_request from some_service " << req << " " <<  std::endl;
    }
};

// user implementation for the class to register in the factory (factory class)
class service_base_wrapper
{
public:
    static service_base* create_object()
    {
        static some_service ss;
        static service<some_service> s(ss);
        return &s;
    }
};

// main system class exporting the factory
class system_core
{
public:
    /// Factory for registering all the server handler classes.
    typedef snode::reg_factory<service_base> service_factory;

    void execute_service()
    {
        service_base* s = service_factory::create_instance("example");
        if (s)
        {
            s->on_request(145);
        }
    }
};

system_core::service_factory::registrator<service_base_wrapper> service_reg("example");

int main()
{
    //some_service ss;
    //service<some_service> s(ss);
    //s.on_request(14);

    system_core core;
    core.execute_service();

    return 0;
}
