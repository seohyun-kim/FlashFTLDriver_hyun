

//  FTL Direct Mapping

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include "normal.h"
#include "../../bench/bench.h"

#include <sys/wait.h>


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


uint32_t normal_get(request *const req){ // READ
	normal_params* params=(normal_params*)malloc(sizeof(normal_params));
	params->test=-1;

	algo_req *my_req=(algo_req*)malloc(sizeof(algo_req));
	my_req->parents=req;
	my_req->end_req=normal_end_req; //end 호출
	my_req->param=(void*)params;
	my_req->type=DATAR;

	__normal.li->read(req->key, PAGESIZE, req->value, my_req);
	return 1;
}
uint32_t normal_set(request *const req){ // WRITE
	normal_params* params=(normal_params*)malloc(sizeof(normal_params));
	params->test=1;
	algo_req *my_req=(algo_req*)malloc(sizeof(algo_req));
	my_req->parents=req;
	my_req->end_req=normal_end_req; //end 호출

	// 임시 저장
	//value_set* value = inf_get_valueset(NULL, FS_MALLOC_W, PAGESIZE);

	//임시 저장 공간 free 
	//inf_free_valueset(value, FS_MALLOC_W);

	
	my_req->type=DATAW;
	my_req->param=(void*)params;
	
	memcpy(req->value->value, &req->key, sizeof(req->key));
	req->end_req(req);
	__normal.li->write(req->key, PAGESIZE, req->value, my_req);
	return 0;
}
uint32_t normal_remove(request *const req){
	__normal.li->trim_block(req->key);
	return 1;
}
void *normal_end_req(algo_req* input){
	normal_params* params=(normal_params*)(input->param);//
	request *res=input->parents;
	//res->end_req(res);

	uint32_t ppa;

	switch (input->type) {
		case DATAR: //READ
			
			ppa = *(uint32_t*)(res->value->value);
			normal_cnt ++;
			if(normal_cnt > 100){
				printf("exit over 100");	
				exit(0);
			}
			printf("lba:%u -> ppa:%u\n", res->key, ppa);
			if (ppa != res->key) {
				printf("WRONG!\n");
				exit(1);
			}
			break;
		case DATAW: //WRITE
			//params->test--;
			break;
		default:
			exit(1);
			break;
	}

	res -> end_req(res);
		
	free(params);
	free(input);
	return NULL;
}
