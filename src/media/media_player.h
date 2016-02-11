// media_player.h
// Copyright (C) 2016  Emil Penchev, Bulgaria


#ifndef MEDIA_PLAYER_H_
#define MEDIA_PLAYER_H_

#include <string>
#include <memory>
#include <map>
#include <vector>
#include "media_source.h"
#include "media_filter.h"
#include "reg_factory.h"
#include "sourcebuf.h"

namespace snode
{
namespace media
{

/// Common player interface for all sorts of streams.
class media_player
{
public:
    typedef std::shared_ptr<media_source> source_ptr;
    typedef std::shared_ptr<media_filter> filter_ptr;
    typedef media_source::stream_type stream_type;
    typedef media_source::live_stream_type live_stream_type;
    typedef media_source::off_type off_type;

    media_player(const std::string& name, source_ptr sr, filter_ptr fl = nullptr)
      : source_(sr), filter_(fl), databuf_(1024)
    {}

    virtual ~media_player()
    {}

    /// Start playing the given stream
    void play();

    /// Put the stream on pause, the stream can be resumed with the play() method.
    /// When resumed stream will start from the current position (not valid for live streams).
    void pause();

    /// Stop playing the stream, the stream can be resumed with the play() method.
    /// When resumed stream will start from beginning (not valid for live streams).
    void stop();

    /// Start recording the stream to a given a location while it plays (DVR).
    void record(const std::string& filepath);

    /// Seek to a specific time (in seconds) in the stream.
    /// Only valid if this is multimedia stream and is not live.
    void seek(float ts);

    /// Get the name of the stream.
    const std::string& name() const;

    /// Get player's stream
    stream_type& stream() { return stream_; }

    /// Get player's livestream
    live_stream_type& live_stream() { return live_stream_; }

private:
    template<typename media_player> friend class streams::sourcebuf;

    /// Internal program interface to be used only from sourcebuf
    /// media_player is complying with SourceImpl interface
    size_t read(media_source::char_type* ptr, size_t count, off_type offset = -1);
    size_t size() const;
    void close() {}

    void read_handler(size_t count);
    void read_handler_live(size_t count);

    std::string name_;
    stream_type stream_;
    live_stream_type live_stream_;
    source_ptr source_;
    filter_ptr filter_;
    std::vector<media_source::char_type> databuf_;
};

/// Stores all active players and creates new ones for a given media stream.
class player_factory
{
public:
    typedef std::shared_ptr<media_player> player_ptr;

    /// Factory for registering a media source class.
    typedef reg_factory<media_source> source_factory;
    /// Factory for registering a media filter class.
    typedef reg_factory<media_filter> filter_factory;

    /// Factory method, create a new player for a given stream or return an already created.
    /// If player can't be created nullptr is returned
    player_ptr create(const std::string& name, const std::string& source_class,
                      const std::string& filter_class = "", const std::string& filter_options = "");

private:
    /// stores the stream name -> player relationship
    std::map<std::string, player_ptr> players_;

};

} // end namespace media
} // end namespace snode


#endif /* MEDIA_PLAYER_H_ */

