#include "../../include/container.h"

typedef struct gc_value {
	uint32_t ppa;
	value_set* value;
	bool isdone;
}gc_value;


void* page_gc_end_req(algo_req* input);
gc_value* send_req(uint32_t ppa, uint8_t type, value_set* value);
void travel_page_in_segment(algorithm* __normal, __gsement* _target_segment, __segment* __reserve_segment)
void run_hyun_gc(algorithm* __normal);
