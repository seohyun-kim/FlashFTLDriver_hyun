
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


uint32_t normal_create(lower_info* li, blockmanager* a, algorithm* algo) {
	//static hyun_map *map_table = (hyun_map*)malloc(RANGE*sizeof(hyun_map));
	//static uint32_t cnt_write_req = 0;
	algo->li = li; //lower_info
	return 1;
}

void normal_destroy(lower_info* li, algorithm* algo) {
	//free(map_table);
	//normal_cdf_print();

	return;
}

int normal_cnt = 0;
int rem_cnt = 0;

static hyun_map *map_table = (hyun_map*)malloc(RANGE*sizeof(hyun_map));
static uint32_t cnt_write_req = 0;

//static hyun_map map_table[RANGE]; //mapping table
//static uint32_t cnt_write_req=0; // 몇번째 요청?


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

	//mapping
	//fdriver_lock(&(map_table[req->key].write_lock));
	map_table[req->key].ppa = cnt_write_req;

	normal_params* params = (normal_params*)malloc(sizeof(normal_params));
	algo_req* my_req = (algo_req*)malloc(sizeof(algo_req));
	my_req->parents = req;
	my_req->end_req = normal_end_req;

	// value buffer
	static value_set* value;
	if (cnt_write_req % 4 == 0) {
		value = inf_get_valueset(NULL, FS_MALLOC_W, PAGESIZE);
	}
	params->value_buf = value;
	my_req->type = DATAW;
	my_req->param = (void*)params;
	memcpy((uint32_t*)&(value->value[4 * K * (cnt_write_req % 4)]), &req->key, sizeof(req->key));

	if (cnt_write_req % 4 == 3) {
		__normal.li->write((map_table[req->key].ppa)/4, PAGESIZE, value, my_req);//
		//inf_free_valueset(value, FS_MALLOC_W);
	}
	else {
		req->end_req(req);
		free(params);
		free(my_req);
	}
	cnt_write_req++;
	//fdriver_unlock(&(map_table[req->key].write_lock));

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
		normal_cnt++;
		if (normal_cnt > 100) {
			printf("exit over 100. done!\n");
			free(map_table);
			exit(0);
		}
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
