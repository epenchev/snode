//
// server_app.cpp
// Copyright (C) 2015  Emil Penchev, Bulgaria

#include <server_app.h>
#include <task.h>
#include <iostream>
#include <utility>
#include <http_headers.h>
#include <http_msg.h>

using namespace boost::asio::ip;

#define THREADS_CONF_SECT        "threads"
#define LOGFILE_CONF_SECT        "logfile"

#define SERVERS_CONF_SECT        "servers"
#define SERVER_ADDRESS_SUBSECT   "address"
#define SERVER_PORT_SUBSECT      "port"
#define SERVER_NAME_SUBSECT      "name"

#define STREAMS_CONF_SECT        "streams"
#define STREAMS_NAME_SUBSECT     "name"
#define STREAMS_LOCATION_SUBSECT "location"

namespace smkit
{

void socket_acceptor::start()
{
    if (m_acceptor.is_open())
        return; // already started

    m_acceptor.open(boost::asio::ip::tcp::v4());
    if (!m_host.empty())
    {
        boost::asio::ip::address ip_address;
        ip_address.from_string(m_host);
        m_acceptor.bind(tcp::endpoint(ip_address, m_port));
    }
    else
        m_acceptor.bind(tcp::endpoint(tcp::v4(), m_port));
    m_acceptor.set_option(tcp::acceptor::reuse_address(true));
    m_acceptor.listen();
    this->accept();
}

void socket_acceptor::stop()
{
    if (m_acceptor.is_open())
    {
        // no error handling here just disable exceptions
        boost::system::error_code error;
        m_acceptor.close(error);
    }
}

void socket_acceptor::accept()
{
    task_scheduler& scheduler = task_scheduler::instance();
    thread_id_t tid = scheduler.next_thread();
    tcp::socket* socket = new tcp::socket(scheduler.get_thread_io_service(tid));
    m_acceptor.async_accept(*socket, boost::bind(&socket_acceptor::handle_accept, this, socket, boost::asio::placeholders::error, tid));
}

void socket_acceptor::handle_accept(boost::asio::ip::tcp::socket* socket, const boost::system::error_code& err, thread_id_t tid)
{
    if (err || !m_server)
        delete socket;
    else
        m_server->on_accept(tcp_socket_ptr(socket), tid);

    // Continue accepting new connections.
    this->accept();
}

server_app::server_app()
{
}

void server_app::init(const std::string& filename)
{
    m_config_reader.open(filename);
    int thread_count = m_config_reader.get_value<int>(THREADS_CONF_SECT);
    xml_reader::iterator it;
    for (it = m_config_reader.begin(SERVERS_CONF_SECT); it != m_config_reader.end(SERVERS_CONF_SECT); it++)
    {
        std::string address = it.get_value<std::string>(SERVER_ADDRESS_SUBSECT);
        unsigned int netport = it.get_value(SERVER_PORT_SUBSECT, 0);
        socket_acceptor* acceptor = new socket_acceptor(address, netport, m_io_service, this);
        m_acceptors.push_back(acceptor);
        for (int i = 0; i < thread_count; i++)
        {
            // create a server handler for each thread
            server_app::server_handler* handler = server_handler_factory_t::create_instance(it.get_value<std::string>(SERVER_NAME_SUBSECT));
            m_srv_handlers.push_back(std::make_pair(netport, handler));
        }
    }

    for (it = m_config_reader.begin(STREAMS_CONF_SECT); it != m_config_reader.end(STREAMS_CONF_SECT); it++)
    {
        smkit::stream_config* stream = new smkit::stream_config(it);
        m_streams[it.get_value<std::string>(STREAMS_NAME_SUBSECT)] = stream;
    }
}

void server_app::run()
{
    int thread_count = m_config_reader.get_value<int>(THREADS_CONF_SECT);
    task_scheduler::instance().run(thread_count);

    std::list<socket_acceptor*>::iterator it;
    for (it = m_acceptors.begin(); it != m_acceptors.end(); it++)
        (*it)->start();

    // make server handler thread_id association
    std::list<std::pair<unsigned int, server_handler*> >::iterator it_handlers;
    for (it_handlers = m_srv_handlers.begin(); it_handlers != m_srv_handlers.end(); it_handlers++)
        m_srv_handlers_assoc[task_scheduler::instance().next_thread()][it_handlers->first] = it_handlers->second;

    m_io_service.run();
}

void server_app::on_accept(tcp_socket_ptr socket, thread_id_t tid)
{
    boost::system::error_code err;
    boost::asio::ip::tcp::endpoint endpoint = socket->remote_endpoint(err);
    if (!err)
        std::cout << "[DEBUG] Connected from " << endpoint.port() << std::endl;
}

} // end of namespace


