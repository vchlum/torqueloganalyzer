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
}

int parse_options(int argc, char *argv[])
{
	po::options_description desc("Available options");
	desc.add_options()
			("help", "Display this help.")
			("simple-stats", "Print a summary overview of the processed events.")
			("max-threads", po::value<size_t>(), "Set the number of threads used for parsing the input.")
			("single-user", po::value<string>(), "print information about a single user")
			("detect-sessions", "Print sessions detected in the workload.")
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

	if (vm.count("maximum_threads"))
	{
		options::maximum_threads = vm["maximum_threads"].as<size_t>();
	}
	else
	{
		options::maximum_threads = 1;
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

	return 0;
}
