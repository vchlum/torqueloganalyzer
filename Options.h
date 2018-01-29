#ifndef OPTIONS_H
#define OPTIONS_H

#include<vector>
#include<string>

namespace options
{
    extern std::vector<std::string> inputs;
    extern std::string single_user;
    extern bool simple_stats;
    extern bool detect_sessions;
    extern bool validate_job_data;
    extern std::string queue_filter;
    extern std::string user_filter;
    extern std::string write_workload;
    extern bool ignore_array_jobs;
}

int parse_options(int argc, char *argv[]);

#endif // OPTIONS_H
