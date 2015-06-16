// INTERNAL
#include "FastLineReader.h"
#include "Data.h"
#include "Options.h"
#include "SimpleStats.h"
#include "Utils.h"

// POSIX
#include <time.h>

// STD C++
#include <iostream>
#include <sstream>
#include <string>
#include <limits>
#include <vector>
#include <fstream>
#include <cstdio>
#include <iomanip>
#include <map>
#include <set>
using namespace std;

namespace options
{
    const bool print_per_user_info = true;
    const bool print_per_queue_info = true;
    const char event_type = 'S';
    const bool utilization_only = false;
}

bool order_times(const pair<int64_t,string>& lhs, const pair<int64_t,string>& rhs);
bool order_times(const pair<int64_t,string>& lhs, const pair<int64_t,string>& rhs)
{
    if (lhs.first < rhs.first)
	return true;

    if (lhs.first == rhs.first)
	return lhs.second < rhs.second;

    return false;
}

//vector<FullJob> raw_user_jobs;

//map<string, vector<FullJob> > raw_user_jobs;

/*
void single_user_stats(vector<Data>& job_data);
void single_user_stats(vector<Data>& job_data)
{
    int64_t weekly_counts[24][7] = {0};

    map<int64_t, int64_t> size_distr;
    vector<int64_t> runtimes;
    int64_t runtime = 0;

    for (size_t i = 0; i < job_data.size(); i++)
    {
	if (job_data[i].event == 'Q')
	{
	    auto j = fulljobs.find(job_data[i].id);

	    if (j != fulljobs.end() && j->second.event_start != NULL && j->second.event_start->fields.owner)
	    {
		Data* ev = j->second.event_start;
		if (ev->fields.owner->substr(0,options::single_user.length()) == options::single_user)
		{
		    struct tm tm1;
		    if (strptime(data[i].timestamp.c_str(), "%m/%d/%Y %H:%M:%S", &tm1) != NULL)
		    {
			time_t t = mktime(&tm1);
			weekly_counts[tm1.tm_hour][localtime(&t)->tm_wday]++;
		    }

		    size_distr[*(ev->fields.resc_total_cores)] += 1;

		    Data* ev_compl = j->second.event_compl;
		    if (ev_compl != NULL)
		    {
			runtimes.push_back((*(ev_compl->fields.time_compl) - *(ev_compl->fields.time_start) + 59)/60);
		    }
		}
	    }
	}
    }

    cout << "Daily submits " << endl;
    for (size_t i = 0; i < 7; i++)
    {
	int64_t sum = 0;
	for (size_t j = 0; j < 24; j++)
	    sum += weekly_counts[j][i];
	cout << "Day \"" << i+1 << " " << sum << endl;
    }

    cout << "Hourly submits " << endl;
    for (size_t i = 0; i < 24; i++)
    {
	int64_t sum = 0;
	for (size_t j = 0; j < 7; j++)
	    sum += weekly_counts[i][j];
	cout << "Hour \"" << i << " " << sum << endl;
    }

    for (auto i : size_distr)
	cout << i.second << " jobs with " << i.first << " processors." << endl;

    runtime = 0;
    for (auto i : runtimes)
	runtime += i;

    double avg_runtime = double(runtime)/runtimes.size();
    double variance = 0;

    for (auto i : runtimes)
    {
	variance += (i - avg_runtime)*(i - avg_runtime);
    }

    variance /= runtimes.size();

    cout << "Average runtime " << fixed << avg_runtime << " minutes" << endl;
    cout << "Runtime variance " << fixed << sqrt(variance) << endl;

}

*/

struct Batch
{
	int64_t first_arrival;
	int64_t last_completion;
	vector<Data*> job_info;
	pair<unsigned, unsigned> global_id;

	Batch(Data *first_job, unsigned sessionID, unsigned batchID)
	{
		first_arrival = *(first_job->fields.time_arriv);
		last_completion = *(first_job->fields.time_compl);
		global_id.first = sessionID;
		global_id.second = batchID;
		job_info.push_back(first_job);
	}

	void add_job(Data *job)
	{
		if (first_arrival > *(job->fields.time_arriv))
			cerr << "Bad ordering between jobs. Earlier arrival processed after later arrival. (" << *(job->fields.time_arriv) << ") diff (" << (first_arrival - *(job->fields.time_arriv)) << ")" << endl;

		if (last_completion < *(job->fields.time_compl))
			last_completion = *(job->fields.time_compl);

		job_info.push_back(job);
	}

	vector< pair<unsigned,unsigned> > after_arrival;
	vector< pair<unsigned,unsigned> > after_completion;

	void add_completion_dep(unsigned session, unsigned batch)
	{
		after_completion.push_back(make_pair(session,batch));
	}

