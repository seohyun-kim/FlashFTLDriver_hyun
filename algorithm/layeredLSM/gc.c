#include "./gc.h"
#include "./piece_ppa.h"
#include "../../include/data_struct/list.h"
#include "./shortcut.h"
#include "./page_aligner.h"
extern uint32_t test_piece_ppa;
extern sc_master *shortcut;
extern lower_info *g_li;
extern bool debug_flag;
static void *__gc_end_req(algo_req *req){
	gc_value *g_value=(gc_value*)req->param;
	switch(req->type){
		case GCDR:
		case GCMR:
			fdriver_unlock(&g_value->lock);
			break;
		case GCDW:
		case GCMW:
			inf_free_valueset(g_value->value, FS_MALLOC_R);
			free(g_value);
			break;
	}
	free(req);
	return NULL;
}

static algo_req *__get_algo_req(gc_value *g_value, uint32_t type){
	algo_req *res=(algo_req*)malloc(sizeof(algo_req));
	res->type=type;
	res->ppa=g_value->ppa;
	res->value=g_value->value;
	res->end_req=__gc_end_req;
	res->param=(void*)g_value;
	return res;
}

static gc_value* __gc_issue_read(uint32_t ppa, blockmanager *sm, uint32_t type){
	gc_value *g_value=(gc_value*)malloc(sizeof(gc_value));
	g_value->ppa=ppa;
	g_value->value=inf_get_valueset(NULL, FS_MALLOC_R, PAGESIZE);
	fdriver_mutex_init(&g_value->lock);
	fdriver_lock(&g_value->lock);
	algo_req *res=__get_algo_req(g_value, type);
	memcpy(g_value->oob, sm->get_oob(sm, res->ppa), sizeof(uint32_t) * L2PGAP);
	g_li->read(res->ppa, PAGESIZE, res->value, res);
	return g_value;
}

static inline void __gc_issue_write(gc_value *g_value, blockmanager *sm, uint32_t type){
	algo_req *res=__get_algo_req(g_value, type);
	sm->set_oob(sm, (char*)g_value->oob, sizeof(uint32_t) *L2PGAP, res->ppa);
	g_li->write(res->ppa, PAGESIZE, res->value, res);
}

static inline void __gc_read_check(gc_value* g_value){
	fdriver_lock(&g_value->lock);
	fdriver_destroy(&g_value->lock);
}

static inline void __clear_block_and_gsegment(L2P_bm *bm, __gsegment *target){
	__block *blk;
	uint32_t bidx;
	for_each_block(target, blk, bidx){
		L2PBm_clear_block_info(bm, blk->block_idx);
	}
	blockmanager *sm=bm->segment_manager;
	sm->trim_segment(sm, target);
}

void gc_summary_segment(L2P_bm *bm, __gsegment *target){
	blockmanager *sm=bm->segment_manager;
	list *temp_list=list_init();
	uint32_t page, bidx, pidx;
	gc_value *g_value;
	for_each_page_in_seg(target,page,bidx,pidx){
		if(sm->is_invalid_piece(sm, page * L2PGAP)) continue;
		g_value=__gc_issue_read(page, sm, GCMR);
		list_insert(temp_list, (void*)g_value);
	}

	li_node *now,*nxt;
	while(temp_list->size){
		for_each_list_node_safe(temp_list,now,nxt){
			g_value=(gc_value*)now->data;
			__gc_read_check(g_value);
			uint32_t sid=g_value->oob[0];
			uint32_t intra_idx=g_value->oob[1];
			sid_info* info=sorted_array_master_get_info(sid);
			if(info->sa->sp_meta[intra_idx].ppa!=g_value->ppa){
				EPRINT("mapping error", true);
			}

			if(invalidate_ppa(sm, g_value->ppa, true) ==BIT_ERROR){
				EPRINT("invalidate map error", true);
			}
			uint32_t rppa=L2PBm_get_map_rppa(bm);
			if(validate_ppa(sm, rppa, true)==BIT_ERROR){
				EPRINT("validate map error", true);
			}
			info->sa->sp_meta[intra_idx].ppa=rppa;
			g_value->ppa=rppa;

			__gc_issue_write(g_value, sm, GCMW);
			list_delete_node(temp_list,now);
		}
	}

	__clear_block_and_gsegment(bm, target);
	bm->now_summary_seg=bm->reserve_summary_seg;
	sm->change_reserve_to_active(sm, bm->reserve_summary_seg);
	bm->reserve_summary_seg=sm->get_segment(sm, BLOCK_RESERVE);
	L2PBm_set_seg_type(bm, bm->reserve_summary_seg->seg_idx, SUMMARY_SEG);
	list_free(temp_list);
}

