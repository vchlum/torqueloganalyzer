// INTERNAL
#include "Data.h"
#include "EventParser.h"
#include "Options.h"

// STD C++
#include <future>
using namespace std;

// BOOST
#include <boost/spirit/include/qi.hpp>
namespace qi = boost::spirit::qi;

vector<future<Data>> futures;

static size_t curr_thread = 0;

vector<Data> data;
set<string> users;

void callback(const char * const begin, const char * const end)
{
        packaged_task<Data()> task([begin,end]() {
                const string input(begin,static_cast<size_t>(end-begin));
                using It = string::const_iterator;
                It f(input.begin()), l(input.end());
                Data parsed;

                bool ok = qi::phrase_parse(f,l,grammar<It>(),qi::space,parsed);

                if (ok) {
                } else {
                    cout << "Parsing failed\n";
                }

                return parsed;
        });

        if (futures.size() < options::maximum_threads)
        {
                futures.push_back(task.get_future());
                thread(move(task)).detach();
        }
        else
        {
		data.push_back(futures[curr_thread % options::maximum_threads].get());
		if (data[data.size()-1].fields.owner)
			users.insert(*(data[data.size()-1].fields.owner));

                futures[curr_thread % options::maximum_threads] = task.get_future();
                thread(move(task)).detach();
        }

        curr_thread++;
}

void finish_processing()
{
        size_t tail = curr_thread;

        do
        {
		data.push_back(futures[tail % options::maximum_threads].get());
		if (data[data.size()-1].fields.owner)
			users.insert(*(data[data.size()-1].fields.owner));

                tail++;
        }
        while (tail % options::maximum_threads != curr_thread % options::maximum_threads);

        futures.clear();
        curr_thread = 0;
}
