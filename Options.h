#ifndef OPTIONS_H
#define OPTIONS_H

#include<vector>
#include<string>

namespace options
{
    extern size_t maximum_threads;
    extern std::vector<std::string> inputs;
    extern std::string single_user;
    extern bool simple_stats;
    extern bool detect_sessions;
}

int parse_options(int argc, char *argv[]);

#endif // OPTIONS_H
