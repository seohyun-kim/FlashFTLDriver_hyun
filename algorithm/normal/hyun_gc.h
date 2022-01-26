#include "../../include/container.h"
#include "../../interface/interface.h"
#include "../../include/data_struct/list.h"

typedef struct align_gc_buffer {
	uint8_t idx;
	char value[PAGESIZE];
	KEYT key[L2PGAP];
}align_gc_buffer;

typedef struct gc_value {
	uint32_t ppa;
	value_set* value;
	bool isdone;
}gc_value;

typedef struct page_map_body {
	uint32_t* mapping;
	bool isfull;
	uint32_t assign_page;

	/*segment is kind of Physical Block*/
	__segment* reserve; //for gc
	__segment* active; //for gc
}pm_body;


void run_hyun_gc(algorithm*, __segment*);

void* page_gc_end_req(algo_req*);
gc_value* send_req(uint32_t , uint8_t , value_set* , algorithm*);
void travel_page_in_segment(algorithm* , __gsegment* , __segment*);
void list_delete_node(list*,li_node*);
