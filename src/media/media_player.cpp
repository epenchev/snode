//
// media_player.cpp
// Copyright (C) 2015  Emil Penchev, Bulgaria

#include "media_player.h"
#include <functional>

namespace snode
{
namespace media
{

media_player::media_player(const std::string& name, source_ptr source, filter_ptr filter = nullptr)
     : streambuf_(media_player::buf_size), source_(source), filter_(filter)
{
    stream_ = source_->stream();
    streamlive_ = source_->live_stream();
}

void media_player::play()
{
    if (stream_.is_open())
        stream_.read(streambuf_, media_player::buf_size, std::bind(&media_player::read_handler, this, std::placeholders::_1));
    else if (streamlive_.is_open())
        streamlive_.read(streambuf_, media_player::buf_size, std::bind(&media_player::read_handler_live, this, std::placeholders::_1));
}

void media_player::pause()
{
    bool cancel_read = true;
}

void media_player::stop()
{
    bool cancel_read = true;
}

void media_player::record(const std::string& filepath)
{
}

void media_player::seek(float ts)
{
}

void media_player::seek(off_type offset)
{
    // TODO
    // stream_.seek(offset);
}

/// TO Be REMOVED !!!!
size_t media_player::read(media_source::char_type* ptr, size_t count, off_type offset = -1)
{
    /*
    size_t res = 0;
    if (ptr && count > 0)
    {
        size_t rcount = streambuf_.in_avail();
        if (rcount > count)
            rcount = count;
        res = streambuf_.scopy(ptr, rcount);
    }*/

    return 0;
}

size_t media_player::size()
{
    return 0;
}
const std::string& media_player::name() const
{
    return name_;
}

media_player::stream_type media_player::stream()
{
    return streambuf_.create_istream();
}

void media_player::read_handler(size_t count)
{
    stream_.read(streambuf_, media_player::buf_size, std::bind(&media_player::read_handler, this, std::placeholders::_1));
}

void media_player::read_handler_live(size_t count)
{
    streamlive_.read(streambuf_, media_player::buf_size, std::bind(&media_player::read_handler_live, this, std::placeholders::_1));
}

player_factory::player_ptr player_factory::create(const std::string& name, const std::string& source_type,
                                                  const std::string& filter_type = "", const std::string& filter_opt = "")
{
    // check if we have a player with that name
    auto search = players_.find(name);
    if (players_.end() != search)
        return player_ptr(nullptr);

    // must have a stream name and a source type
    if (name.empty() || source_type.empty())
        return player_ptr(nullptr);

    media_player::source_ptr source(source_factory::create_instance(source_type));
    if (!source)
        return player_ptr(nullptr);

    if (!filter_type.empty() && !filter_opt.empty())
    {
        media_player::filter_ptr filter(filter_factory::create_instance(filter_type));
        if (filter)
            filter->set_option(filter_opt);

        auto player = std::make_shared<media_player>(name, source, filter);
        players_[name] = player;
        return player;
    }
    else
    {
        auto player = std::make_shared<media_player>(name, source);
        players_[name] = player;
        return player;
    }
}

} // end namespace media
} // end namespace snode

