// dbase.h
// Copyright (C) 2016  Emil Penchev, Bulgaria

#ifndef DBASE_H_
#define DBASE_H_

namespace snode
{
namespace media
{

/// Stores all data associated with media streams and files (ex. location, metadata ..).
class dbase
{
public:

    /// Add media location to be indexed
    void add_location(const std::string& location)
    {}

    static media::dbase& instance()
    {
        static media::dbase s_media_db;
        return s_media_db;
    }

private:
    dbase();
    ~dbase();
};

} // end namespace media
} // end namespace snode

#endif /* DBASE_H_ */
