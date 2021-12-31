//  FTL Direct Mapping

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include "normal.h"
#include "../../bench/bench.h"

//#define LOWERTYPE 10

extern MeasureTime mt;
struct algorithm __normal = {
	.argument_set = NULL,
	.create = normal_create,
	.destroy = normal_destroy,
	.read = normal_get,
	.write = normal_set,
	.remove = normal_remove
};

//n_cdf _cdf[LOWERTYPE];

//char temp[PAGESIZE];


//void normal_cdf_print(){
//	// 
//	for (int i = 0; i < LOWERTYPE; i++)
//	{
//		printf("[normal_cdf]\n total_micro: %lu\n cnt: %lu\n max: %lu\n min:%lu\n",
//			_cdf->total_micro, _cdf ->cnt, _cdf->max, _cdf->min);
//	}
//}

uint32_t normal_create (lower_info* li,blockmanager *a, algorithm *algo){
	algo->li=li; //lower_info

	//memset(temp,'x',PAGESIZE);

	//for(int i=0; i<LOWERTYPE; i++){
	//	_cdf[i].min=UINT_MAX;
	//}// _cdf 초기화

	return 1;
}

void normal_destroy (lower_info* li, algorithm *algo){
	//normal_cdf_print();

	return;
}

int normal_cnt;


uint32_t normal_get(request *const req){
	normal_params* params=(normal_params*)malloc(sizeof(normal_params));
	params->test=-1;

	algo_req *my_req=(algo_req*)malloc(sizeof(algo_req));
	my_req->parents=req;
	my_req->end_req=normal_end_req; //end 호출
	my_req->param=(void*)params;
	normal_cnt++;
	my_req->type=DATAR;

	//__normal.li->read(req->key,PAGESIZE,req->value,req->isAsync,my_req);
	__normal.li->read(req->key, PAGESIZE, req->value, my_req);
	return 1;
}
uint32_t normal_set(request *const req){
	normal_params* params=(normal_params*)malloc(sizeof(normal_params));
	params->test=-1;

	algo_req *my_req=(algo_req*)malloc(sizeof(algo_req));
	my_req->parents=req;
	my_req->end_req=normal_end_req;
	normal_cnt++;
	my_req->type=DATAW;
	my_req->param=(void*)params;
	static int cnt=0;

	__normal.li->write(req->key, PAGESIZE, req->value, my_req);
	//printf("\n [ req-> key : %d , req -> value %s]\n", req->key, req->value-> value);
	return 0;
}
uint32_t normal_remove(request *const req){
	//__normal.li->trim_block(req->key, false);
	__normal.li->trim_block(req->key);
	return 1;
}
void *normal_end_req(algo_req* input){
	normal_params* params=(normal_params*)(input->param);//
	//bool check=false;
	//int cnt=0;
	request *res=input->parents;
	res->end_req(res);

	free(params);
	free(input);
	return NULL;
}
