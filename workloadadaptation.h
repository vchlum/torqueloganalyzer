#ifndef WORKLOADADAPTATION_H
#define WORKLOADADAPTATION_H

#include <map>
#include <string>
#include <vector>

#include "Data.h"

struct Batch
{
  int64_t first_arrival;
  int64_t last_completion;
  std::vector<JobData*> job_info;
  std::pair<unsigned, unsigned> global_id;

  Batch(JobData *first_job, unsigned sessionID, unsigned batchID);
  void add_job(JobData *job);

  std::vector< std::pair<unsigned,unsigned> > after_arrival;
  std::vector< std::pair<unsigned,unsigned> > after_completion;

  void add_completion_dep(unsigned session, unsigned batch);
  void add_completion_dep(std::pair<unsigned,unsigned> globalID);
  void add_arrival_dep(std::pair<unsigned,unsigned> globalID);
  void add_arrival_dep(unsigned session, unsigned batch);

  std::string get_batch_label();
};

struct Session
{
  int64_t first_arrival;
  int64_t last_arrival;
  int64_t last_completion;
  std::vector<Batch> batches;

  unsigned session_id;
  unsigned last_batch_id;

  Session(JobData* first_job, unsigned sessionID);
  unsigned add_job(JobData* job);
  void setup_internal_dependencies();

  void add_completion_dep(unsigned session, unsigned batch);
  void add_completion_dep(std::pair<unsigned,unsigned> globalID);
  void add_arrival_dep(std::pair<unsigned,unsigned> globalID);
  void add_arrival_dep(unsigned session, unsigned batch);

  std::string get_session_label();
};


void detect_sessions(std::map<std::string, std::vector<Session> >& sessions);
void store_sessions(std::map<std::string, std::vector<Session> >& sessions);

#endif // WORKLOADADAPTATION_H
