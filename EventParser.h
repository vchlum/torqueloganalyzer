#ifndef EVENTPARSER_H
#define EVENTPARSER_H

// INTERNAL
#include "Data.h"

// BOOST
//#define BOOST_SPiRiT_DEBUG
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/posix_time/posix_time_io.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix.hpp>

namespace qi = boost::spirit::qi;
namespace phx = boost::phoenix;

template <typename it, typename Skipper = qi::space_type>
struct grammar : qi::grammar<it, JobData(), Skipper> {
    grammar() : grammar::base_type(start), other(), event(), text(), id(), timestamp(), fields(), start() {
	using namespace qi;
	timestamp = lexeme [ +(~qi::char_(' ')) >> qi::char_(' ') >> +(~qi::char_(';')) ];

	real_parser<double, strict_real_policies<double> > real_;

	text   = lexeme [
		    '"' >> *('\\' >> char_ | ~char_('"')) >> '"'
		  | "'" >> *('\\' >> char_ | ~char_("'")) >> "'"
		  | *graph
	       ];

	id     = lexeme [ *~char_(';') ];
	event  = lexeme [ char_("QSEDARC") ];

    auto time_arriv = bind(&JobData::Fields::time_arriv, _val);
    auto time_start = bind(&JobData::Fields::time_start, _val);
    auto time_compl = bind(&JobData::Fields::time_compl, _val);
    auto time_eligb = bind(&JobData::Fields::time_eligb, _val);
    auto queue = bind(&JobData::Fields::queue, _val);
    auto owner = bind(&JobData::Fields::owner, _val);
    auto resc_total_cores = bind(&JobData::Fields::resc_total_cores, _val);
    auto resc_total_mem = bind(&JobData::Fields::resc_total_mem, _val);
    auto req_walltime = bind(&JobData::Fields::req_walltime, _val);
    auto exec_host = bind(&JobData::Fields::exec_host, _val);
    auto nodespec = bind(&JobData::Fields::nodespec, _val);

	//other  = lexeme [ +(graph-'=') ] >> '=' >> (real_|int_|text);
    other  = lexeme [ +(graph-'=') ] >> '=' >> text;

	fields = *(
		    ("qtime" >> lit('=') >> int_) [ time_arriv = _1 ]
		  | ("start" >> lit('=') >> int_) [ time_start = _1 ]
		  | ("end"   >> lit('=') >> int_) [ time_compl = _1 ]
		  | ("etime" >> lit('=') >> int_) [ time_eligb = _1 ]
		  | ("queue" >> lit('=') >> text) [ queue = _1 ]
		  | ("owner" >> lit('=') >> text) [ owner = _1 ]
		  | ("resc_req_total.procs" >> lit('=') >> int_) [ resc_total_cores = _1 ]
		  | ("resc_req_total.mem" >> lit('=') >> text) [ resc_total_mem = _1 ]
		  | ("resc_req_total.walltime" >> lit('=') >> text) [ req_walltime = _1 ]
		  | ("exec_host" >> lit('=') >> text) [ exec_host = _1 ]
		  | ("Resource_List.processed_nodes" >> lit('=') >> text) [ nodespec = _1 ]
		  | ("user" >> lit('=') >> text) [ owner = _1 ]
		  | ("Resource_List.ncpus" >> lit('=') >> int_) [ resc_total_cores = _1 ]
		  | ("Resource_List.mem" >> lit('=') >> text) [ resc_total_mem = _1 ]
		  | ("Resource_List.walltime" >> lit('=') >> text) [ req_walltime = _1 ]
		  | ("Resource_List.select" >> lit('=') >> text) [ nodespec = _1 ]
		  | other
		  );

	start  = timestamp >> ';' >> event >> ';' >> id >> -(';' >> fields);

	BOOST_SPIRIT_DEBUG_NODES((timestamp)(id)(start)(text)(other)(fields))
    }

  virtual ~grammar() {}
  private:
    qi::rule<it,                                 Skipper> other;
    qi::rule<it, char(),                         Skipper> event;
    qi::rule<it, std::string(),                  Skipper> text, id;
    qi::rule<it, std::string(),                  Skipper> timestamp;
    qi::rule<it, JobData::Fields(),                 Skipper> fields;
    qi::rule<it, JobData(),                         Skipper> start;
};


#endif // EVENTPARSER_H
