#include "amf_info.h"
#include "../../include/settings.h"
#include "../../bench/bench.h"
#include "../../include/debug_utils.h"
#include "./block_buffer_write.h"
#include "./normal_write.h"
#include <pthread.h>
#include <unistd.h>
#include <queue>
AmfManager *am;
/*testtest*/
#ifdef LOWER_MEM_DEV
char **mem_pool;
char *temp_mem_buf;
#endif

void amf_error_call_back_r(void *req);
void amf_error_call_back_w(void *req);
void amf_error_call_back_e(void *req);

void amf_traffic_print(lower_info *);

lower_info amf_info={
	.create=amf_info_create,
	.destroy=amf_info_destroy,
	.write=amf_info_write,
	.read=amf_info_read,
	.write_sync=amf_info_write_sync,
	.read_sync=amf_info_read_sync,
	.device_badblock_checker=NULL,
	.trim_block=amf_info_trim_block,
	.trim_a_block=NULL,
	.refresh=amf_info_refresh,
	.stop=amf_info_stop,
	.lower_alloc=NULL,
	.lower_free=NULL,
	.lower_flying_req_wait=amf_flying_req_wait,
	.lower_show_info=amf_info_show_info,

	.lower_tag_num=amf_info_lower_tag_num,
	.print_traffic=amf_traffic_print,

	.dump=amf_info_dump,
	.load=amf_info_load,
};

void amf_traffic_print(lower_info *li){
	static int cnt=0;
	printf("traffic print: %u\n", cnt++);
	for(int i=0; i<LREQ_TYPE_NUM;i++){
		fprintf(stderr,"%s %lu\n",bench_lower_type(i),li->req_type_cnt[i]);
	}
}

static inline void __amf_info_create_body(){
#ifndef TESTING
	if (access("aftl.bin", F_OK)!=-1){
		am=AmfOpen(1);
	}
	else{
		am=AmfOpen(2);
	}
#endif

#if BPS!=AMF_PUNIT
	printf("buffer_block wirte on lower_info");
	p_bbuf_init();
#else
	printf("normal wirte on lower_info\n");
	normal_write_init();
#endif
	mem_pool=(char**)malloc(sizeof(char*)*_NOP);
	temp_mem_buf=(char*)malloc(PAGESIZE);
}

uint32_t amf_info_create(lower_info *li, blockmanager *bm){
	__amf_info_create_body();

#ifdef LOWER_MEM_DEV
	printf("lower mem dev  mode\n");
	for(uint32_t i=0; i<_NOP; i++){
		mem_pool[i]=(char*)malloc(PAGESIZE);
	}
#endif
	return 1;
}

void* amf_info_destroy(lower_info *li){
	amf_flying_req_wait();

	for(int i=0; i<LREQ_TYPE_NUM;i++){
		fprintf(stderr,"%s %lu\n",bench_lower_type(i),li->req_type_cnt[i]);
	}

    fprintf(stderr,"Total Read Traffic : %lu\n", li->req_type_cnt[1]+li->req_type_cnt[3]+li->req_type_cnt[5]+li->req_type_cnt[7]);
    fprintf(stderr,"Total Write Traffic: %lu\n\n", li->req_type_cnt[2]+li->req_type_cnt[4]+li->req_type_cnt[6]+li->req_type_cnt[8]);
    fprintf(stderr,"Total WAF: %.2f\n\n", (float)(li->req_type_cnt[2]+li->req_type_cnt[4]+li->req_type_cnt[6]+li->req_type_cnt[8]) / li->req_type_cnt[6]);

	li->write_op=li->read_op=li->trim_op=0;


#if BPS!=AMF_PUNIT
	p_bbuf_free();
#else
	normal_write_free();
#endif


#ifdef LOWER_MEM_DEV
	for(uint32_t i=0; i<_NOP; i++){
		free(mem_pool[i]);
	}
	free(mem_pool);
	free(temp_mem_buf);
#endif

#ifndef TESTING
	AmfClose(am);
#endif
	return NULL;
}

void* amf_info_write(uint32_t ppa, uint32_t size, value_set *value,algo_req * const req){
	collect_io_type(req->type, &amf_info);

	req->test_ppa=ppa;
	req->type_lower=0;

	memcpy(mem_pool[ppa], value->value, PAGESIZE);
#if BPS!=AMF_PUNIT
	p_bbuf_issue(LOWER_WRITE, ppa, temp_mem_buf, req);
#else
	normal_write_issue(LOWER_WRITE, ppa, temp_mem_buf, req);
#endif
	return NULL;
}


