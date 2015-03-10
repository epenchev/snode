//
// stream.h
// Copyright (C) 2015  Emil Penchev, Bulgaria

#ifndef STREAM_H_
#define STREAM_H_

#include <string>
#include <buffer.h>
#include <xml_reader.h>
#include <reg_factory.h>
//#include <boost/shared_ptr.hpp>

namespace smkit
{

///  media stream configuration and basic stream management.
class stream_config
{
public:
    stream_config(xml_reader::iterator& it);
    ~stream_config();

    std::string get_name() const;
    xml_reader::iterator& get_xml_config();

private:
    xml_reader::iterator* m_conf;
};

///  Input interface to media source.
class stream_source
{
public:
    /// Factory creator method, to be overridden by subclasses.
    static stream_source* create_object() { return NULL; }

    class listener
    {
    public:
        virtual void on_read(smkit::pod_buffer* data) {}
    };

    virtual void init(stream_config& conf) = 0;
    virtual unsigned long get_size() = 0;

    virtual void open(const std::string& location) = 0;
    virtual void do_read(smkit::pod_buffer& out_buf, unsigned long& bytes_read) = 0;
    virtual void seek(long unsigned int offset) = 0;
    // For live streams
    virtual void add_listener(stream_source::listener* listener)  = 0;
    virtual ~stream_source()
    {
    }
};

class stream_filter
{
public:
    /// Factory creator method, to be overridden by subclasses.
    static stream_filter* create_object() { return NULL; }

    virtual smkit::pod_buffer* process(smkit::pod_buffer* in_data) = 0;
    virtual void init(stream_config& conf) = 0;
    virtual ~stream_filter()
    {
    }
};

class player : public stream_source::listener
{
public:

    /// For live streams subscription
    class listener
    {
        public:
            virtual void on_read(smkit::pod_buffer* data) = 0;
    };

    /// Factories for registering stream_source and stream_filter classes implemented as modules.
    typedef reg_factory<stream_source> source_factory_t;
    typedef reg_factory<stream_filter> filter_factory_t;

    ///  play a media stream.
    void play(smkit::stream_config& st);

    ///  pause accepting data from a stream.
    void pause();

    ///  seek to a current location in a stream specified as a bytes offset
    void seek(int bytes);

    /// seek to a current location in a stream specified as a time stamp offset in seconds.
    void seek(float timeStamp);

    ///  return player packet buffer
    smkit::pod_buffer* get();

    /// from stream_source::listener for live streams subscription
    void on_read(smkit::pod_buffer* data) {}

    void add_live_stream_listener(player::listener* listener);

    size_t size() const;

private:
    friend class server_app;
    player();
    virtual ~player();

    stream_config*  m_stream;
    stream_source*  m_source;
    stream_filter*  m_filter;
};

}// end namespace smkit

#endif /* STREAM_H_ */
