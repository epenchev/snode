#include <iostream>
#include <vector>
#include <cstdint>

template<typename Impl>
class streambuf_base
{
public:
    //typedef typename Impl::char_type chtype;
    decltype (typename Impl::char_type) chp; 
    void foo(int ch)
    {
        decltype (ch) chp;
        chp = ch; 
        static_cast<Impl*>(this)->iml_foo(chp);
    }
};

template<typename CharType>
class streambuf_impl : public streambuf_base<streambuf_impl<CharType>>
{
public:
    typedef CharType char_type;

    void iml_foo(CharType ch)
    {
        std::cout << ch << std::endl;
    }
};

void some (void* p)
{
 // 
}

int main()
{
    char a = 'b';
    streambuf_base<streambuf_impl<char>> buf;
    buf.foo(a);
    int p;
    some(&p);

    return 0;
}
