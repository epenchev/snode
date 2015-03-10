//
// async_streams.h
// Copyright (C) 2015  Emil Penchev, Bulgaria

#ifndef ASYNC_STREAMS_H_
#define ASYNC_STREAMS_H_

#include <boost/asio.hpp>

namespace smkit
{

typedef boost::asio::streambuf streambuf;

class async_istream
{
public:
	/// Default constructor
	async_istream()
	{
	}

	/// Constructor
	async_istream(streambuf& buffer);

};

}

#endif /* ASYNC_STREAMS_H_ */
