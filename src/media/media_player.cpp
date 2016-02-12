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
     : source_(source), filter_(filter)
{
    instream_ = source_->stream();
    instreamlive_ = source_->live_stream();
}

void media_player::play()
{
    if (instream_.is_open())
        instream_.read(playerbuf_, 1024, std::bind(&media_player::read_handler, this, std::placeholders::_1));
    else if (instreamlive_.is_open())
        instreamlive_.read(playerbuf_, 1024, std::bind(&media_player::read_handler_live, this, std::placeholders::_1));
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

media_player::stream_type& media_player::stream()
{
    media_player::stream_type s();
    return s;
}

void media_player::read_handler(size_t count)
{
    instream_.read(playerbuf_, 1024, std::bind(&media_player::read_handler, this, std::placeholders::_1));
}

void media_player::read_handler_live(size_t count)
{
    instreamlive_.read(playerbuf_, 1024, std::bind(&media_player::read_handler_live, this, std::placeholders::_1));
}


player_factory::player_ptr player_factory::create(const std::string& name, const std::string& source_class,
                                                  const std::string& filter_class = "", const std::string& filter_options = "")
{
    // check if we have a player with that name
    auto search = players_.find(name);
    if (players_.end() == search)
    {
        // must have a stream name and a specified source class
        if (!name.empty() && !source_class.empty())
        {
            media_player::source_ptr source(source_factory::create_instance(source_class));
            if (!source)
                return player_ptr(nullptr);

            if (!filter_class.empty() && !filter_options.empty())
            {
                media_player::filter_ptr filter(filter_factory::create_instance(filter_class));
                if (filter)
                    filter->set_option(filter_options);

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
    }
    return player_ptr(nullptr);
}

} // end namespace media
} // end namespace snode

