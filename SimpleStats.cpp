#include "SimpleStats.h"
using namespace std;

GlobalStats global_stats;

int generate_simple_stats()
{
	for (auto i : data)
	{
		switch(i.event)
		{
		case 'Q': global_stats.event_queued++; break;
		case 'S': global_stats.event_started++; break;
		case 'R': global_stats.event_reruns++; break;
		case 'C': global_stats.event_checkpoint++; break;
		case 'T': global_stats.event_restart_from_checkpoint++; break;
		case 'E': global_stats.event_completed++; break;
		case 'D': global_stats.event_delete++; break;
		case 'A': global_stats.event_abort++; break;
		case 'L': break;
		default: return 1;
		}
	}

	return 0;
}
