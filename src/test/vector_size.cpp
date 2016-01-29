#include <iostream>
#include <vector>

template<typename Impl>
class derived;

class composed_into_base
{
public:
    composed_into_base()
    {
        std::cout << "composed_into_base() \n";
    }

    ~composed_into_base()
    {
        std::cout << "~composed_into_base() \n";
    }
};

class base
{
public:
    base() {}
    ~base() {  std::cout << "bye base \n"; }

    template<typename Impl>
    Impl& get_impl()
    {
        return static_cast<derived<Impl>*>(this)->impl();
    }

    composed_into_base& get_composed()
    {
        static composed_into_base obj;
        return obj;
    }

};

template<typename Impl>
class derived : public base
{
public:
    derived(Impl& impl) : base(), impl_(impl)
    {}

    Impl& impl() { return impl_; }

private:
    Impl& impl_;
};

class some_impl
{
public:
    void do_something()
    {
        std::cout << "do_something() \n";
    }
};

void foo(void* ptr)
{
    std::cout << "foo " << ptr << std::endl;
    auto ptr_1 = ptr + 1;
    std::cout << "foo + 1  " << ptr_1 << std::endl;
}

int main()
{
#if 0
    std::vector<unsigned char> v(18);
    std::cout << "size : " << v.size() << std::endl;
    v.reserve(512);
    std::cout << "after resize size : " << v.size() << std::endl;
#endif

#if 0
    some_impl objimpl;
    base* obj = new base();/*new derived<some_impl>(objimpl);*/
    auto imp = obj->get_impl<some_impl>();
    imp.do_something();
#endif

#if 0
    base obj2;
    base obj3;
    obj2.get_composed();
    obj3.get_composed();
    std::cout << "Here \n";
#endif
    std::vector<int> v(10);
    std::cout << v.size() << std::endl;
    std::cout << v.capacity() << std::endl;
    std::cout << v.data() << std::endl;
    //auto ptr = v.data() + 2;
    foo(v.data() + 1);

    //std::cout << (void*)ptr << std::endl;


    return 0;
}
