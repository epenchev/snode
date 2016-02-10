// player.h
// Copyright (C) 2016  Emil Penchev, Bulgaria


#ifndef PLAYER_H_
#define PLAYER_H_

#include <string>
#include <memory>
#include "source.h"
#include "filter.h"
#include "reg_factory.h"

namespace snode
{
namespace media
{

/// Common player interface for all sorts of streams.
class player
{
public:
    typedef std::shared_ptr<media::source> source_ptr;
    typedef std::shared_ptr<media::filter> filter_ptr;
    typedef media::source::stream_type stream_type;
    typedef media::source::live_stream_type live_stream_type;
    typedef media_source::off_type off_type;

    player(const std::string& name, source_ptr sr, filter_ptr fl = nullptr) : source_(sr), filter_(fl)
    {}

    virtual ~player()
    {}

    /// Start playing the given stream
    void play();

    /// Put the stream on pause
    void pause();

    /// Start recording the stream to a given a location while it plays (DVR).
    void record(const std::string& filepath);
    
    /// Seek to a specific offset (in bytes) in the stream.
    /// Will not work on live streams.
    void seek(off_type offset);

    /// Seek to a specific time (in seconds) in the stream.
    /// Only vaild if this is multimedia stream and is not live.
    void seek(float ts);

    /// Get the name of the stream.
    const std::string& name() const;

    /// Get player's stream
    stream_type& stream() { return stream_; }

    /// Get player's livestream
    live_stream_type& live_stream() { return live_stream_; }

private:
    template<typename media::player> friend class streams::sourcebuf;

    /// Player is complying with SourceImpl interface
    size_t read(char_type* ptr, size_t count, off_type offset = -1);
    size_t size() const
    void close();
    
    std::string name_;
    stream_type stream_;
    live_stream_type live_stream_;
    media::source* source_;
    media::filter* filter_;
};

/// Stores all active players and creates new ones for a given media stream.
class player_factory
{
public:
    typedef std::shared_ptr<media::player> player_ptr;

    /// Factory for registering a media source class.
    typedef reg_factory<media::source> source_factory;
    /// Factory for registering a media filter class.
    typedef reg_factory<media::filter> filter_factory;

    /// Create a new player for a given stream or return an already created.
    player_ptr create(const std::string& name,
                      const std::string& source_class,
                      const std::string& filter_class = "",
                      const std::string& filter_options = "")
    {}
};

} // end namespace media
} // end namespace snode


#endif /* PLAYER_H_ */

