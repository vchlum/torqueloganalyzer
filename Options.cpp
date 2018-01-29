#include "Options.h"

// C++ STD
#include <iostream>
using namespace std;

// BOOST
#include <boost/program_options.hpp>
namespace po = boost::program_options;

namespace options
{
	size_t maximum_threads;
	vector<string> inputs;
	string single_user = "";
	bool simple_stats = false;
	bool detect_sessions = false;
	bool validate_job_data = false;
	std::string queue_filter = "";
	std::string user_filter = "";
	std::string write_workload = "";
	bool ignore_array_jobs = false;
}

int parse_options(int argc, char *argv[])
{
	po::options_description desc("Available options");
	desc.add_options()
			("help", "Display this help.")
			("simple-stats", "Print a summary overview of the processed events.")
			("single-user", po::value<string>(), "print information about a single user")
			("detect-sessions", "Print sessions detected in the workload.")
			("validate-job-data", "Validate job data, only valid jobs will be processed.")
			("single-queue", po::value<string>(), "Only process jobs from a single queue.")
			("single-user", po::value<string>(), "Only process jobs from a single user.")
			("write-workload", po::value<string>(), "Write workload to the specified filename.")
			("ignore-array-jobs", "Ignore all array jobs")
			;

	po::options_description hidden("Hidden options");
	hidden.add_options()
			("input-file", po::value< vector<string> >(), "Input file to parse.")
			;

	po::positional_options_description p;
	p.add("input-file", -1);

	po::options_description cmdline_options;
	cmdline_options.add(desc).add(hidden);

	po::variables_map vm;
	po::store(po::command_line_parser(argc,argv).options(cmdline_options).positional(p).run(),vm);
	po::notify(vm);

	if (vm.count("help"))
	{
		cout << desc << endl;
		return 1;
	}

	if (vm.count("input-file"))
	{
		options::inputs = vm["input-file"].as< vector<string> >();
	}

	if (vm.count("single-user"))
	{
		options::single_user = vm["single-user"].as<string>();
	}

	if (vm.count("simple-stats"))
	{
		options::simple_stats = true;
	}

	if (vm.count("detect-sessions"))
	{
		options::detect_sessions = true;
	}

	if (vm.count("single-queue"))
	{
		options::queue_filter = vm["single-queue"].as<string>();
	}

	if (vm.count("single-user"))
	{
		options::queue_filter = vm["single-user"].as<string>();
	}

	if (vm.count("write-workload"))
	{
		options::write_workload = vm["write-workload"].as<string>();
	}

	if (vm.count("ignore-array-jobs"))
	{
		options::ignore_array_jobs = true;
	}
	return 0;
}
