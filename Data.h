#ifndef DATA_H
#define DATA_H

// STD C++
#include <vector>
#include <string>
#include <set>

// BOOST
#include <boost/optional/optional_io.hpp>
#include <boost/fusion/adapted/struct.hpp>

struct Data
{
    std::string timestamp;
    char event;
    std::string id;
    struct Fields {
        boost::optional<int64_t> time_arriv;
        boost::optional<int64_t> time_start;
        boost::optional<int64_t> time_compl;
        boost::optional<int64_t> time_eligb;
        boost::optional<std::string> queue;
        boost::optional<std::string> owner;
        boost::optional<int> resc_total_cores;
        boost::optional<std::string> resc_total_mem;

        Fields() : time_arriv(), time_start(), time_compl(), time_eligb(), queue() {}
    } fields;

    Data() : timestamp(), event(), id(), fields() {}
};

BOOST_FUSION_ADAPT_STRUCT(Data::Fields,
        (boost::optional<int64_t>, time_arriv)
        (boost::optional<int64_t>, time_start)
        (boost::optional<int64_t>, time_compl)
        (boost::optional<int64_t>, time_eligb)
        (boost::optional<std::string>, queue)
        (boost::optional<std::string>, owner)
        (boost::optional<int>, resc_total_cores)
        (boost::optional<std::string>, resc_total_mem)
    )

BOOST_FUSION_ADAPT_STRUCT(Data,
        (std::string, timestamp)
        (char, event)
        (std::string, id)
        (Data::Fields, fields)
    )

extern std::vector<Data> data;
extern std::set<std::string> users;

void callback(const char * const begin, const char * const end);
void finish_processing();

#endif // DATA_H
