#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "normal.h"
#include "hyun_gc.h"
#include "../../include/data_struct/list.h"

// 1. Invalidate page 많은 block(segment)선택 : [get_gc_target]
// 2. for_each_page_in_seg 돌면서 segment 내 valid data의 page 주소 찾음
// 3. valid data를 BLOCK_RESERVE 공간에 memcpy
// 4. 원래 segment 전체 ERASE : [trim_segment(bm,target)]
// 5. ERASE 된 공간을 다음 GC에 사용할 BLOCK_RESERVE로 넘겨줌
// 6. mapping table update


gc_value* send_req(uint32_t ppa, uint8_t type, value_set* value, algorithm* __normal) {
	algo_req* my_req = (algo_req*)malloc(sizeof(algo_req));
	my_req->parents = NULL;
	my_req->end_req = page_gc_end_req;//call back function for GC
	my_req->type = type;

	/*for gc, you should assign free space for reading valid data*/
	gc_value* res = NULL;
	switch (type) {
	case GCDR:
		res = (gc_value*)malloc(sizeof(gc_value));
		res->isdone = false;
		res->ppa = ppa;
		my_req->param = (void*)res;
		my_req->type_lower = 0;
		/*when read a value, you can assign free value by this function*/
		res->value = inf_get_valueset(NULL, FS_MALLOC_R, PAGESIZE);
		__normal->li->read(ppa, PAGESIZE, res->value, my_req);
		break;
	case GCDW:
		res = (gc_value*)malloc(sizeof(gc_value));
		res->value = value;
		my_req->param = (void*)res;
		__normal->li->write(ppa, PAGESIZE, res->value, my_req);
		break;
	}
	return res;
}


void travel_page_in_segment(algorithm* __normal, __gsegment* _target_segment, __segment* __reserve_segment) {

	uint32_t page, bidx, pidx;
	gc_value* gv;
	align_gc_buffer g_buffer;
	blockmanager* bm = __normal->bm;
	list* temp_list = list_init();

	for_each_page_in_seg(_target_segment, page, bidx, pidx) {
		// check the page is valid or not
		bool should_read = false;
		for (uint32_t i = 0; i < L2PGAP; i++) {
			if (__normal->bm ->is_invalid_piece(__normal->bm, page * L2PGAP + i)) continue;
			else {
				should_read = true;
				break;
			}
		}
		if (should_read) { // valid piece
			gv = send_req(page, GCDR, NULL, __normal);
			list_insert(temp_list, (void*)gv);// temp_list에 append
		}
	}

	// ===========================================================

	li_node* now, * nxt;
	g_buffer.idx = 0;
	KEYT* lbas;
	while (temp_list->size) {
		for_each_list_node_safe(temp_list, now, nxt) {

			gv = (gc_value*)now->data;
			if (!gv->isdone) continue;
			lbas = (KEYT*)bm->get_oob(bm, gv->ppa);
			for (uint32_t i = 0; i < L2PGAP; i++) {
				if (bm->is_invalid_piece(bm, gv->ppa * L2PGAP + i)) continue;
				memcpy(&g_buffer.value[g_buffer.idx * LPAGESIZE], &gv->value->value[i * LPAGESIZE], LPAGESIZE);
				g_buffer.key[g_buffer.idx] = lbas[i];

				g_buffer.idx++;

				if (g_buffer.idx == L2PGAP) {
					uint32_t res = page_map_gc_update(g_buffer.key, L2PGAP);
					validate_ppa(res, g_buffer.key, g_buffer.idx);
					send_req(res, GCDW, inf_get_valueset(g_buffer.value, FS_MALLOC_W, PAGESIZE));
					g_buffer.idx = 0;
				}
			}

			inf_free_valueset(gv->value, FS_MALLOC_R);
			free(gv);
			//you can get lba from OOB(spare area) in physicall page
			list_delete_node(temp_list, now);
		}
	}

	if (g_buffer.idx != 0) {
		uint32_t res = page_map_gc_update(g_buffer.key, g_buffer.idx);
		validate_ppa(res, g_buffer.key, g_buffer.idx);
		send_req(res, GCDW, inf_get_valueset(g_buffer.value, FS_MALLOC_W, PAGESIZE));
		g_buffer.idx = 0;
	}

	bm->trim_segment(bm, target); //erase a block
	p->active = p->reserve;//make reserved to active block
	bm->change_reserve_to_active(bm, p->reserve);
	p->reserve = bm->get_segment(bm, BLOCK_RESERVE); //get new reserve block from block_manager

	list_free(temp_list);

}




void run_hyun_gc(algorithm* __normal) {

	pm_body* p = (pm_body*)__normal->algo_body;


	// get target segment for gc
	__gsegment* target_segment = __normal->bm->get_gc_target(__normal->bm);

	// get reserve segment for gc
	__segment* reserve_segment = __normal->bm->get_segment(__normal->bm, BLOCK_RESERVE);

	// processing  per page in segment
	// (find valid page -> copy value -> update map table)
	travel_page_in_segment(__normal, target_segment, reserve_segment);
	
	// ERADSE target segment
	//__normal->bm->trim_segment(__normal->bm, target_segment);

	// change [ RESERVE BLOCK -> ACTIVE BLOCK ]
	//__normal->bm->change_reserve_to_active(__normal->bm, reserve_segment);

	// change [ gc target -> RESERVE BLOCK ]


	return;
}

void* page_gc_end_req(algo_req* input) {
	gc_value* gv = (gc_value*)input->param;
	switch (input->type) {
	case GCDR:
		gv->isdone = true;
		break;
	case GCDW:
		/*free value which is assigned by inf_get_valueset*/
		inf_free_valueset(gv->value, FS_MALLOC_R);
		free(gv);
		break;
	}
	free(input);
	return NULL;
}