typedef struct gc_block{
	block_info *b_info;
	__block *blk;
}gc_block;

static inline void copy_normal_block(L2P_bm *bm, list *blk_list){
	if(blk_list->size==0) return;
	//GDB_MAKE_BREAKPOINT;
	li_node *now, *nxt, *p_now, *p_nxt;
	blockmanager *sm=bm->segment_manager;
	list *temp_list=list_init();
	gc_value *g_value;
	for_each_list_node_safe(blk_list, now, nxt){
		gc_block *g_blk=(gc_block*)now->data;
		uint32_t bidx=g_blk->blk->block_idx;
		sid_info* info=sorted_array_master_get_info(g_blk->b_info->sid);
		uint32_t invalidte_piece_num=0, validate_piece_num=0;

		/*read all page*/
		for(uint32_t i=0; i<_PPB; i++){
			uint32_t page=bidx*_PPB+i;
			bool read_flag=false;
			for(uint32_t j=0; j<L2PGAP; j++){
				if(sm->is_invalid_piece(sm, page*L2PGAP+j)){
					invalidte_piece_num++;
					continue;
				}
				validate_piece_num++;
				read_flag=true;
				break;
			}

			if(read_flag){
				g_value=__gc_issue_read(page, sm, GCDR);
				list_insert(temp_list, (void*)g_value);
			}
		}

		uint32_t new_pba=L2PBm_pick_empty_RPBA(bm);
		uint32_t prev_ppa=UINT32_MAX;
		bool valid_data_ptr[L2PGAP];
		uint32_t idx=g_blk->b_info->intra_idx;
		L2PBm_make_map(bm, new_pba, info->sid, idx);
		/*write all page to new address*/
		while(temp_list->size){
			for_each_list_node_safe(temp_list, p_now, p_nxt){
				g_value=(gc_value*)p_now->data;
				__gc_read_check(g_value);

				for(uint32_t j=0; j<L2PGAP; j++){
					if(sm->is_invalid_piece(sm, g_value->ppa*L2PGAP+j)){
						valid_data_ptr[j]=false;
						continue;
					}
					valid_data_ptr[j]=true;
					if(invalidate_piece_ppa(sm, g_value->ppa*L2PGAP+j, true)==BIT_ERROR){
						EPRINT("bit error in normal block copy", true);
					}
				}

				uint32_t start_rppa=new_pba+(g_value->ppa%_PPB);
				if(prev_ppa==UINT32_MAX || prev_ppa<start_rppa){
					prev_ppa=start_rppa;
				}
				else{
					EPRINT("start_rppa should be increase", true);
				}
				g_value->ppa=start_rppa;
	

				for(uint32_t j=0; j<L2PGAP; j++){
					if(!valid_data_ptr[j]){
						continue;
					}
					if(validate_piece_ppa(sm, g_value->ppa*L2PGAP+j, true)==BIT_ERROR){
						EPRINT("bit error in normal block copy", true);
					}
				}

				__gc_issue_write(g_value, sm, GCDW);
				list_delete_node(temp_list, p_now);
			}
		}
		
		/*update block mapping*/
		if(info->sa->pba_array[idx].PBA!=bidx*_PPB){
			EPRINT("inaccurate block!", true);
		}
		info->sa->pba_array[idx].PBA=new_pba;

		list_delete_node(blk_list, now);
	}

	list_free(temp_list);
}

static inline void __gc_issue_write_temp(value_set *value, uint32_t ppa,
		char *oob, blockmanager *sm){
	gc_value *temp_value=(gc_value*)malloc(sizeof(gc_value));
	temp_value->value=value;
	temp_value->ppa=ppa;
	memcpy(temp_value->oob, oob, sizeof(uint32_t) *L2PGAP);
	__gc_issue_write(temp_value, sm, GCDW);
}

