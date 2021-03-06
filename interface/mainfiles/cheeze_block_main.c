#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <signal.h>
#include "../../include/FS.h"
#include "../../include/settings.h"
#include "../../include/types.h"
#include "../../bench/bench.h"
#include "../interface.h"
#include "../vectored_interface.h"
#include "../cheeze_hg_block.h"

void log_print(int sig){
	request_print_log();
	request_memset_print_log();
	free_cheeze();
	printf("f cheeze\n");
	inf_free();
	printf("f inf\n");
	fflush(stdout);
	fflush(stderr);
	sync();
	printf("before exit\n");
	exit(1);
}

void * thread_test(void *){
	vec_request **req_arr=NULL;
	while((req_arr=get_vectored_request_arr())){
		for(int i=0; req_arr[i]!=NULL; i++){
			assign_vectored_req(req_arr[i]);
		}
		free(req_arr);
	}
	return NULL;
}

void log_lower_print(int sig){
    printf("-------------lower print!!!!-------------\n");
    inf_lower_log_print();
    printf("-------------lower print end-------------\n");
}

//int MS_TIME_SL;
void print_temp_log(int sig){
	request_print_log();
	request_memset_print_log();
	inf_print_log();
}

pthread_t thr; 
int main(int argc,char* argv[]){
	struct sigaction sa;
	setbuf(stdout, NULL);
	setbuf(stderr, NULL);
	sa.sa_handler = log_print;
	sigaction(SIGINT, &sa, NULL);

	struct sigaction sa2;
	sa2.sa_handler = print_temp_log;
	sigaction(SIGUSR1, &sa2, NULL);

	printf("signal add!\n");

//	MS_TIME_SL = atoi(getenv("MS_TIME_SL"));
//	printf("Using MS_TIME_SL of %d\n", MS_TIME_SL);


	inf_init(1,0, argc, argv);
	init_cheeze(0);
/*
	if(argc<2){
		init_cheeze(0);
	}
	else{
		init_cheeze(atoll(argv[1]));
	}
*/
	pthread_create(&thr, NULL, thread_test, NULL);
	pthread_join(thr, NULL);

	free_cheeze();
	inf_free();
	return 0;
}
