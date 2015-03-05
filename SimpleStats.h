#ifndef SIMPLESTATS_H
#define SIMPLESTATS_H

// INTERNAL
#include "Data.h"

#include <stdint.h>

struct GlobalStats
{
	int64_t event_queued;
	int64_t event_started;
	int64_t event_reruns;
	int64_t event_completed;
	int64_t event_delete;
	int64_t event_abort;
	int64_t event_checkpoint;
	int64_t event_restart_from_checkpoint;

	GlobalStats() : event_queued(0), event_started(0), event_reruns(0),
		event_completed(0), event_delete(0), event_abort(0),
		event_checkpoint(0), event_restart_from_checkpoint(0) {}
};

extern GlobalStats global_stats;

int generate_simple_stats();

#endif // SIMPLESTATS_H
