



//  FTL Page Mapping (except GC)
//	2022-01-12 

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <pthread.h>
#include "normal.h"
#include "../../interface/interface.h"
#include "../../include/sem_lock.h"
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

static hyun_map* map_table;
static uint32_t cnt_write_req;
static __segment* hyun_segment; //

uint32_t normal_create(lower_info* li, blockmanager* a, algorithm* algo) {
	map_table = (hyun_map*)calloc(RANGE, sizeof(hyun_map));
	//static uint32_t cnt_write_req = 0;
	algo->li = li; //lower_info
	algo->bm = a; //blockmanager
	hyun_segment = __normal.bm->get_segment(__normal.bm, BLOCK_ACTIVE); // first get_segment in create

	return 1;
}

void normal_destroy(lower_info* li, algorithm* algo) {
	free(map_table);
	//normal_cdf_print();

	return;
}

int normal_cnt = 0;
int rem_cnt = 0;


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
	hyun_segment = (__segment*)malloc(sizeof(__segment)); //
	my_req->parents = req;
	my_req->end_req = normal_end_req;


	if (map_table[req->key].is_lba_re_req == true){ // if same lba re-req
		__normal.bm->bit_unset(__normal.bm, map_table[req->key].ppa);  // origin mem unset
		//map_table[req->key].ppa = 0;
	}

	if (__normal.bm->check_full(hyun_segment)) { // if segment is full
		hyun_segment = __normal.bm->get_segment(__normal.bm, BLOCK_ACTIVE); // get segment
	}

	//printf("checkfull: %d\n", __normal.bm->check_full(hyun_segment) );

	static int32_t page_start_addr;
	// value buffer
	static value_set* value;
	if (cnt_write_req % L2PGAP == 0) {
		value = inf_get_valueset(NULL, FS_MALLOC_W, PAGESIZE);
		page_start_addr = __normal.bm->get_page_addr(hyun_segment); 
	}

	map_table[req->key].ppa = LPAGESIZE* (L2PGAP*page_start_addr +(cnt_write_req % L2PGAP));
	printf("page_start_addr: %d/ ppa : %u \n", page_start_addr, map_table[req->key].ppa);

	params->value_buf = value;
	my_req->type = DATAW;
	my_req->param = (void*)params;

	memcpy((uint32_t*)&(value->value[4 * K * (cnt_write_req % 4)]), &req->key, sizeof(req->key)); //버퍼에 copy
	__normal.bm->bit_set(__normal.bm, map_table[req->key].ppa);
	map_table[req->key].is_lba_re_req = true;

	if (cnt_write_req % L2PGAP == 3) { //  모아서 쓰기
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
		//normal_cnt++;
		//if (normal_cnt > 100) {
			//printf("exit over 100. done!\n");
			//free(map_table);
			//exit(0);
		//}
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
		printf("normal_end_req__default");
		exit(1);
		break;
	}

	res->end_req(res);
	free(params);
	free(input);
	return NULL;
}