	void add_completion_dep(pair<unsigned,unsigned> globalID)
	{
		after_completion.push_back(globalID);
	}

	void add_arrival_dep(pair<unsigned,unsigned> globalID)
	{
		after_arrival.push_back(globalID);
	}

	void add_arrival_dep(unsigned session, unsigned batch)
	{
		after_arrival.push_back(make_pair(session,batch));
	}

	string get_batch_label()
	{
		stringstream s;
		s << "Batch_" << global_id.first << "_" << global_id.second;
		return s.str();
	}
};

struct Session
{
	int64_t first_arrival;
	int64_t last_arrival;
	int64_t last_completion;
	vector<Batch> batches;

	unsigned session_id;
	unsigned last_batch_id;

	Session(Data* first_job, unsigned sessionID)
	{
		first_arrival = first_job->fields.time_arriv.get();
		last_arrival = first_job->fields.time_arriv.get();
		last_completion = first_job->fields.time_compl.get();
		last_batch_id = 0;
		batches.push_back(Batch(first_job,sessionID,last_batch_id));
		session_id = sessionID;
	}

	unsigned add_job(Data* job)
	{
		if (first_arrival > job->fields.time_arriv.get())
			cerr << "Bad ordering between jobs. Earlier arriva processed after later arrival. (" << *(job->fields.time_arriv) << ") diff (" << (first_arrival - *(job->fields.time_arriv)) << ") \"" << job->id << "\"" << endl;

		last_arrival = job->fields.time_arriv.get();
		if (last_completion < job->fields.time_compl.get())
			last_completion = job->fields.time_compl.get();

		if (batches[batches.size()-1].last_completion < *(job->fields.time_arriv) || batches[batches.size()-1].first_arrival > *(job->fields.time_compl))
			batches.push_back(Batch(job,session_id,++last_batch_id));
		else
			batches[batches.size()-1].add_job(job);

		return last_batch_id;
	}

	void setup_internal_dependencies()
	{
		for (size_t i = 0; i < batches.size(); i++)
		{
			if (i > 0)
				batches[i].add_completion_dep(session_id,batches[i-1].global_id.second);
		}
	}

	void add_completion_dep(unsigned session, unsigned batch)
	{
		batches[0].add_completion_dep(session,batch);
	}

	void add_completion_dep(pair<unsigned,unsigned> globalID)
	{
		batches[0].add_completion_dep(globalID);
	}

	void add_arrival_dep(pair<unsigned,unsigned> globalID)
	{
		batches[0].add_arrival_dep(globalID);
	}

	void add_arrival_dep(unsigned session, unsigned batch)
	{
		batches[0].add_arrival_dep(session,batch);
	}

	string get_session_label()
	{
		stringstream s;
		s << "Session_" << session_id;
		return s.str();
	}
};

map<string, vector<Session> > user_sessions;
bool compare_job_arrival(Data* left, Data *right);

