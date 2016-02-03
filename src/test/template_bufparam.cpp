#include <iostream>
#include <vector>
#include <cstdint>

template<typename CharType, typename Impl>
class streambuf_base
{
public:
    typedef CharType char_type;
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

template<typename CharType>
class specific_streambuf1 : public streambuf_impl<CharType>
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

int main()
{
    char a = 'b';
    streambuf_base<char, streambuf_impl<char>> buf;
    buf.foo(a);

    specific_streambuf<char> spbuf;
    spbuf.foo(a);

    specific_streambuf1<char> spbuf1;
    spbuf1.foo(a);

    async_istream<specific_streambuf<char> > s(&spbuf);
    char b[10];
    s.read(b, 10);

    return 0;
}