typedef struct pinned_info{
	run *r;
	uint32_t intra_offset;
	uint32_t old_psa;
}pinned_info;

static inline void __update_pinning(uint32_t *lba_set, pinned_info *pset, uint32_t ppa,
		uint32_t num, blockmanager *bm){
	for(uint32_t i=0; i<num; i++){
		run *r=pset[i].r;
		if(validate_piece_ppa(bm, ppa*L2PGAP+i, true)==BIT_ERROR){
			EPRINT("validate piece ppa error in frag blk copy", true);
		}
		st_array_update_pinned_info(r->st_body, pset[i].intra_offset, ppa*L2PGAP+i, pset[i].old_psa);
	}
}

static inline void copy_frag_block(L2P_bm *bm, list *frag_list){
	if(frag_list->size==0) return;
	li_node *now, *nxt, *p_now, *p_nxt;
	blockmanager *sm=bm->segment_manager;
	list *temp_list=list_init();
	gc_value *g_value;
	pp_buffer *buffer=NULL;
	uint32_t rppa=UINT32_MAX;
	pinned_info pset[L2PGAP];
	for_each_list_node_safe(frag_list, now, nxt){
		gc_block *g_blk=(gc_block*)now->data;
		uint32_t bidx=g_blk->blk->block_idx;

		/*read all page*/
		for(uint32_t i=0; i<_PPB; i++){
			uint32_t page=bidx*_PPB+i;
			bool read_flag=false;
			for(uint32_t j=0; j<L2PGAP; j++){
				if(sm->is_invalid_piece(sm, page*L2PGAP+j)){
					continue;
				}
				read_flag=true;
				break;
			}

			if(read_flag){
				g_value=__gc_issue_read(page, sm, GCDR);
				list_insert(temp_list, (void*)g_value);
			}
		}

		while(temp_list->size){
			for_each_list_node_safe(temp_list, p_now, p_nxt){
				g_value=(gc_value*)p_now->data;
				if(buffer==NULL){
					buffer=pp_init();
				}
				__gc_read_check(g_value);

				for(uint32_t j=0; j<L2PGAP; j++){
					if(sm->is_invalid_piece(sm, g_value->ppa*L2PGAP+j)){
						continue;
					}
					uint32_t lba=g_value->oob[j];
					uint32_t psa=g_value->ppa*L2PGAP+j;
					uint32_t intra_offset;

					run *r=run_find_include_address(shortcut, lba, psa, &intra_offset);
					if(r==NULL){ //unlinked ppa;
						bool processed=false;
						block_info *binfo=g_blk->b_info;
						for(uint32_t k=0; k<MAX_RUN_NUM_FOR_FRAG; k++){
							if((binfo->frag_info & (1<<k))==0) continue;

							sid_info* temp_sid=sorted_array_master_get_info(k);
							if(temp_sid->r==NULL) continue;

							intra_offset=run_find_include_address_byself(temp_sid->r, lba, psa);
							if(intra_offset==NOT_FOUND) continue;

							st_array_unlink_bit_set(temp_sid->sa, intra_offset, psa);
							processed=true;
							break;
						}
						if(!processed){
							EPRINT("unlinking psa failed", true);
						}
						continue;
					}
					else{
						pset[buffer->buffered_num].r=r;
						pset[buffer->buffered_num].intra_offset=intra_offset;
						pset[buffer->buffered_num].old_psa=psa;
					}

					if(invalidate_piece_ppa(sm, psa,true)==BIT_ERROR){
						EPRINT("bit error in frag block copy", true);
					}

					if(pp_insert_value(buffer, lba, &g_value->value->value[LPAGESIZE*j])){
						if(rppa==UINT32_MAX){
							rppa=L2PBm_pick_empty_RPBA(bm);
						}
						value_set *value=pp_get_write_target(buffer, false);
						__gc_issue_write_temp(value,  rppa, (char*)buffer->LBA, sm);
						__update_pinning(buffer->LBA, pset, rppa, buffer->buffered_num, sm);
						
						pp_reinit_buffer(buffer);
						rppa++;
						if(rppa%_PPB==0){
							rppa=UINT32_MAX;
						}
					}
				}

				inf_free_valueset(g_value->value, FS_MALLOC_R);
				free(g_value);
				list_delete_node(temp_list, p_now);
			}
		}

		list_delete_node(frag_list, now);
	}
	list_free(temp_list);

	if(buffer && buffer->buffered_num){
		if(rppa==UINT32_MAX){
			rppa=L2PBm_pick_empty_RPBA(bm);
		}
		value_set *value=pp_get_write_target(buffer, true);
		__gc_issue_write_temp(value, rppa, (char*)buffer->LBA, sm);
		__update_pinning(buffer->LBA, pset, rppa, buffer->buffered_num, sm);
	}
	if(buffer){
		pp_free(buffer);
	}
}

