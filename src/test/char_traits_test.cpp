#include <string>
#include <iostream>
#include <cstdint>

template<typename CharType>
struct char_traits : std::char_traits<CharType>
{
    static typename std::char_traits<CharType>::int_type requires_async() { return std::char_traits<CharType>::eof()-1; }
};

template<typename CharType>
class some_test
{
public:
    typedef CharType char_type;
    typedef std::char_traits<CharType> traits;
    typedef typename traits::int_type int_type;
    typedef typename traits::pos_type pos_type;
    typedef typename traits::off_type off_type;
};

class some_test_derived : public some_test<int>
{
public:
    void foo()
    {
    }
};

int main()
{
    /*
    std::cout << sizeof(char_traits<uint8_t>::eof()) << std::endl;
    std::cout << sizeof(char_traits<int>::eof()) << std::endl;
    std::cout << sizeof(char_traits<int>::int_type) <<  std::endl;
    std::cout << sizeof(char_traits<int>::off_type) << std::endl;
    std::cout << sizeof(char_traits<int>::pos_type) <<  std::endl;
    */
    std::cout << sizeof(some_test_derived::off_type) << std::endl;
    return 0;
}
