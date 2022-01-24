#include "../../include/container.h"
#include "../../interface/interface.h"

typedef struct gc_value {
	uint32_t ppa;
	value_set* value;
	bool isdone;
}gc_value;

void run_hyun_gc(algorithm*);

void* page_gc_end_req(algo_req*);
gc_value* send_req(uint32_t , uint8_t , value_set* );
void travel_page_in_segment(algorithm* , __gsegment* , __segment*);

//void run_hyun_gc(algorithm* __normal);
