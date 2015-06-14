#include <iostream>
#include <vector>
#include <queue>

struct some_handler
{
    void operator() (std::size_t size)
    {
        std::cout << "size is : " << size << std::endl;
    }
};

class operation
{
public:
    void complete(std::size_t size)
    {
        func_(this, size);
    }

protected:
    typedef void (*func_type)(operation*, std::size_t);
    operation(func_type func) : func_(func)
    {
    }

private:       
    func_type func_;
};

template <typename Handler>
class wait_operation : public operation
{
public:
    wait_operation(Handler& h) : operation(&wait_operation::do_complete)
    {}

    static void do_complete(operation* base, std::size_t size)
    {
        wait_operation* op(static_cast<wait_operation*>(base));
        op->handler_(size);
    }
private:
    Handler handler_;
};

// global
std::queue<operation> g_op_queue;

template <typename Handler>
void async_wait(Handler& handler)
{
    // Allocate and construct an operation to wrap the handler.
    wait_operation<Handler> op(handler);
    g_op_queue.push(op);
}

int main()
{
    some_handler handler;
    async_wait(handler);
    g_op_queue.front().complete(456);

    return 0;
}





