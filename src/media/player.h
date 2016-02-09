// player.h
// Copyright (C) 2016  Emil Penchev, Bulgaria


#ifndef PLAYER_H_
#define PLAYER_H_

#include <string>
#include "source.h"

namespace snode
{
namespace media
{

/// Common player interface for all sorts of streams.
class player
{
public:
    typedef media::source::stream_type stream_type;
    typedef media::source::live_stream_type live_stream_type;
    typedef media_source::off_type off_type;

    player()
    {}

    virtual ~player()
    {}

    void play(const std::string& streamName)
    {}

    void pause(const std::string& streamName)
    {}

    void seek(off_type offset)
    {}

    void seek(float ts)
    {}

    stream_type& stream()
    {}

    live_stream_type& live_stream()
    {}

private:
    media::source source_;
    media::filter filter_;
};

} // end namespace media
} // end namespace snode


#endif /* PLAYER_H_ */
