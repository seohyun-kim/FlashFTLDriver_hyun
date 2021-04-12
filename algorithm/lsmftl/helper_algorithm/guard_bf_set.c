#include "guard_bf_set.h"
#include <stdlib.h>
#include <stdio.h>

static uint32_t BP_sub_member_num=UINT32_MAX;
static uint32_t BO_sub_member_num=UINT32_MAX;
extern uint32_t debug_lba;
static inline void find_sub_member_num(float target_fpr, uint32_t member, uint32_t type){
	uint32_t min_bit=UINT32_MAX;
	uint32_t target_number;
	for(uint32_t i=(1+1); i<member; i++){
		uint32_t member_set_num=member/i + (member%i?1:0);
		uint32_t total_bit=get_number_of_bits(get_target_each_fpr(target_fpr, i)) * i + member_set_num*(sizeof(uint32_t) * 2);
		if(min_bit>total_bit){
			min_bit=total_bit;
			target_number=i;
		}
	}
	switch(type){
		case BLOOM_PTR_PAIR:
			BP_sub_member_num=target_number;
			break;
		case BLOOM_ONLY:
			BO_sub_member_num=target_number;
			break;
		default:
			EPRINT("no type of gbf_set", true);
			break;
	}
}

void gbf_set_prepare(float target_fpr, uint32_t member, uint32_t type){
	if(type==BLOOM_PTR_PAIR){
		if(BP_sub_member_num==UINT32_MAX){
			find_sub_member_num(target_fpr, member,type);
		}
	}
	if(type==BLOOM_ONLY){
		if(BO_sub_member_num==UINT32_MAX){
			find_sub_member_num(target_fpr, member,type);
		}
	}
}

guard_bf_set *gbf_set_init(float target_fpr, uint32_t member, uint32_t type){
	guard_bf_set *res=(guard_bf_set*)malloc(sizeof(guard_bf_set));
	res->now=res->max=0;
	res->max=member;
	res->memory_usage_bit=0;
	res->type=type;
	uint32_t set_num;
	uint32_t i=0;
	switch(type){
		case BLOOM_PTR_PAIR:
			if(BP_sub_member_num==UINT32_MAX){
				find_sub_member_num(target_fpr, member,type);
			}
			set_num=member/BP_sub_member_num+(member%BP_sub_member_num?1:0);
			res->body=(guard_bp_pair*)malloc(sizeof(guard_bp_pair)*set_num);
			res->set_num=set_num;
			for(; i<set_num; i++){
				res->body[i].start_lba=UINT32_MAX;
				res->body[i].end_lba=0;
				res->body[i].array=(void*)bf_set_init(target_fpr, BP_sub_member_num, type);
			}
			break;
		case BLOOM_ONLY:
			if(BO_sub_member_num==UINT32_MAX){
				find_sub_member_num(target_fpr, member,type);
			}
			set_num=member/BO_sub_member_num+(member%BO_sub_member_num?1:0);
			res->body=(guard_bp_pair*)malloc(sizeof(guard_bp_pair)*set_num);
			res->set_num=set_num;
			for(; i<set_num; i++){
				res->body[i].start_lba=UINT32_MAX;
				res->body[i].end_lba=0;
				res->body[i].array=(void*)bf_set_init(target_fpr, BO_sub_member_num, type);
			}
			break;
		default:
			EPRINT("no type of gbf_set", true);
			break;
	}

	return res;
}

bool gbf_set_insert(guard_bf_set *gbf, uint32_t lba, uint32_t piece_ppa){
	if(gbf->now>=gbf->max){
		EPRINT("over flow", true);
		return false;
	}
	
	uint32_t type=((bf_set*)gbf->body[0].array)->type;
	uint32_t idx;
	switch(type){
		case BLOOM_PTR_PAIR:
			idx=gbf->now/BP_sub_member_num;
			if(lba==debug_lba){
				printf("guard idx:%u\n",idx);
				if(idx==11){
					EPRINT("break", false);
				}
			}
			bf_set_insert((bf_set*)gbf->body[idx].array, lba, piece_ppa);
			break;
		case BLOOM_ONLY:
			idx=gbf->now/BO_sub_member_num;
			if(lba==debug_lba){
				printf("guard idx:%u\n",idx);
			}
			bf_set_insert((bf_set*)gbf->body[idx].array, lba, piece_ppa);
			break;
	}
	if(gbf->body[idx].start_lba>lba){ gbf->body[idx].start_lba=lba;}
	if(gbf->body[idx].end_lba<lba){ gbf->body[idx].end_lba=lba;}
	gbf->now++;
	return true;
}

static inline uint32_t find_guard_bf_set(guard_bf_set *gbf, uint32_t type, uint32_t lba){
	uint32_t s,e,mid;
	s=0; 
	switch(type){
		case BLOOM_PTR_PAIR:
			e=gbf->now/BP_sub_member_num+(gbf->now%BP_sub_member_num?1:0);
			break;
		case BLOOM_ONLY:
			e=gbf->now/BO_sub_member_num+(gbf->now%BO_sub_member_num?1:0);
			break;
	}

	while(s<=e){
		mid=(s+e)/2;
		if(gbf->body[mid].start_lba<=lba && gbf->body[mid].end_lba>=lba){
			return mid;
		}
		if(gbf->body[mid].start_lba > lba){
			e=mid-1;
		}
		else if(gbf->body[mid].end_lba<lba){
			s=mid+1;
		}
	}
	return UINT32_MAX;
}

uint32_t gbf_set_get_piece_ppa(guard_bf_set *gbf, uint32_t *last_idx, uint32_t lba){
	uint32_t type=((bf_set*)gbf->body[0].array)->type;
	uint32_t idx=find_guard_bf_set(gbf, type, lba);
	if(idx==UINT32_MAX) return UINT32_MAX;

	if(type==BLOOM_ONLY){
		return bf_set_get_piece_ppa((bf_set*)gbf->body[idx].array, last_idx, lba)+idx*BO_sub_member_num;
	}
	else return bf_set_get_piece_ppa((bf_set*)gbf->body[idx].array, last_idx, lba);
}


uint32_t gbf_get_start_idx(guard_bf_set *gbf, uint32_t lba){
	uint32_t type=((bf_set*)gbf->body[0].array)->type;
	uint32_t idx=find_guard_bf_set(gbf, type, lba);
	if(idx==UINT32_MAX) return UINT32_MAX;
	return ((bf_set*)gbf->body[idx].array)->now-1;
}

guard_bf_set* gbf_set_copy(guard_bf_set *src){
	guard_bf_set *res=(guard_bf_set*)malloc(sizeof(guard_bf_set));
	guard_bp_pair *temp_body=src->body;
	*res=*src;
	uint32_t i=0;
	temp_body=(guard_bp_pair*)malloc(sizeof(guard_bp_pair)*res->set_num);
	for(;i<res->set_num; i++){
		temp_body[i]=src->body[i];
		temp_body[i].array=(void*)bf_set_copy((bf_set*)src->body[i].array);
	}
	res->body=temp_body;
	return res;
}	

void gbf_set_move(guard_bf_set *des, guard_bf_set *src){
	*des=*src;
	src->body=NULL;
}

void gbf_set_free(guard_bf_set* gbf){
	for(uint32_t i=0; i<gbf->set_num; i++){
		bf_set_free((bf_set*)gbf->body[i].array);
	}
	free(gbf->body);
	free(gbf);
}

