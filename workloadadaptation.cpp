#include "workloadadaptation.h"
#include "Data.h"
#include "Utils.h"
#include <sstream>
#include <fstream>
#include <iostream>
#include "Options.h"
using namespace std;

Batch::Batch(JobData *first_job, unsigned sessionID, unsigned batchID)
{
  first_arrival = *(first_job->fields.time_arriv);
  last_completion = *(first_job->fields.time_compl);
  global_id.first = sessionID;
  global_id.second = batchID;
  job_info.push_back(first_job);
}

void Batch::add_job(JobData *job)
{
  if (first_arrival > *(job->fields.time_arriv))
    cerr << "Bad ordering between jobs. Earlier arrival processed after later arrival. (" << *(job->fields.time_arriv) << ") diff (" << (first_arrival - *(job->fields.time_arriv)) << ")" << endl;

  if (last_completion < *(job->fields.time_compl))
    last_completion = *(job->fields.time_compl);

  job_info.push_back(job);
}

void Batch::add_completion_dep(unsigned session, unsigned batch)
{
  after_completion.push_back(make_pair(session,batch));
}

void Batch::add_completion_dep(pair<unsigned,unsigned> globalID)
{
  after_completion.push_back(globalID);
}

void Batch::add_arrival_dep(pair<unsigned,unsigned> globalID)
{
  after_arrival.push_back(globalID);
}

void Batch::add_arrival_dep(unsigned session, unsigned batch)
{
  after_arrival.push_back(make_pair(session,batch));
}

string Batch::get_batch_label()
{
  stringstream s;
  s << "Batch_" << global_id.first << "_" << global_id.second;
  return s.str();
}

Session::Session(JobData* first_job, unsigned sessionID)
{
  first_arrival = first_job->fields.time_arriv.get();
  last_arrival = first_job->fields.time_arriv.get();
  last_completion = first_job->fields.time_compl.get();
  last_batch_id = 0;
  batches.push_back(Batch(first_job,sessionID,last_batch_id));
  session_id = sessionID;
}

unsigned Session::add_job(JobData* job)
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

void Session::setup_internal_dependencies()
{
  for (size_t i = 0; i < batches.size(); i++)
  {
    if (i > 0)
      batches[i].add_completion_dep(session_id,batches[i-1].global_id.second);
  }
}

void Session::add_completion_dep(unsigned session, unsigned batch)
{
  batches[0].add_completion_dep(session,batch);
}

void Session::add_completion_dep(pair<unsigned,unsigned> globalID)
{
  batches[0].add_completion_dep(globalID);
}

void Session::add_arrival_dep(pair<unsigned,unsigned> globalID)
{
  batches[0].add_arrival_dep(globalID);
}

void Session::add_arrival_dep(unsigned session, unsigned batch)
{
  batches[0].add_arrival_dep(session,batch);
}

string Session::get_session_label()
{
  stringstream s;
  s << "Session_" << session_id;
  return s.str();
}

void detect_sessions(map<string, vector<Session> >& sessions)
  {
    // go over the pre-processed user data, user-by-user
    for (auto& i : data_by_user)
    {
      string username = i.first.substr(0,i.first.find('@'));
      unsigned last_session_id = 0;
      unsigned last_batch_id = 0;

      sessions.insert(make_pair(username,vector<Session>()));

      int64_t last_arrival = 0;
      int64_t this_arrival = 0;

      vector<Session>& user_session = sessions[username];

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
    }
  }

void store_sessions(map<string, vector<Session> >& sessions)
  {
  fstream base_workload;
  base_workload.open(options::write_workload+".dyn",ios_base::out | ios_base::trunc);

  int user_id = 1;

  // store a description file
  for (auto& i : data_by_user)
  {
    string username = i.first.substr(0,i.first.find('@'));
    vector<Session>& user_session = sessions[i.first];
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
         << ts.tm_wday << "\t" << ts.tm_hour << "\t" << ts.tm_min;

      time_t session_end = j.last_arrival;
      ts = *localtime(&session_end);

      sessions << "\t" << ts.tm_wday << "\t" << ts.tm_hour << "\t" << ts.tm_min << endl;

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

  base_workload.close();
  }
