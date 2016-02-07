#include <iostream>
#include <vector>
#include <cstdint>
#include <memory>

template<typename CharType, typename Impl>
class streambuf_base
{
public:
    
    typedef CharType char_type;

    streambuf_base()
    {
        std::cout << "streambuf_base() \n";
    }

    void foo(char_type ch)
    {
        std::cout << "streambuf_base::foo is called\n";
        static_cast<Impl*>(this)->foo(ch);
    }
};

template<typename CharType>
class streambuf_impl : public streambuf_base<CharType, streambuf_impl<CharType> >
{
public:
    typedef typename streambuf_base<CharType, streambuf_impl<CharType>>::char_type char_type;

    static streambuf_base<CharType, streambuf_impl<CharType>>* create_streambuf_impl()
    {
        return new streambuf_impl<CharType>();
    }

    streambuf_impl() : streambuf_base<CharType, streambuf_impl<CharType> >()
    {
        std::cout << "streambuf_impl() \n";
    }
    void foo(char_type ch)
    {
        std::cout << ch << std::endl;
    }
};

template<typename CharType>
class specific_streambuf : public streambuf_base<CharType, streambuf_impl<CharType> >
{
public:
};

template<typename Buff>
class async_istream
{
public:
    typedef typename Buff::char_type char_type;
    
    async_istream(Buff* buffer) : buffer_(buffer)
    {}

    void read(char_type* ptr, size_t size)
    {
    }
private:
    Buff* buffer_;
};

template<typename CharType>
streambuf_base<CharType, streambuf_impl<CharType>>* create_streambuf_impl()
{
    return new streambuf_impl<CharType>();
}

template<typename T>
class sample
{
public:
    template<typename TVar>
    void do_something(TVar var)
    {
        std::cout << sizeof(var) << std::endl;
    }

    void do_size(T var)
    {
        std::cout << sizeof(var) << std::endl;
    }
};

int main()
{
    //char a = 'b';
    std::string w("юююююю");
    std::cout << w << std::endl;
    std::cout << w.length() << std::endl;
    std::cout << w[0] << w[1] << std::endl;
    unsigned int w0 = w[0];
    unsigned int w1 = w[1];
    std::cout << (w0 & 0xff) << std::endl;
    std::cout << (w1 & 0xff) << std::endl;
 
    //auto sbuf = create_streambuf_impl<char>();
    //auto sbuf = streambuf_impl<char>::create_streambuf_impl();
    //sbuf->foo(a);

    /*
    async_istream<specific_streambuf<char> > s(&spbuf);
    char b[10];
    s.read(b, 10);
    */

    sample<char> sm;
       

    return 0;
}
