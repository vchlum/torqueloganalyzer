// INTERNAL
#include "Data.h"
#include "EventParser.h"
#include "Options.h"

// STD C++
#include <future>

// BOOST
#include <boost/spirit/include/qi.hpp>
namespace qi = boost::spirit::qi;

std::vector<std::future<Data>> futures;

static size_t curr_thread = 0;

std::vector<Data> data;
std::set<std::string> users;
std::map<std::string, FullJob> full_data;
std::set<std::string> job_blacklist;
std::map<std::string, std::vector<Data*> > data_by_user;

void callback(const char * const begin, const char * const end)
{
	std::packaged_task<Data()> task([begin,end]() {
		const std::string input(begin,static_cast<size_t>(end-begin));
		using It = std::string::const_iterator;
		It f(input.begin()), l(input.end());
		Data parsed;

		bool ok = qi::phrase_parse(f,l,grammar<It>(),qi::space,parsed);
		if (!ok) {
		    std::cout << "Parsing failed" << std::endl;
		}

		return parsed;
	});

	if (futures.size() < options::maximum_threads)
	{
		futures.push_back(task.get_future());
		std::thread(std::move(task)).detach();
	}
	else
	{
		data.push_back(futures[curr_thread % options::maximum_threads].get());
		if (data[data.size()-1].fields.owner)
			users.insert(*(data[data.size()-1].fields.owner));

		futures[curr_thread % options::maximum_threads] = task.get_future();
		std::thread(std::move(task)).detach();
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

void job_preprocess()
{
    for (size_t i = 0; i < data.size(); ++i)
    {
	if (data[i].fields.time_start && *(data[i].fields.time_start) == 0)
	    continue;

	if (data[i].fields.time_compl && *(data[i].fields.time_compl) == 0)
	    continue;

	if (data[i].fields.time_arriv && *(data[i].fields.time_arriv) == 0)
	    continue;

	if (options::queue_filter.size() != 0)
	    if (data[i].fields.queue && (*data[i].fields.queue).substr(0,options::queue_filter.size()) != options::queue_filter)
		continue;

	if (options::user_filter.size() != 0)
	    if (data[i].fields.owner && (*data[i].fields.owner).substr(0,options::user_filter.size()) == options::user_filter)
		continue;

	if (data[i].event == 'S')
	    job_blacklist.insert(data[i].id);

	if (data[i].event == 'E' || data[i].event == 'A' || data[i].event == 'D')
	    job_blacklist.erase(data[i].id);
    }
}

bool validate_job(std::string jobid)
{
	// Job must have a creation, start and completion event
	if (full_data[jobid].event_arrive == NULL)
		return false;
	if (full_data[jobid].event_start == NULL)
		return false;
	if (full_data[jobid].event_compl == NULL)
		return false;

	Data* fulldata = full_data[jobid].event_compl;

	// time information
	if (!fulldata->fields.time_arriv)
		return false;
	if (!fulldata->fields.time_start)
		return false;
	if (!fulldata->fields.time_eligb)
		return false;
	if (!fulldata->fields.time_compl)
		return false;

	// memory and CPU resource requirements
	if (!fulldata->fields.resc_total_mem)
		return false;
	if (!fulldata->fields.resc_total_cores)
		return false;

	// Must have a queue and an owner
	if (!fulldata->fields.queue)
		return false;
	if (!fulldata->fields.owner)
		return false;

	// Validate time sequences

	// ignore jobs before year 2000, probably a errornous time information
	if (fulldata->fields.time_arriv.get() <= 946684800)
		return false;
	if (fulldata->fields.time_start.get() <= 946684800)
		return false;
	if (fulldata->fields.time_eligb.get() <= 946684800)
		return false;
	if (fulldata->fields.time_compl.get() <= 946684800)
		return false;

	// arrival time must be first
	if (fulldata->fields.time_arriv.get() > fulldata->fields.time_eligb.get())
		return false;
	if (fulldata->fields.time_arriv.get() > fulldata->fields.time_start.get())
		return false;
	if (fulldata->fields.time_arriv.get() > fulldata->fields.time_compl.get())
		return false;

	// followed by eligible time
	if (fulldata->fields.time_eligb.get() > fulldata->fields.time_start.get())
		return false;
	if (fulldata->fields.time_eligb.get() > fulldata->fields.time_compl.get())
		return false;

	// followed by start
	if (fulldata->fields.time_start.get() > fulldata->fields.time_compl.get())
		return false;

	return true;
}

bool compare_job_arrival(Data* left, Data *right)
{
	return left->fields.time_arriv.get() < right->fields.time_arriv.get();
}

void construct_fulljob_information()
{
	job_preprocess();

	for (size_t i = 0; i < data.size(); ++i)
	{
		if (job_blacklist.find(data[i].id) != job_blacklist.end())
			continue;

		std::map<std::string, FullJob>::iterator j = full_data.find(data[i].id);
		if (j == full_data.end())
		{
			full_data.insert(make_pair(data[i].id,FullJob()));
			j = full_data.find(data[i].id);
		}

		if (data[i].event == 'Q')
		{
			j->second.event_arrive = &data[i];
		}

		if (data[i].event == 'S')
		{
			j->second.event_start = &data[i];
		}

		if (data[i].event == 'E')
		{
			j->second.event_compl = &data[i];
			std::pair<std::map< std::string,std::vector<Data*> >::iterator,bool> it;
			it = data_by_user.insert(std::make_pair(data[i].fields.owner.get(),std::vector<Data*>()));
			if (options::validate_job_data)
			{
				if (validate_job(data[i].id))
					it.first->second.push_back(&data[i]);
			}
			else
			{
				it.first->second.push_back(&data[i]);
			}
		}
	}

	for (auto& i : data_by_user)
	{
		sort(i.second.begin(),i.second.end(),compare_job_arrival);
	}
}

