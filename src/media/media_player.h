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
    typedef media_source::off_type off_type;
    typedef media_source::char_type char_type;
    typedef streams::async_streambuf<char_type, streams::producer_consumer_buffer<char_type> > streambuf_type;
    typedef streambuf_type::istream_type stream_type;
    static const size_t buf_size = 16*1024;

    media_player(const std::string& name, source_ptr source, filter_ptr filter = nullptr);

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
    /// Only valid if this is multi-media stream and is not live.
    void seek(float ts);

    /// Seek to a specific location (in bytes) in to the stream.
    /// Only valid if stream is not live.
    void seek(off_type offset);

    /// Get the name of the stream.
    const std::string& name() const;

    /// Get player's stream
    stream_type stream();

private:
    template<typename media_player> friend class streams::sourcebuf;
    typedef media_source::char_type char_type;
    typedef streams::async_streambuf<char_type, streams::producer_consumer_buffer<char_type> > streambuf_type;

    /// Internal program interface to be used only from sourcebuf
    /// media_player is complying with SourceImpl interface
    size_t read(media_source::char_type* ptr, size_t count, off_type offset = -1);
    size_t size();
    void close() {}

    void read_handler(size_t count);
    void read_handler_live(size_t count);

    streambuf_type streambuf_;                   // player's internal stream buffer
    source_ptr source_;                          // Source object.
    filter_ptr filter_;                          // Filter object to process source data if specified.
    std::string name_;                           // Name of the stream we are playing.
    media_source::stream_type stream_;           // async_istream instance to read data from the source.
    media_source::livestream_type streamlive_;   // async_istream instance to read live data from the source.
};

/// Stores all active players and creates new ones for a given media stream.
class player_factory
{
public:
    typedef std::shared_ptr<media_player> player_ptr;

    /// Factory for registering a media source.
    typedef reg_factory<media_source> source_factory;

    /// Factory for registering a media filter.
    typedef reg_factory<media_filter> filter_factory;

    /// Factory method, create a new player for a given stream.
    /// If player can't be created NULL is returned instead.
    player_ptr create(const std::string& name, const std::string& sourcetype,
                      const std::string& filtertype = "", const std::string& filter_opt = "");

private:
    /// stores the stream name -> player relationship
    std::map<std::string, player_ptr> players_;

};

} // end namespace media
} // end namespace snode


#endif /* MEDIA_PLAYER_H_ */