static inline bool __normal_block_valid_check(blockmanager *sm, uint32_t block_idx){
	uint32_t invalidate_cnt=0;
	uint32_t validate_cnt=0;
	bool res=0;
	for(uint32_t i=0; i<_PPB; i++){
		uint32_t page=block_idx*_PPB+i;
		for(uint32_t j=0; j<L2PGAP; j++){
			if(sm->is_invalid_piece(sm, page*L2PGAP+j)){
				invalidate_cnt++;
			}
			else{
				validate_cnt++;
				break;
			}
		}
	}
	return invalidate_cnt!=L2PGAP *_PPB;
}

void gc_data_segment(L2P_bm *bm, __gsegment *target){
	DEBUG_CNT_PRINT(cnt_gc_data, UINT32_MAX, __FUNCTION__, __LINE__);
	blockmanager *sm=bm->segment_manager;
	list *normal_list=list_init();
	list *frag_list=list_init();
	__block *blk;
	uint32_t bidx;
	for_each_block(target, blk, bidx){
		gc_block *temp=(gc_block*)malloc(sizeof(gc_block));
		temp->b_info=&bm->PBA_map[blk->block_idx];
		temp->blk=blk;

		switch(bm->PBA_map[blk->block_idx].type){
			case LSM_BLOCK_NORMAL:
				if(__normal_block_valid_check(sm, blk->block_idx)){
					list_insert(normal_list, (void*)temp);
				}
				break;
			case LSM_BLOCK_FRAGMENT:
				list_insert(frag_list, (void*)temp);
				break;
			default:
				EPRINT("EMPTY BLOCK!", true);
				break;
		}
	}

	copy_normal_block(bm, normal_list);
	copy_frag_block(bm, frag_list);

	__clear_block_and_gsegment(bm, target);

	list_free(normal_list);
	list_free(frag_list);

	bm->now_seg_idx=bm->reserve_seg->seg_idx;
	bm->now_block_idx=bm->reserve_block_idx;
	sm->change_reserve_to_active(sm, bm->reserve_seg);
	bm->reserve_block_idx=0;
	bm->reserve_seg=sm->get_segment(sm, BLOCK_RESERVE);
	L2PBm_set_seg_type(bm, bm->reserve_seg->seg_idx, DATA_SEG);
}

bool gc(L2P_bm *bm, uint32_t type){
	blockmanager *sm=bm->segment_manager;
	__gsegment *target=sm->get_gc_target(sm);
	bool full_invalid=target->invalidate_piece_num==target->validate_piece_num;
	if(full_invalid){
		__clear_block_and_gsegment(bm, target);
		return false;
	}
	
	std::queue<uint32_t> temp_seg_q;
	while(bm->seg_type[target->seg_idx]!=type){
		temp_seg_q.push(target->seg_idx);
		target=sm->get_gc_target(sm);
		if(target==NULL){
			EPRINT("dev full", true);
		}
	}

	switch(type){
		case SUMMARY_SEG:
			gc_summary_segment(bm, target);
			break;
		case DATA_SEG:
			gc_data_segment(bm, target);
			break;
	}

	while(temp_seg_q.size()){
		uint32_t seg_idx=temp_seg_q.front();
		sm->insert_gc_target(sm, seg_idx);
		temp_seg_q.pop();
	}
	return true;
}