int main(int argc, char *argv[])
{
	// parse program options
	int err = -1;
	if ((err = parse_options(argc,argv)) != 0)
		return err;

	// make sure that we have at least one input file
	if (options::inputs.size() == 0)
	{
		cerr << "No input files specified." << endl;
		return 1;
	}

	// process specified account logs
	for (auto i : options::inputs)
	{
		if (fastLineParser(i.c_str(),callback) != 0)
		{
			cerr << "Couldn't process file " << i << endl;
			return 1;
		}
		finish_processing();
	}

	cout << "Finished reading input data." << endl;

	// generate an overview of collected data
	if (options::simple_stats)
	{
		if ((err = generate_simple_stats()) != 0)
			return err;

		cout << "Total number of" << endl;
		if (global_stats.event_queued > 0)
			cout << "\tarrived jobs : " << global_stats.event_queued << endl;
		if (global_stats.event_started > 0)
			cout << "\tstarted jobs : " << global_stats.event_started << endl;
		if (global_stats.event_completed > 0)
			cout << "\tcompleted jobs : " << global_stats.event_completed << endl;
		if (global_stats.event_delete > 0)
			cout << "\tuser deleted jobs : " << global_stats.event_delete << endl;
		if (global_stats.event_abort > 0)
			cout << "\taborted jobs : " << global_stats.event_abort << endl;
		if (global_stats.event_checkpoint > 0)
			cout << "\tcheckpoints : " << global_stats.event_checkpoint << endl;
		if (global_stats.event_restart_from_checkpoint > 0)
			cout << "\tjobs restarted from checkpoints :" << global_stats.event_restart_from_checkpoint << endl;
		if (global_stats.event_reruns > 0)
			cout << "\tjob reruns : " << global_stats.event_reruns << endl;

		return 0;
	}

	cout << "Processing and filtering job information." << endl;
	construct_fulljob_information();
	cout << "Finished processing job information." << endl;

	// detect sessions in the workload
	if (options::detect_sessions)
	{
		fstream stats;
		stats.open("session_stats.dat",ios_base::out | ios_base::trunc);

		// go over the pre-processed user data, user-by-user
		for (auto& i : data_by_user)
		{
			string username = i.first.substr(0,i.first.find('@'));
			unsigned last_session_id = 0;
			unsigned last_batch_id = 0;

			user_sessions.insert(make_pair(i.first,vector<Session>()));

			int64_t last_arrival = 0;
			int64_t this_arrival = 0;

			vector<Session>& user_session = user_sessions[i.first];

			for (auto& j : i.second)
			{
				this_arrival = *(j->fields.time_arriv);
				if (last_arrival == 0 || this_arrival - last_arrival > HOUR)
				{ // create a new session
					user_session.push_back(Session(j,last_session_id));
					if (last_session_id > 0)
					{
						user_session[user_session.size()-1].add_arrival_dep(last_session_id-1,last_batch_id);
						for (size_t k = 0; k < user_session.size(); k++)
						{
							size_t last_batch = user_session[k].batches.size()-1;
							if (user_session[k].batches.size() == 0)
								cerr << "Error 0 batches detected" << endl;
							if (user_session[k].batches[last_batch].last_completion < this_arrival)
								user_session[user_session.size()-1].add_completion_dep(user_session[k].batches[last_batch].global_id);
						}
					}
					++last_session_id;
					last_batch_id = 0;
				}
				else if (this_arrival - last_arrival <= HOUR)
				{ // same session
					last_batch_id = user_session[user_session.size()-1].add_job(j);
				}
				last_arrival = this_arrival;
			}

			// detect batches inside sessions
			// setup dependencies between batches

			stringstream filename;
			filename << "user_" << username << ".dot";

			fstream f;
			f.open(filename.str(),ios_base::out | ios_base::trunc);

			int64_t total_jobs = 0;
			int64_t total_batches = 0;
			int64_t total_sessions = 0;

			f << "digraph main { " << endl;
			for (size_t j = 0; j < user_session.size(); j++)
			{
				user_session[j].setup_internal_dependencies();

				f << "subgraph cluster_" << user_session[j].get_session_label() << " {" << endl;
				f << "color = black;" << endl;
				f << "label = \"" << user_session[j].get_session_label() << "\";" << endl;
				for (size_t k = 0; k < user_session[j].batches.size(); k++)
				{
					f << " \"" << user_session[j].batches[k].get_batch_label() << "\"";
					total_jobs += user_session[j].batches[k].job_info.size();
					total_batches++;
				}
				f << ";" << endl;
				f << "}" << endl;
				total_sessions++;
			}

			for (size_t j = 0; j < user_session.size(); j++)
			{
				for (size_t k = 0; k < user_session[j].batches.size(); k++)
				{
					for (size_t l = 0; l < user_session[j].batches[k].after_arrival.size(); l++)
					{
						unsigned ses = user_session[j].batches[k].after_arrival[l].first;
						unsigned bat = user_session[j].batches[k].after_arrival[l].second;
						stringstream s;
						s << "Batch_" << ses << "_" << bat;
						f << "\"" << s.str() << "\" -> \"" << user_session[j].batches[k].get_batch_label() << "\";" << endl;
					}
				}
			}

			f << "}" << endl;
			f.close();

			stats << i.first << " " << total_jobs << " " << total_batches << " " << total_sessions << " " << (double)total_jobs/total_batches << " " << (double)total_batches/total_sessions << " " << (double)total_jobs/total_sessions << endl;
		}

		if (options::write_workload != "")
		{
			fstream base_workload;
			base_workload.open(options::write_workload+".dyn",ios_base::out | ios_base::trunc);

			int user_id = 1;

			// store a description file
			for (auto& i : data_by_user)
			{
				string username = i.first.substr(0,i.first.find('@'));
				vector<Session>& user_session = user_sessions[i.first];
				base_workload << username << "\t" << "dynamic" << endl;

				fstream user_jobs;
				user_jobs.open(options::write_workload+".dyn_"+username,ios_base::out | ios_base::trunc);

				for (auto& j : i.second)
				{
					string jobid = j->id.substr(0,j->id.find('.'));
					int hours = 0;
					int minutes = 0;
					int seconds = 0;
					sscanf(j->fields.req_walltime.get().c_str(),"%d:%d:%d",&hours,&minutes,&seconds);

					string processed_exec_host = j->fields.exec_host.get();

					size_t start = processed_exec_host.find('/');
					while (start != processed_exec_host.npos)
					{
						size_t end = processed_exec_host.find('+');
						processed_exec_host.erase(start,end-start);
						processed_exec_host[start] = ',';
						start = processed_exec_host.find('/');
					}

					user_jobs << jobid << " " << j->fields.time_arriv.get() << " " << (j->fields.time_start.get()-j->fields.time_arriv.get()) << " " << (j->fields.time_compl.get()-j->fields.time_start.get()) <<
						  " " << j->fields.resc_total_cores.get() << " " << "1" << " " << "1024" << " " << j->fields.resc_total_cores.get() << " " << ((hours*60+minutes)*60+seconds) <<
						  " " << "419430400" << " " << "1" << " " << user_id << " " << "1" << " " << "1 -1 1 1 1" << " " << "{" << processed_exec_host << "}" << " " << j->fields.queue.get() << " " << j->fields.nodespec.get() << endl;
				}

				fstream sessions,batches;
				sessions.open(options::write_workload+".dyn_"+username+"_sessions",ios_base::out | ios_base::trunc);
				batches.open(options::write_workload+".dyn_"+username+"_batches",ios_base::out | ios_base::trunc);

				for (auto& j : user_session)
				{
					time_t session_start = j.first_arrival;
					struct tm ts;

					ts = *localtime(&session_start);

					sessions << j.session_id << "\t" << j.first_arrival << "\t" << j.last_arrival << "\t" << j.last_completion << "\t"
						 << ts.tm_wday << "\t" << ts.tm_hour << "\t" << ts.tm_min << endl;

					for (auto& k : j.batches)
					{
						batches << k.global_id.first << "\t" << k.global_id.second << "\t" << k.first_arrival << "\t" << k.last_completion;
						for (auto& l : k.after_arrival)
						{
							batches << "\tafter_arrival:" << l.first << "|" << l.second;
						}

						for (auto& l : k.after_completion)
						{
							batches << "\tafter_completion:" << l.first << "|" << l.second;
						}

						for (auto& l : ((Batch)k).job_info)
						{
							batches << "\tjob:" << l->id.substr(0,l->id.find('.'));
						}
						batches << endl;

					}
				}
				sessions.close();
				batches.close();

				++user_id;
				user_jobs.close();
			}
			// store each user jobs into a file in SWF format
			// store sessions
			// store batches

			base_workload.close();
		}


#if 0
		// iterate over users
		for (auto i : users)
		{
			unsigned last_session_id = 0;
			unsigned last_batch_id = 0;

			user_sessions.insert(make_pair(i,vector<Session>()));
			int64_t last_arrival = 0;
			int64_t this_arrival = 0;
			for (size_t j = 0; j < data.size(); ++j)
			{
				if (job_blacklist.find(data[j].id) != job_blacklist.end())
				    continue;

				if (data[j].event == 'Q')
				{
					if (full_data[data[j].id].event_start != NULL &&					// only started jobs
						full_data[data[j].id].event_compl != NULL &&				// only completed jobs
						full_data[data[j].id].event_start->fields.owner &&			// has user information
						(i == *(full_data[data[j].id].event_start->fields.owner)) &&		// this user
						full_data[data[j].id].event_start->fields.time_arriv &&
						full_data[data[j].id].event_compl->fields.time_compl &&
						full_data[data[j].id].event_compl->fields.time_start)			// has arrival
					{
						if (options::validate_job_data && (!validate_job(data[j].id)))
							continue;

						this_arrival = *(full_data[data[j].id].event_start->fields.time_arriv);
						if (last_arrival == 0 || this_arrival - last_arrival > HOUR)
						{ // create a new session
							user_sessions[i].push_back(Session(&full_data[data[j].id],last_session_id));
							if (last_session_id > 0)
							{
								user_sessions[i][user_sessions[i].size()-1].add_follows_dep(last_session_id-1,last_batch_id);
								for (size_t k = 0; k < user_sessions[i].size(); k++)
								{
									size_t last_batch = user_sessions[i][k].batches.size()-1;
									if (user_sessions[i][k].batches.size() == 0)
										cerr << "Error 0 batches detected" << endl;
									if (user_sessions[i][k].batches[last_batch].last_completion < this_arrival)
										user_sessions[i][user_sessions[i].size()-1].add_follows_dep(user_sessions[i][k].batches[last_batch].global_id);
								}
							}
							++last_session_id;
							last_batch_id = 0;
						}
						else if (this_arrival - last_arrival <= HOUR)
						{ // same session
							last_batch_id = user_sessions[i][user_sessions[i].size()-1].add_job(&full_data[data[j].id]);
						}
						last_arrival = this_arrival;
					}
				}
			}

			// detect batches inside sessions
			// setup dependencies between batches

			stringstream filename;
			filename << "user_" << i.substr(0,i.find('@')) << ".dot";

			fstream f;
			f.open(filename.str(),ios_base::out | ios_base::trunc);

			int64_t total_jobs = 0;
			int64_t total_batches = 0;
			int64_t total_sessions = 0;

			f << "digraph main { " << endl;
			for (size_t j = 0; j < user_sessions[i].size(); j++)
			{
				user_sessions[i][j].setup_internal_dependencies();

				f << "subgraph cluster_" << user_sessions[i][j].get_session_label() << " {" << endl;
				f << "color = black;" << endl;
				f << "label = \"" << user_sessions[i][j].get_session_label() << "\";" << endl;
				for (size_t k = 0; k < user_sessions[i][j].batches.size(); k++)
				{
					f << " \"" << user_sessions[i][j].batches[k].get_batch_label() << "\"";
					total_jobs += user_sessions[i][j].batches[k].job_info.size();
					total_batches++;
				}
				f << ";" << endl;
				f << "}" << endl;
				total_sessions++;
			}

			for (size_t j = 0; j < user_sessions[i].size(); j++)
			{
				for (size_t k = 0; k < user_sessions[i][j].batches.size(); k++)
				{
					for (size_t l = 0; l < user_sessions[i][j].batches[k].strict_after.size(); l++)
					{
						unsigned ses = user_sessions[i][j].batches[k].strict_after[l].first;
						unsigned bat = user_sessions[i][j].batches[k].strict_after[l].second;
						stringstream s;
						s << "Batch_" << ses << "_" << bat;
						f << "\"" << s.str() << "\" -> \"" << user_sessions[i][j].batches[k].get_batch_label() << "\";" << endl;
					}
				}
			}

			f << "}" << endl;
			f.close();

			stats << i << " " << total_jobs << " " << total_batches << " " << total_sessions << " " << (double)total_jobs/total_batches << " " << (double)total_batches/total_sessions << " " << (double)total_jobs/total_sessions << endl;
		}
#endif
		stats.close();
	}

	return 0;

#if 0


    if (options::single_user != "")
    {
	cout << "Single user mode \"" << options::single_user << "\"" << endl;
	single_user_stats(data);
//		return 0;
    }


    map<string,int64_t> last_user_submit;
    map<string,int64_t> last_user_completion;
    map<string,string> last_user_submit_jobid;
    map<string,string> last_user_completion_jobid;

    map<string,int64_t> batches;
    map<string,int64_t> chains;

    vector<string> batch_id;
    vector<string> chain_id;

    map<string,int64_t> user_job_count;


    for (size_t i = 0; i < data.size(); ++i)
    {
	if (data[i].event == 'S')
	{
	}

	if (data[i].event == 'Q')
	{
	    if (fulljobs[data[i].id].event_start != NULL)
	    {

		int64_t submit = *(fulljobs[data[i].id].event_start->fields.time_arriv);
		string owner = *(fulljobs[data[i].id].event_start->fields.owner);
		if (options::single_user != "" && owner.substr(0,options::single_user.length()) != options::single_user)
		    continue;
		user_job_count[owner] += 1;

		if (submit - last_user_submit[owner] <= 5*60)
		{
		    //cout << "Batch chain detected " << owner << " -- " << last_user_submit_jobid[owner] << "->" << data[i].id << endl;
		    if (batches.find(last_user_submit_jobid[owner]) == batches.end())
		    {
			batch_id.push_back(last_user_submit_jobid[owner]);
			batches.insert(make_pair(data[i].id,batch_id.size()-1));
		    }
		    else
		    {
			int64_t id = batches.find(last_user_submit_jobid[owner])->second;
			batches.insert(make_pair(data[i].id,id));
		    }
		}

		if (submit - last_user_completion[owner] <= 45*60) // && submit - last_user_submit[owner] > 60
		{
		    //cout << "Interactive chain detected " << owner << " -- " << last_user_completion_jobid[owner] << "->" << data[i].id << endl;
		    if (chains.find(last_user_completion_jobid[owner]) == chains.end())
		    {
			chain_id.push_back(last_user_completion_jobid[owner]);
			chains.insert(make_pair(data[i].id,chain_id.size()-1));
		    }
		    else
		    {
			int64_t id = chains.find(last_user_completion_jobid[owner])->second;
			chains.insert(make_pair(data[i].id,id));
		    }
		}
		last_user_submit[owner] = submit;
		last_user_submit_jobid[owner] = data[i].id;
	    }
	}

	if (data[i].event == 'E')
	{
	    last_user_completion[*(data[i].fields.owner)] = *(data[i].fields.time_compl);
	    last_user_completion_jobid[*(data[i].fields.owner)] = data[i].id;
	}
    }


    map<string,int64_t> batch_count;
    map<string,int64_t> chain_count;
    map<string,int64_t> batch_count2;
    map<string,int64_t> chain_count2;

    cout << "Detected chains:" << endl;

    fstream dotfile;
    dotfile.open("input.dot",ios_base::out | ios_base::trunc);

    dotfile << "digraph XXX {" << endl;

    //dotfile << "subgraph ";

    map<string,int64_t> batch_lengths_by_user;
    int id = 1;
    for (auto i : batch_id)
    {
	int64_t batch_length = 1;

	if (fulljobs.find(i)->second.event_start)
	if (fulljobs.find(i)->second.event_start->fields.owner)
	    batch_count[*(fulljobs.find(i)->second.event_start->fields.owner)] +=1;
	dotfile << "subgraph cluster" << id << " {" << endl;
	dotfile << "label = \"cluster" << id << "\";" << endl;
	dotfile << "job" << i.substr(0,7) << ";" << endl;

	cout << "Batch head " << i << endl;
	for (auto j : batches)
	{
	    if (j.second == id-1)
	    {
		dotfile << "job" << j.first.substr(0,7) << ";" << endl;
		if (fulljobs.find(j.first)->second.event_start)
		if (fulljobs.find(j.first)->second.event_start->fields.owner)
		    batch_count[*(fulljobs.find(j.first)->second.event_start->fields.owner)] +=1;
	    batch_length++;
	    }
	}
	id++;

	if (fulljobs.find(i)->second.event_start)
	if (fulljobs.find(i)->second.event_start->fields.owner)
	    batch_count2[*(fulljobs.find(i)->second.event_start->fields.owner)] += 1;

	//batch_lengths_by_user(make_pair(*(fulljobs.find(i)->second.event_start->fields.owner),batch_length));
	dotfile << "}" << endl;
    }


    map<string,int64_t> chain_lengths_by_user;
    id = 1;
    for (auto i : chain_id)
    {
	int64_t chain_length = 1;

	if (fulljobs.find(i)->second.event_start)
	if (fulljobs.find(i)->second.event_start->fields.owner)
	chain_count[*(fulljobs.find(i)->second.event_start->fields.owner)] += 1;
	//cout << "Chain " << i << endl;
	for (auto j : chains)
	{
	    if (j.second == id-1)
	    {
		dotfile << "job" << i.substr(0,7) << " -> " << "job" << j.first.substr(0,7) << ";" << endl;
		//cout << " " << j.first;
		if (fulljobs.find(j.first)->second.event_start)
		if (fulljobs.find(j.first)->second.event_start->fields.owner)
		chain_count[*(fulljobs.find(j.first)->second.event_start->fields.owner)] += 1;
		chain_length++;
	    }
	}
	id++;

	if (fulljobs.find(i)->second.event_start)
	if (fulljobs.find(i)->second.event_start->fields.owner)
	    chain_count2[*(fulljobs.find(i)->second.event_start->fields.owner)] += 1;

	//chain_length_by_user.insert(make_pair(*(fulljobs.find(i)->second.event_start->fields.owner),chain_length));
    }

    dotfile << "}" << endl;
    dotfile.close();

    cout << setw(16) << std::left << "name" << " " << setw(16) << "total jobs" << " " << setw(16) << std::left << "jobs in batches" << " " << setw(16) << std::left << "avg in batch" << " " << setw(16) << std::left << "jobs in chains" << " " << setw(16) << std::left << "avg in chain" << endl;
    for (auto i = user_job_count.begin(); i != user_job_count.end(); i++)
    {
	cout << setw(16) << std::left << i->first << " " << setw(16) << i->second << " " << setw(16) << std::left << batch_count[i->first] << " " << setw(16) << std::left << double(batch_count[i->first])/batch_count2[i->first] << " " << setw(16) << std::left << chain_count[i->first] << " " << setw(16) << std::left << double(chain_count[i->first])/chain_count2[i->first] << endl;
    }

    return 0;

    map<string,int64_t> starts_by_queue;
    map<string,int64_t> compls_by_queue;
    map<string,int64_t> swait_by_queue;
    map<string,int64_t> cwait_by_queue;

    map<string,int64_t> starts_by_user;
    map<string,int64_t> compls_by_user;
    map<string,int64_t> swait_by_user;
    map<string,int64_t> cwait_by_user;
    map<string,int64_t> cusage_by_user;

    set<string> known_starts;
    map<int64_t,int64_t> fprocs;
    map<int64_t,int64_t> fmem;

    int64_t total_waitS = 0;
    int64_t total_jobsS = 0;
    int64_t total_waitE = 0;
    int64_t total_jobsE = 0;
    int64_t total_jobsQ = 0;

    int64_t procs = 0;
    int64_t mem = 0;

    int64_t last_timestamp;

    int64_t arriv_counts[24][31] = {0};
    int64_t weekly_counts[24][7] = {0};

    map<string,int64_t> user_last_submit;
    map<string,int64_t> user_last_compl;
    map<string,vector<string> > users_sessions;
    map<string,vector<string> > users_batches;


    for (size_t i = 0; i < data.size(); ++i)
    {
	    if (jobs_black_list.find(data[i].id) != jobs_black_list.end())
		continue;

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

	    if (data[i].event == 'Q')
	    {
		++total_jobsQ;
		struct tm tm1;
		if (strptime(data[i].timestamp.c_str(), "%m/%d/%Y %H:%M:%S", &tm1) != NULL)
		{
		    arriv_counts[tm1.tm_hour][tm1.tm_mday]++;
		    time_t t = mktime(&tm1);
		    weekly_counts[tm1.tm_hour][localtime(&t)->tm_wday]++;
		}
	    }

	    if (options::utilization_only)
	    {
		    if (data[i].event == 'S' && known_starts.find(data[i].id) == known_starts.end())
		    {
			    known_starts.insert(data[i].id);
			    procs += *(data[i].fields.resc_total_cores);
			    mem += atoll((*(data[i].fields.resc_total_mem)).c_str())/1024;

			    if (*(data[i].fields.resc_total_cores) > 200)
				    cerr << "Suspicios job " << data[i].id << " cores req " << *(data[i].fields.resc_total_cores) << endl;

			    if (*(data[i].fields.resc_total_cores) <= 0)
				    cerr << "Suspicios job " << data[i].id << " cores req " << *(data[i].fields.resc_total_cores) << endl;

			    fprocs.insert(make_pair(static_cast<int64_t>(*(data[i].fields.time_start)),procs));
			    fmem.insert(make_pair(static_cast<int64_t>(*(data[i].fields.time_start)),mem));
			    last_timestamp = static_cast<int64_t>(*(data[i].fields.time_start));
		    }

		    if (data[i].event == 'E')
		    {
			    if (known_starts.find(data[i].id) != known_starts.end())
			    {
				    procs -= *(data[i].fields.resc_total_cores);
				    mem -= atoll((*(data[i].fields.resc_total_mem)).c_str())/1024;
				    if (mem == 0)
				    {
					    //cerr << "Weird mem" << endl;
				    }
				    fprocs.insert(make_pair(static_cast<int64_t>(*(data[i].fields.time_compl)),procs));
				    fmem.insert(make_pair(static_cast<int64_t>(*(data[i].fields.time_compl)),mem));

				    known_starts.erase(data[i].id);

				    last_timestamp = static_cast<int64_t>(*(data[i].fields.time_compl));
			    }
		    }

		    if ((data[i].event == 'A') || (data[i].event == 'D'))
		    {
			size_t j = i;

			if (known_starts.find(data[i].id) != known_starts.end())
			do
			{
				if (i != j && data[j].id == data[i].id && data[j].event == 'S')
				{
					procs -= *(data[j].fields.resc_total_cores);
					mem -= atoll((*(data[j].fields.resc_total_mem)).c_str())/1024;
					fprocs.insert(make_pair(last_timestamp,procs));
					fmem.insert(make_pair(last_timestamp,mem));
					known_starts.erase(data[i].id);
					//cerr << "Removing " << data[i].id << endl;
					break;
				}

				--j;

			} while (j != 0);

		    }
	    }

	    if (options::event_type == 'S' && data[i].event == 'S' && !options::utilization_only)
	    {
		    ++total_jobsS;
		    if (*(data[i].fields.time_start) < *(data[i].fields.time_arriv))
		    {
			    cerr << "Start: Bad start time " << *(data[i].fields.time_arriv) << " vs " << *(data[i].fields.time_start) << endl;
		    }

		    total_waitS += *(data[i].fields.time_start) - *(data[i].fields.time_arriv);

		    swait_by_queue[*(data[i].fields.queue)] += *(data[i].fields.time_start) - *(data[i].fields.time_arriv);
		    starts_by_queue[*(data[i].fields.queue)] += 1;

		    swait_by_user[*(data[i].fields.owner)] += *(data[i].fields.time_start) - *(data[i].fields.time_arriv);
		    starts_by_user[*(data[i].fields.owner)] += 1;
	    }

	    if (options::event_type == 'E' && data[i].event == 'E' && !options::utilization_only)
	    {
		    ++total_jobsE;
		    if (*(data[i].fields.time_start) < *(data[i].fields.time_arriv))
		    {
			    cerr << "End: Bad start time " << *(data[i].fields.time_arriv) << " vs " << *(data[i].fields.time_start) << endl;
		    }

		    total_waitE += *(data[i].fields.time_start) - *(data[i].fields.time_arriv);

		    cwait_by_queue[*(data[i].fields.queue)] += *(data[i].fields.time_start) - *(data[i].fields.time_arriv);
		    compls_by_queue[*(data[i].fields.queue)] += 1;

		    cwait_by_user[*(data[i].fields.owner)] += *(data[i].fields.time_start) - *(data[i].fields.time_arriv);
		    compls_by_user[*(data[i].fields.owner)] += 1;

		    cusage_by_user[*(data[i].fields.owner)] += ((*(data[i].fields.time_compl) - *(data[i].fields.time_start))*(*(data[i].fields.resc_total_cores)) + 59)/60;
	    }
    }

    if (options::utilization_only)
    {
	    fstream fsprocs,fsmem;
	    fsprocs.open("procs",fstream::out);
	    fsmem.open("mem",fstream::out);
	    for (auto && i : fprocs)
	    {
		fsprocs << i.first << " " << i.second << endl;
	    }

	    for (auto && i : fmem)
	    {
		fsmem << i.first << " " << i.second << endl;
	    }

	    fsprocs.close();
	    fsmem.close();

	    cout << "Unended jobs" << endl;
	    for (auto && i : known_starts)
	    {
		    cout << i << endl;
	    }

	    return 0;
    }


    cout << "Total events " << data.size() << endl;

    if (options::event_type == 'S')
    {
	cout << "Total Start Wait " << std::fixed << std::setprecision(2) << total_waitS/static_cast<double>(total_jobsS) << " (" << total_jobsS << ")" << endl;

	double avg_wait = 0;
	for (auto && i : swait_by_user)
	{
	    avg_wait += i.second/static_cast<double>(starts_by_user[i.first]);
	}

	cout << "User average wait time " << std::fixed << std::setprecision(2) << avg_wait/swait_by_user.size() << endl;
    }

    if (options::print_per_queue_info && options::event_type == 'S')
    for (auto && i : swait_by_queue)
    {
	    cout << std::setw(32) << i.first << "  :  " << std::setw(16) << std::fixed << std::setprecision(2) << i.second / static_cast<double>(starts_by_queue[i.first]) << "\t(" << starts_by_queue[i.first] << ")" << endl;
    }

    if (options::print_per_user_info && options::event_type == 'S')
    for (auto && i : swait_by_user)
    {
	    cout << std::setw(32) << i.first << "  :  " << std::setw(16) << std::fixed << std::setprecision(2) << i.second / static_cast<double>(starts_by_user[i.first]) << "\t(" << starts_by_user[i.first] << ")\t[" << i.second/static_cast<double>(total_jobsS) << "]" << endl;
    }


    if (options::event_type == 'E')
    {
	cout << "Total End Wait " << total_waitE/static_cast<double>(total_jobsE) << " (" << total_jobsE << ")" << endl;

	double avg_wait = 0;
	for (auto && i : cwait_by_user)
	{
	    avg_wait += i.second;
	}

	cout << "User average wait time " << std::fixed << std::setprecision(2) << avg_wait/cwait_by_user.size() << endl;
    }


    if (options::print_per_queue_info && options::event_type == 'E')
    for (auto && i : cwait_by_queue)
    {
	    cout << std::setw(32) << i.first << "  :  " << std::setw(16) << std::fixed << std::setprecision(2) << i.second / static_cast<double>(compls_by_queue[i.first]) << "\t(" << compls_by_queue[i.first] << ")" << endl;
    }

    if (options::print_per_user_info && options::event_type == 'E')
    for (auto && i : cwait_by_user)
    {
	    cout << std::setw(32) << i.first << "  :  " << std::setw(16) << std::fixed << std::setprecision(2) << i.second / static_cast<double>(compls_by_user[i.first]) << "\t(" << compls_by_user[i.first] << ")\t[" << i.second/static_cast<double>(total_jobsE) << "]" << endl;
    }

    double avg_nuwt = 0;
    int user_count = 0;
    for (auto && i : cusage_by_user)
    {
	    if (i.second > 60) // only consider users with at least one cpu hour
	    {
		avg_nuwt += cwait_by_user[i.first]/static_cast<double>(i.second);
		++user_count;
	    }
	    //cout << std::setw(32) << i.first << "  :  " << std::setw(16) << std::fixed << std::setprecision(2) << cwait_by_user[i.first]/static_cast<double>(i.second) << " (" << i.second << ")" << endl;
    }
    cout << "ANUWT " << avg_nuwt/user_count << endl << endl;

    int day_arrivals[7] = { 0 };
    int hour_arrivals[24] = { 0 };

    cout << "Hourly arrivals" << endl;
    for (int j = 0; j < 7; j++)
    for (int i = 0; i < 24; i++)
    {
	cout << weekly_counts[i][j] << endl;
	day_arrivals[j] += weekly_counts[i][j];
	hour_arrivals[i] += weekly_counts[i][j];
    }
    cout << endl;

    cout << "Day arrivals " << endl;
    for (int i = 0; i < 7; i++)
	cout << day_arrivals[i] << endl;
    cout << endl;

    cout << "Hourly arrivals " << endl;
    for (int i = 0; i < 24; i++)
	cout << hour_arrivals[i] << endl;
    cout << endl;

    return 0;
#endif
}

