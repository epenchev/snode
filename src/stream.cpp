//
// stream.cpp
// Copyright (C) 2015  Emil Penchev, Bulgaria

#include <stream.h>

namespace smkit
{

stream_config::stream_config(xml_reader::iterator& it) : m_conf(&it)
{
}

stream_config::~stream_config()
{
}

std::string stream_config::get_name() const
{
    return m_conf->get_value<std::string>("name");
}

xml_reader::iterator& stream_config::get_xml_config()
{
    return *m_conf;
}

player::player() : m_stream(NULL), m_source(NULL), m_filter(NULL)
{
}

player::~player()
{
}

void player::play(stream_config& st)
{

}

} // end of namespace


