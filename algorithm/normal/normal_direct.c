


//  FTL Direct Mapping

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
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


uint32_t normal_create (lower_info* li,blockmanager *a, algorithm *algo){
	algo->li=li; //lower_info
	return 1;
}

void normal_destroy (lower_info* li, algorithm *algo){
	//normal_cdf_print();

	return;
}

int normal_cnt=0;
int rem_cnt=0;

uint32_t normal_get(request *const req){ // READ
	normal_params* params=(normal_params*)malloc(sizeof(normal_params));

	algo_req *my_req=(algo_req*)malloc(sizeof(algo_req));
	my_req->parents=req;
	my_req->end_req=normal_end_req;
	my_req->param=(void*)params;
	my_req->type=DATAR;

	__normal.li->read((req->key)/4, PAGESIZE, req->value, my_req);
	return 1;
}
uint32_t normal_set(request *const req){ // WRITE
	normal_params* params=(normal_params*)malloc(sizeof(normal_params));

	algo_req *my_req=(algo_req*)malloc(sizeof(algo_req));
	my_req->parents=req;
	my_req->end_req=normal_end_req;

	static value_set* value;

	// value buffer
	if (req->key % 4 == 0) {
		value = inf_get_valueset(NULL, FS_MALLOC_W, PAGESIZE);
	}

	my_req->type=DATAW;
	my_req->param=(void*)params;
	memcpy((uint32_t*)&(value->value[4*K*(req->key%4)]), &req->key, sizeof(req->key));



	if (req->key % 4 == 3) {
		__normal.li->write((req->key)/4, PAGESIZE, value, my_req);//
		inf_free_valueset(value, FS_MALLOC_W);
	}else{
		req->end_req(req);
		free(params);
		free(my_req);
	}
	return 0;
}
uint32_t normal_remove(request *const req){
	__normal.li->trim_block(req->key);
	return 1;
}
void *normal_end_req(algo_req* input){
	normal_params* params=(normal_params*)(input->param);//
	request *res=input->parents;

	uint32_t ppa;
	switch (input->type) {
	case DATAR: //READ
		ppa = *(uint32_t*)&(res->value->value[4*K*((res->key)%4)]);
		normal_cnt++;
		if (normal_cnt > 100) {
			printf("exit over 100. done!\n");
			exit(0);
		}

		printf("lba:%u -> ppa:%u\n", res->key, ppa);
		if (ppa != res->key) {
			printf("WRONG!\n");
			exit(1);
		}
		break;
	case DATAW: //WRITE
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
