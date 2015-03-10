//
// xml_reader.cpp
// Copyright (C) 2014  Emil Penchev, Bulgaria

#ifndef XML_READER_H_
#define XML_READER_H_

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/lexical_cast.hpp>
#include <string>
#include <exception>

namespace smkit
{

///  General error class used for throwing exceptions from xml_reader.
class xml_reader_error : public std::runtime_error
{
public:
	///  Construct error
    xml_reader_error(const std::string& what_arg) : runtime_error(what_arg)
    {
    }

	~xml_reader_error() throw()
    {
	}
};

///  XML parser. Implementation is just a wrapper over boost property tree.
class xml_reader
{
public:
    xml_reader() 
    {
    }

    virtual ~xml_reader()
    {
    }
    
    ///  Open XML file. Will throw xml_reader_error exception on error.
    void open(const std::string& filepath)
    {
    	try
    	{
    		read_xml(filepath, m_ptree);
    	} catch (std::exception &ex) {
    		throw xml_reader_error(ex.what());
    	}
    }

    ///  XML tag iterator
    class iterator
    {
    public:
        iterator()
        {
        }

        iterator(xml_reader& reader)
        {
        	try
        	{
        	    m_ptree_iter = reader.m_ptree.begin();
        	} catch (std::exception& ex) {
                throw xml_reader_error(ex.what());
        	}
        }

        iterator(boost::property_tree::ptree::iterator it)
        {
        	m_ptree_iter = it;
        }

        iterator& operator =(boost::property_tree::ptree::iterator it)
        {
            m_ptree_iter = it;
            return *this;
        }

        ///  Name of XML tag/object referenced by iterator.
        const std::string& get_name()
        {
            try
            {
                return m_ptree_iter->first;
            } catch (std::exception& ex) {
                throw xml_reader_error(ex.what());
            }
        }

        ///  Get value by a key, throws exception on error.
        template <class T>
        T get_value(const std::string& key = "")
        {
        	try
        	{
        	    if (key.empty())
        	    	return boost::lexical_cast<T>(m_ptree_iter->second.data());
        	    else
        	    	return m_ptree_iter->second.get<T>(key);
        	} catch (std::exception& ex) {
        		throw xml_reader_error(ex.what());
        	}
        }

        ///  Return default value if path is not found
        template <class T>
        T get_value(const std::string& key, T defaultVal)
        {
        	try
        	{
        		return m_ptree_iter->second.get<T>(key, defaultVal);
        	} catch (std::exception& ex) {
        		throw xml_reader_error(ex.what());
        	}
        }
        ///  Return iterator to a given tag subsection.
        iterator begin(const std::string& key)
        {
        	try
        	{
        		iterator it = m_ptree_iter->second.get_child(key).begin();
        		return it;
        	} catch (std::exception& ex) {
        		throw xml_reader_error(ex.what());
        	}
        }

        ///  Return iterator to the end of given tag subsection.
        iterator end(const std::string& key)
        {
        	try
            {
        		iterator it = m_ptree_iter->second.get_child(key).end();
        		return it;
            } catch (std::exception& ex) {
            	throw xml_reader_error(ex.what());
            }
        }

        ///  Find if a given tag subsection exists.
        bool find(const std::string& key)
        {
        	boost::property_tree::ptree::const_assoc_iterator it = m_ptree_iter->second.find(key);
            if (it != m_ptree_iter->second.not_found())
            	return true;
            else
            	return false;
        }

        iterator& operator ++()
        {
            m_ptree_iter++;
            return *this;
        }

        iterator operator ++(int unused)
        {
            iterator res = *this;
            m_ptree_iter++;
            return res;
        }

        friend bool operator ==(const xml_reader::iterator& ita, const xml_reader::iterator& itb)
        {
            return ita.m_ptree_iter == itb.m_ptree_iter;
        }

        friend bool operator !=(const xml_reader::iterator& ita, const xml_reader::iterator& itb)
        {
            return ita.m_ptree_iter != itb.m_ptree_iter;
        }

       private:
           boost::property_tree::ptree::iterator m_ptree_iter;
    };

    ///  throws exception if path is not found
    xml_reader::iterator begin(const std::string& path = "")
    {
        xml_reader::iterator it;
        try
        {
        	if (path.empty())
        		return it = m_ptree.begin();
        	else
        		return it = m_ptree.get_child(path).begin();
        } catch (std::exception& ex) {
        	throw xml_reader_error(ex.what());
        }
    }

    ///  throws exception if path is not found
    xml_reader::iterator end(const std::string& path = "")
    {
        xml_reader::iterator it;
        try
        {
        	if (path.empty())
        		return it = m_ptree.end();
        	else
        		return it = m_ptree.get_child(path).end();
        } catch (std::exception& ex) {
        	throw xml_reader_error(ex.what());
        }
    }

    ///  Get value specified by a XML tag path, throws exception if path is not found.
    template <class T>
    T get_value(const std::string& path)
    {
        try
        {
        	return m_ptree.get<T>(path);
        } catch (std::exception& ex) {
        	throw xml_reader_error(ex.what());
        }
    }

    ///  return default value if path is not found
    template <class T>
    T get_value(const std::string& path, T defaultVal)
    {
        return m_ptree.get(path, defaultVal);
    }

    ///  Find if a given tag subsection exists.
    bool find(const std::string& path)
    {
    	boost::property_tree::ptree::const_assoc_iterator it = m_ptree.find(path);
    	if (it != m_ptree.not_found())
    		return true;
    	else
    		return false;
    }

private:
    friend class xml_reader::iterator;
    boost::property_tree::ptree m_ptree;
};
}

#if 0 /* test only */
#include <iostream>

int main()
{
    smkit::xml_reader reader;
    reader.open("skit_conf.xml");
    int threads = reader.get_value<int>("threads");
    threads = reader.get_value("threads", 0);
    bool run_daemon = reader.get_value<bool>("daemon");

    std::cout << threads << std::endl;
    std::cout << run_daemon << std::endl;

    smkit::xml_reader::iterator it = reader.begin("servers");
    //smkit::xml_reader::iterator it_end = reader.end("servers");
    while (it != reader.end("servers"))
    {
        std::string server_name = it.get_value<std::string>("name");
        unsigned server_port = it.get_value<unsigned>("listen");
        if (it.find("options"))
        {
            std::cout << "found options \n";
            smkit::xml_reader::iterator it_opt = it.begin("options");
            while (it_opt != it.end("options"))
            {
                std::cout << it_opt.get_name() << " " << it_opt.get_value<std::string>() << std::endl;
                ++it_opt;
            }
        }

        std::cout << server_name << std::endl;
        std::cout << server_port << std::endl;
        ++it;
    }

    it = reader.begin("streams");
    while (it != reader.end("streams"))
    {
        std::string name = it.get_value<std::string>("name");
        std::string location = it.get_value<std::string>("location");
        std::cout << name << std::endl;
        std::cout << location << std::endl;
        ++it;
    }

    return 0;
}
#endif

#endif // XML_READER_H_
