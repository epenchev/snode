//
// buffer.h
// Copyright (C) 2015  Emil Penchev, Bulgaria

#ifndef BUFFER_H_
#define BUFFER_H_

#include <cstdlib>
#include <cstring>

namespace smkit
{

class pod_buffer
{
public:
    pod_buffer() : m_buf(NULL), m_size(0), m_allocated(false)
    {
    }

    pod_buffer(void* data, std::size_t size) : m_buf(data), m_size(size), m_allocated(false)
    {
    }

    pod_buffer(std::size_t size) : m_buf(new unsigned char[size]), m_size(size), m_allocated(true)
    {
    }

    virtual ~pod_buffer()
    {
        if (m_allocated && m_buf)
            delete static_cast<unsigned char*>(m_buf);
    }

    std::size_t size() const
    {
        return m_size;
    }

    template <typename T>
    T get()
    {
        return static_cast<T>(m_buf);
    }

    ///  Create a new modifiable buffer that is offset from the start of another.
    friend smkit::pod_buffer operator + (smkit::pod_buffer& source_buf, std::size_t offset)
    {
        if (offset < source_buf.size())
        {
            smkit::pod_buffer buf(source_buf.get<unsigned char*>() + offset, source_buf.size() - offset);
            return buf;
        }
        else
        {
            smkit::pod_buffer buf;
            return buf;
        }
    }

    ///  Create a new modifiable buffer that is offset from the start of another.
    friend smkit::pod_buffer operator + (std::size_t offset, smkit::pod_buffer& source_buf)
    {
        return (source_buf + offset);
    }

protected:
    void*        m_buf;
    std::size_t  m_size;
    bool         m_allocated;
};

} // end of smkit

#endif /* BUFFER_H_ */
