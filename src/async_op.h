/*
 * async_op.h
 *
 *  Created on: Dec 16, 2015
 *      Author: emo
 */

#ifndef SRC_ASYNC_OP_H_
#define SRC_ASYNC_OP_H_

class async_op_base
{
public:

    void run()
    {
        func_(this);
    }

protected:
    typedef void (*func_type)(async_op_base*);
    async_op_base(func_type func_op) : func_(func_op)
    {}

private:
    func_type func_;
};

template<typename Handler>
class async_op : public async_op_base
{
public:

    async_op(Handler h)
      : async_op_base(&async_op::do_run), handler_(h)
    {}

    static void do_run(async_op_base* base)
    {
        async_op* op(static_cast<async_op*>(base));
        op->handler_();
    }

private:
    Handler handler_;
};

#endif /* SRC_ASYNC_OP_H_ */
