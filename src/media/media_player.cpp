//
// media_player.cpp
// Copyright (C) 2015  Emil Penchev, Bulgaria

#include "media_player.h"
#include <functional>

namespace snode
{
namespace media
{

void media_player::play()
{
    if (source_->stream().is_open())
    {
        auto buf = source_->stream().streambuf();
        buf.getn(databuf_.data(), databuf_.size(),
            std::bind(&media_player::read_handler, this, std::placeholders::_1, buf));
    }
    else if (source_->live_stream().is_open())
    {
        auto livebuf = source_->live_stream().streambuf();
        livebuf.getn(databuf_.data(), databuf_.size(),
            std::bind(&media_player::read_handler_live, this, std::placeholders::_1, livebuf));
    }
}

void media_player::pause()
{
}

void media_player::stop()
{
}


void media_player::record(const std::string& filepath)
{
}

void media_player::seek(float ts)
{
}

size_t media_player::size() const
{
    return 0;
}

const std::string& media_player::name() const
{
    return "";
}

void media_player::read_handler(size_t count)
{
    auto buf = source_->stream().streambuf();
    buf.getn(databuf_.data(), databuf_.size(),
        std::bind(&media_player::read_handler, this, std::placeholders::_1, buf));
}

void media_player::read_handler_live(size_t count)
{
    auto livebuf = source_->live_stream().streambuf();
    livebuf.getn(databuf_.data(), databuf_.size(),
        std::bind(&media_player::read_handler_live, this, std::placeholders::_1, livebuf));
}


player_factory::player_ptr player_factory::create(const std::string& name, const std::string& source_class,
                                  const std::string& filter_class = "", const std::string& filter_options = "")
{
    auto search = players_.find(name);
    if (search != players_.end())
    {
        return search->second;
    }

    if (name.empty() || source_class.empty)
        return player_ptr(nullptr);

    media_player::source_ptr sourceptr(source_factory::create_instance(source_class));
    if (!sourceptr)
        return player_ptr(nullptr);

    if (!filter_class.empty() && !filter_options.empty())
    {
        media_player::filter_ptr filterptr(filter_factory::create_instance(filter_class));
#if 0
        if (filterptr)
            filterptr->set_option(filter_options);
#endif
        auto player = std::make_shared<media_player>(name, sourceptr, filterptr);
        players_[name] = player;
        return player;
    }

    auto player = std::make_shared<media_player>(name, sourceptr);
    players_[name] = player;
    return player;
}

} // end namespace media
} // end namespace snode

