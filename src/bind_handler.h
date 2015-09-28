//
// detail/bind_handler.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2015 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef _BIND_HANDLER_H_
#define _BIND_HANDLER_H_

// only C++11
#define SNODE_MOVE_CAST(type) static_cast<type&&>

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "snode_types.h"

namespace snode {

template <typename Handler, typename Arg1>
class binder1
{
public:
    binder1(const Handler& handler, const Arg1& arg1)
      : handler_(handler),
        arg1_(arg1)
    {
    }

    binder1(Handler& handler, const Arg1& arg1)
      : handler_(SNODE_MOVE_CAST(Handler)(handler)),
        arg1_(arg1)
    {
    }

    void operator()()
    {
        handler_(static_cast<const Arg1&>(arg1_));
    }

    void operator()() const
    {
        handler_(arg1_);
    }

private:
    Handler handler_;
    Arg1 arg1_;
};


template <typename Handler, typename Arg1, typename Arg2>
class binder2
{
public:
    binder2(const Handler& handler, const Arg1& arg1, const Arg2& arg2)
      : handler_(handler),
        arg1_(arg1),
        arg2_(arg2)
    {
    }

    binder2(Handler& handler, const Arg1& arg1, const Arg2& arg2)
      : handler_(SNODE_MOVE_CAST(Handler)(handler)),
        arg1_(arg1),
        arg2_(arg2)
    {
    }

    void operator()()
    {
        handler_(static_cast<const Arg1&>(arg1_),
          static_cast<const Arg2&>(arg2_));
    }

    void operator()() const
    {
        handler_(arg1_, arg2_);
    }

private:
    Handler handler_;
    Arg1 arg1_;
    Arg2 arg2_;
};

template <typename Handler, typename Arg1, typename Arg2>
inline binder2 bind(const Handler& handler, const Arg1& arg1, const Arg2& arg2)
{
  return boost_asio_handler_alloc_helpers::allocate(
      size, this_handler->handler_);
}


template <typename Handler, typename Arg1, typename Arg2, typename Arg3>
class binder3
{
public:
    binder3(const Handler& handler, const Arg1& arg1, const Arg2& arg2,
        const Arg3& arg3)
      : handler_(handler),
        arg1_(arg1),
        arg2_(arg2),
        arg3_(arg3)
    {
    }

    binder3(Handler& handler, const Arg1& arg1, const Arg2& arg2,
        const Arg3& arg3)
      : handler_(SNODE_MOVE_CAST(Handler)(handler)),
        arg1_(arg1),
        arg2_(arg2),
        arg3_(arg3)
    {
    }

    void operator()()
    {
        handler_(static_cast<const Arg1&>(arg1_),
                 static_cast<const Arg2&>(arg2_),
                 static_cast<const Arg3&>(arg3_));
    }

    void operator()() const
    {
      handler_(arg1_, arg2_, arg3_);
    }

private:
    Handler handler_;
    Arg1 arg1_;
    Arg2 arg2_;
    Arg3 arg3_;
};

template <typename Handler, typename Arg1, typename Arg2, typename Arg3>
inline void* asio_handler_allocate(std::size_t size,
    binder3<Handler, Arg1, Arg2, Arg3>* this_handler)
{
  return boost_asio_handler_alloc_helpers::allocate(
      size, this_handler->handler_);
}

template <typename Handler, typename Arg1, typename Arg2, typename Arg3,
          typename Arg4>
class binder4
{
public:
    binder4(const Handler& handler, const Arg1& arg1, const Arg2& arg2,
        const Arg3& arg3, const Arg4& arg4)
      : handler_(handler),
        arg1_(arg1),
        arg2_(arg2),
        arg3_(arg3),
        arg4_(arg4)
    {
    }

    binder4(Handler& handler, const Arg1& arg1, const Arg2& arg2,
        const Arg3& arg3, const Arg4& arg4)
      : handler_(SNODE_MOVE_CAST(Handler)(handler)),
        arg1_(arg1),
        arg2_(arg2),
        arg3_(arg3),
        arg4_(arg4)
    {
    }

    void operator()()
    {
        handler_(static_cast<const Arg1&>(arg1_),
                 static_cast<const Arg2&>(arg2_),
                 static_cast<const Arg3&>(arg3_),
                 static_cast<const Arg4&>(arg4_));
    }

    void operator()() const
    {
        handler_(arg1_, arg2_, arg3_, arg4_);
    }

private:
    Handler handler_;
    Arg1 arg1_;
    Arg2 arg2_;
    Arg3 arg3_;
    Arg4 arg4_;
};

template <typename Handler, typename Arg1, typename Arg2, typename Arg3,
          typename Arg4>
inline void* asio_handler_allocate(std::size_t size,
    binder4<Handler, Arg1, Arg2, Arg3, Arg4>* this_handler)
{
  return boost_asio_handler_alloc_helpers::allocate(
      size, this_handler->handler_);
}


template <typename Handler, typename Arg1, typename Arg2, typename Arg3,
          typename Arg4, typename Arg5>
class binder5
{
public:
  binder5(const Handler& handler, const Arg1& arg1, const Arg2& arg2,
      const Arg3& arg3, const Arg4& arg4, const Arg5& arg5)
    : handler_(handler),
      arg1_(arg1),
      arg2_(arg2),
      arg3_(arg3),
      arg4_(arg4),
      arg5_(arg5)
  {
  }

  binder5(Handler& handler, const Arg1& arg1, const Arg2& arg2,
      const Arg3& arg3, const Arg4& arg4, const Arg5& arg5)
    : handler_(SNODE_MOVE_CAST(Handler)(handler)),
      arg1_(arg1),
      arg2_(arg2),
      arg3_(arg3),
      arg4_(arg4),
      arg5_(arg5)
  {
  }

  void operator()()
  {
      handler_(static_cast<const Arg1&>(arg1_),
               static_cast<const Arg2&>(arg2_),
               static_cast<const Arg3&>(arg3_),
               static_cast<const Arg4&>(arg4_),
               static_cast<const Arg5&>(arg5_));
  }

  void operator()() const
  {
      handler_(arg1_, arg2_, arg3_, arg4_, arg5_);
  }

private:
    Handler handler_;
    Arg1 arg1_;
    Arg2 arg2_;
    Arg3 arg3_;
    Arg4 arg4_;
    Arg5 arg5_;
};

template <typename Handler, typename Arg1, typename Arg2, typename Arg3,
          typename Arg4, typename Arg5>
inline void* asio_handler_allocate(std::size_t size,
    binder5<Handler, Arg1, Arg2, Arg3, Arg4, Arg5>* this_handler)
{
  return boost_asio_handler_alloc_helpers::allocate(
      size, this_handler->handler_);
}

} // namespace snode


#endif // _BIND_HANDLER_H_
