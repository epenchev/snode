//
// snode_types.h
// Copyright (C) 2015  Emil Penchev, Bulgaria


#ifndef _SNODE_TYPES_H_
#define _SNODE_TYPES_H_

#include <boost/asio.hpp>
#include <memory>
#include "thread_wrapper.h"

namespace snode
{

typedef std::shared_ptr<snode::lib::thread> thread_ptr;
typedef std::shared_ptr<boost::asio::io_service> io_service_ptr;
typedef std::shared_ptr<boost::asio::ip::tcp::socket> tcp_socket_ptr;
typedef std::shared_ptr<boost::asio::ip::tcp::acceptor> tcp_acceptor_ptr;
typedef std::shared_ptr<boost::asio::deadline_timer> timer_ptr;
typedef snode::lib::thread::id thread_id_t;

}
#endif /* _SNODE_TYPES_H_ */
