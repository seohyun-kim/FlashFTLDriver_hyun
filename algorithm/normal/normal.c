//  FTL Direct Mapping

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include "normal.h"
#include "../../bench/bench.h"

#define LOWERTYPE 10

extern MeasureTime mt;
struct algorithm __normal = {
	.argument_set = NULL,
	.create = normal_create,
	.destroy = normal_destroy,
	.read = normal_get,
	.write = normal_set,
	.remove = normal_remove
};

n_cdf _cdf[LOWERTYPE];

//char temp[PAGESIZE];
uint_fast8_t temp[PAGESIZE]; //unsigned char로 변경
//======== temp array의 역할? 

void normal_cdf_print(){
	// 
	for (int i = 0; i < LOWERTYPE; i++)
	{
		printf("[normal_cdf]\n total_micro: %lu\n cnt: %lu\n max: %lu\n min:%lu\n",
			_cdf->total_micro, _cdf ->cnt, _cdf->max, _cdf->min);
	}
}

uint32_t normal_create (lower_info* li,blockmanager *a, algorithm *algo){
	algo->li=li; //lower_info

	//memset(temp,'x',PAGESIZE); //초기화('x'= 0110 1000)
	//왜 x로 초기화 하지? 이론상 1111 1111이 아닌가?
	memset(temp, UCHAR_MAX, PAGESIZE);

	for(int i=0; i<LOWERTYPE; i++){
		_cdf[i].min=UINT_MAX;
	}// _cdf 초기화

	return 1;
}

void normal_destroy (lower_info* li, algorithm *algo){
	normal_cdf_print();
	// destroy
	return;
}

int normal_cnt; //idx


uint32_t normal_get(request *const req){
	normal_params* params=(normal_params*)malloc(sizeof(normal_params));
	params->test=-1;

	algo_req *my_req=(algo_req*)malloc(sizeof(algo_req));
	my_req->parents=req;
	my_req->end_req=normal_end_req; //end 호출
	my_req->params=(void*)params;
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
	my_req->params=(void*)params;
	static int cnt=0;
	//if(cnt++%10240==0){
	//	printf("key:%d\n",req->key);
	//}
	//__normal.li->write(req->key,PAGESIZE,req->value,req->isAsync,my_req);
	__normal.li->write(req->key, PAGESIZE, req->value, my_req);
	return 0;
}
uint32_t normal_remove(request *const req){
	//__normal.li->trim_block(req->key, false);
	__normal.li->trim_block(req->key);
	return 1;
}
void *normal_end_req(algo_req* input){
	normal_params* params=(normal_params*)(input->params);//
	//bool check=false;
	//int cnt=0;
	request *res=input->parents; //parents 넘겨줌
	res->end_req(res); //end 요청

	free(params);
	free(input);
	return NULL;
}