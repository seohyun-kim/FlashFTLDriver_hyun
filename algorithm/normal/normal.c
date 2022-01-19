



//  FTL Page Mapping (except GC)
//	2022-01-12

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <pthread.h>
#include "normal.h"
#include "../../interface/interface.h"
#include "../../bench/bench.h"


extern MeasureTime mt;
struct algorithm __normal = {
	.argument_set = NULL,
	.create = normal_create,
	.destroy = normal_destroy,
	.read = normal_get,
	.write = normal_set,
	.remove = normal_remove
};

static hyun_map* map_table; // mapping table
static __segment* hyun_segment; // segment for blockmanager
static uint32_t cnt_write_req;


uint32_t normal_create(lower_info* li, blockmanager* a, algorithm* algo) {
	map_table = (hyun_map*)calloc(RANGE, sizeof(hyun_map));
	algo->li = li; //lower_info
	algo->bm = a; //blockmanager
	hyun_segment = a->get_segment(a, BLOCK_ACTIVE); // first get_segment in create
	return 1;
}

void normal_destroy(lower_info* li, algorithm* algo) {
	free(map_table);
	//normal_cdf_print();
	return;
}

uint32_t normal_get(request* const req) { // READ
	normal_params* params = (normal_params*)malloc(sizeof(normal_params));
	algo_req* my_req = (algo_req*)malloc(sizeof(algo_req));

	my_req->parents = req;
	my_req->end_req = normal_end_req;
	my_req->param = (void*)params;
	my_req->type = DATAR;

	__normal.li->read((map_table[req->key].ppa)/4, PAGESIZE, req->value, my_req);
	return 1;
}

uint32_t normal_set(request* const req) { // WRITE
	normal_params* params = (normal_params*)malloc(sizeof(normal_params));
	algo_req* my_req = (algo_req*)malloc(sizeof(algo_req));
	static int32_t page_start_addr; // return value of [get_page_addr], current write loc
	static value_set* value; // value buffer

	my_req->parents = req;
	my_req->end_req = normal_end_req;
	params->value_buf = value;
	my_req->type = DATAW;
	my_req->param = (void*)params;

	

	// ===================
	// Garbage Collection
	if (__normal.bm->is_gc_needed(__normal.bm) == true) {
		// 1. Invalidate page 많은 block(segment)선택 : [get_gc_target]
		// 2. valid data를 현재 위치의 다음 공간에 [Invalidate piece num] 만큼 WRITE
		// 3. 원래 공간 ERASE 
		//	- bm->bit_unset 
		//	- target_segment->blocks[i]->bitset ? 해당 블록만 지우는 함수 뭔지 모르겠음
		// 4. mapping table update

		__gsegment* target_segment = __normal.bm->get_gc_target(__normal.bm);
		uint32_t current_write_loc = L2PGAP * page_start_addr + (cnt_write_req % L2PGAP);

		// WRITE (valid data copy)
		for (int i = 0; i < target_segment->validate_piece_num; i++)
		{
			__normal.li->write(page_start_addr, PAGESIZE, 타겟블록원래값, my_req); // ?
			// WRITE 16K단위
		}

		// ERASE garbage segment
		for (int i = 0; i < BPS; i++) // BPS가 세그먼트 내 블록 수 인가?
		{
			__normal.bm->bit_unset(__normal.bm, target_segment->blocks[i]->block_idx);
			// seg_idx 가 gc타겟 되는 세그먼트의 원래 LBA인가?

			// ERASE : set all blocks - bit 1
			memset(target_segment->blocks[i]->bitset, 1, _PPB * L2PGAP / 8);

			// update mapping table
			map_table[target_segment->seg_idx].ppa = current_write_loc;
			current_write_loc++;
		}
		
		cnt_write_req += target_segment->validate_piece_num;

	}

	// ===================


	if (map_table[req->key].is_lba_re_req == true){ // if same lba re-req
		__normal.bm->bit_unset(__normal.bm, map_table[req->key].ppa);  // origin mem unset
	}

	if (__normal.bm->check_full(hyun_segment)) { // check if segment is full
		hyun_segment = __normal.bm->get_segment(__normal.bm, BLOCK_ACTIVE); // get new segment
	}

	if (cnt_write_req % L2PGAP == 0) { // Once per 4 times (every first in 4)
		value = inf_get_valueset(NULL, FS_MALLOC_W, PAGESIZE);
		page_start_addr = __normal.bm->get_page_addr(hyun_segment); 
	}

	// write ppa in map
	map_table[req->key].ppa =  L2PGAP*page_start_addr +(cnt_write_req % L2PGAP);

	//copy in buffer (to collect four at a time)
	memcpy((uint32_t*)&(value->value[4 * K * (cnt_write_req % 4)]), &req->key, sizeof(req->key)); 

	__normal.bm->bit_set(__normal.bm, map_table[req->key].ppa); // bit set
	map_table[req->key].is_lba_re_req = true; // flag : LBA called

	if (cnt_write_req % L2PGAP == 3) { // Once per 4 times (every last in 4)
		__normal.li->write((map_table[req->key].ppa) / 4, PAGESIZE, value, my_req);
	}
	else {
		req->end_req(req);
		free(params);
		free(my_req);
	}
	cnt_write_req++;

	return 0;
}
uint32_t normal_remove(request* const req) {
	__normal.li->trim_block(req->key);
	return 1;
}
void* normal_end_req(algo_req* input) {
	normal_params* params = (normal_params*)(input->param);//
	request* res = input->parents;

	uint32_t data;
	switch (input->type) {
	case DATAR: //READ
		data = *(uint32_t*)&(res->value->value[4*K*(map_table[res->key].ppa %4)]);
		printf("lba:%u -> ppa:%u / data: %u\n", res->key, map_table[res->key].ppa, data);
		if (data != res->key) {
			printf("WRONG!\n");
			exit(1);
		}
		break;
	case DATAW: //WRITE
		inf_free_valueset(params->value_buf, FS_MALLOC_W);
		break;
	default:
		exit(1);
		break;
	}

	res->end_req(res);
	free(params);
	free(input);
	return NULL;
}