void* amf_info_read(uint32_t ppa, uint32_t size, value_set *value,algo_req * const req){
	collect_io_type(req->type, &amf_info);

	req->test_ppa=ppa;
	req->type_lower=0;

	memcpy(value->value, mem_pool[ppa], PAGESIZE);
#if BPS!=AMF_PUNIT
	p_bbuf_issue(LOWER_READ, ppa, temp_mem_buf, req);
#else
	normal_write_issue(LOWER_READ, ppa, temp_mem_buf, req);
#endif

	return NULL;
}

void* amf_info_trim_block(uint32_t ppa){
	collect_io_type(TRIM, &amf_info);

#if BPS!=AMF_PUNIT
	if(REAL_PPA(ppa,0)!=0){
		printf("not aligned! %u\n", ppa);
		print_stacktrace();
		abort();
	}
	p_bbuf_issue(LOWER_TRIM, ppa, NULL, NULL);
#else
	if((ppa*R2PGAP)%PAGES_PER_SEGMENT){
		printf("not aligned! %u\n", ppa);
		//print_stacktrace();
		abort();
	}
	normal_write_issue(LOWER_TRIM, ppa, NULL, NULL);
#endif
	return NULL;
}


void* amf_info_refresh(struct lower_info* li){
	li->write_op=li->read_op=li->trim_op=0;
	return NULL;
}

void amf_info_stop(){}

void amf_info_show_info(){}

uint32_t amf_info_lower_tag_num(){
	return NUM_TAGS;
}

void amf_flying_req_wait(){
#ifndef TESTING
	while(IsAmfBusy(am)){}
#endif
	return;
}

void *amf_info_write_sync(uint32_t type, uint32_t ppa, char *data){
	collect_io_type(type, &amf_info);
	memcpy(mem_pool[ppa], data, PAGESIZE);
#if BPS!=AMF_PUNIT
	p_bbuf_sync_issue(LOWER_WRITE, ppa, temp_mem_buf);
#else
	normal_write_sync_issue(LOWER_WRITE, ppa, temp_mem_buf);
#endif
	return NULL;
}

void *amf_info_read_sync(uint32_t type, uint32_t ppa, char *data){
	collect_io_type(type, &amf_info);
#if BPS!=AMF_PUNIT
	p_bbuf_sync_issue(LOWER_READ, ppa, temp_mem_buf);
#else
	normal_write_sync_issue(LOWER_READ, ppa, temp_mem_buf);
#endif
	memcpy(data, mem_pool[ppa], PAGESIZE);
	return NULL;
}

uint32_t amf_info_dump(lower_info*li, FILE *fp){
	uint64_t temp_NOP=_NOP;
	fwrite(&temp_NOP,sizeof(uint64_t), 1, fp);

#ifdef LOWER_MEM_DEV
	for(uint32_t i=0; i<_NOP; i++){
		fwrite(mem_pool[i], 1, PAGESIZE, fp);
	}
#endif

	fwrite(li->req_type_cnt, sizeof(uint64_t), LREQ_TYPE_NUM, fp);
	return 1;
}

uint32_t amf_info_load(lower_info *li, FILE *fp){
	__amf_info_create_body();
	uint64_t now_NOP;
	fread(&now_NOP, sizeof(uint64_t), 1, fp);
	if(now_NOP!=_NOP){
		EPRINT("device setting is differ", true);
	}

#ifdef LOWER_MEM_DEV
	for(uint32_t i=0; i<_NOP; i++){
		mem_pool[i]=(char*)malloc(PAGESIZE);
		fread(mem_pool[i], 1, PAGESIZE, fp);
	}
#endif

	fread(li->req_type_cnt, sizeof(uint64_t), LREQ_TYPE_NUM, fp);
	return 1;
}

void amf_error_call_back_r(void *_req){
	algo_req *req=(algo_req*)_req;

	printf("error! in AMF read ppa:%u\n",req->test_ppa);

	req->end_req(req);
}
void amf_error_call_back_w(void *_req){
	algo_req *req=(algo_req*)_req;

	printf("error! in AMF write ppa:%u\n",req->test_ppa);

	req->end_req(req);
}
void amf_error_call_back_e(void *_req){
	dummy_req *req=(dummy_req*)_req;

	printf("error! in AMF erase ppa:%u\n",req->test_ppa);
	//req->end_req(req);
}
