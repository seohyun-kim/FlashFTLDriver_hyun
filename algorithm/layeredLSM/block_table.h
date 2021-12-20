#ifndef BLOCK_TABLE_H
#define BLOCK_TABLE_H
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <queue>

#include "../../include/data_struct/bitmap.h"
#include "../../include/container.h"
#include "../../include/settings.h"
#include "../../include/debug_utils.h"

enum{
	LSM_BLOCK_NORMAL, LSM_BLOCK_FRAGMENT
};

typedef struct block_info{
	uint32_t sid;
	uint32_t intra_idx;
	uint32_t type;
}block_info;

typedef struct L2P_block_manager{
	block_info *PBA_map;				//run_chunk_id to physical block address 

	uint32_t total_seg_num;			//max number of segemnt in Flash
	uint32_t *seg_trimed_block_num;	//the number of trimed block in a segment
	bitmap **seg_block_bit;			//the bitmap for checking trimed block in a segment

	uint32_t now_segment_idx;		//if this variable is set to UINT32_MAX, 
									//it needs to be assigend a new segment

	__segment* reserve_seg;
	__segment* reserve_summary_seg;
	__segment* now_summary_seg;
	uint32_t reserve_block_idx;
	uint32_t now_block_idx;
	uint32_t now_seg_idx;
	blockmanager *segment_manager; 
}L2P_bm;


/*
	Function: L2PBm_init
	--------------------
		Returns a new L2PBm for managing block mapping
	
	sm: segment manager for get empty segment
 */
L2P_bm *L2PBm_init(blockmanager *sm);

/*
	Function: L2PBm_free
	--------------------
		Free allocated memory
	
	bm: target L2PBm
 */
void L2PBm_free(L2P_bm *bm);

/*
	Function: L2PBm_invalidate_PBA
	--------------------
		invalidating PBA
	
	bm: L2PBm
	PBA: target PBA to be invalid
 */
void L2PBm_invalidate_PBA(L2P_bm *bm, uint32_t PBa);

/*
	Function: L2PBm_pick_empty_PBA
	--------------------
		returns empty physical block address
	
	bm: L2PBm
 */
uint32_t L2PBm_pick_empty_PBA(L2P_bm *bm);

/*
	Function: L2PBm_pick_empty_RPBA
	--------------------
		returns empty reserve physical block address
	
	bm: L2PBg
 */
uint32_t L2PBm_pick_empty_RPBA(L2P_bm *bm);

/*
	Function: L2PBm_make_map
	------------------------
		make mapping inforamtion between PBA -> sid
	
	bm:
	PBA: target PBA
	sid: id of st_array which has PBA
	intra_idx: the index of PBA in PBA list
 */
void L2PBm_make_map(L2P_bm *bm, uint32_t PBA, uint32_t sid, 
		uint32_t intra_idx);

/* 
 * Function: L2PBm_block_fragment
 * ------------------------------
 *		change lsm block type to LSM_BLOCK_FRAGMENT
 *	bm:
 *	PBA:
 * */
void L2PBm_block_fragment(L2P_bm *bm, uint32_t PBA);

/*
 * Function:L2PBm_get_map_ppa
 * -------------------------
 *		return ppa for summary page
 *
 *	sid: managed sorted_array
 * */
uint32_t L2PBm_get_map_ppa(L2P_bm *bm);

/*
 * Function:L2PBm_get_map_ppa
 * -------------------------
 *		return ppa for summary page
 *
 *	sid: managed sorted_array
 * */
uint32_t L2PBm_get_map_rppa(L2P_bm *bm);
#endif