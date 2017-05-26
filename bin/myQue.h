#ifndef __MYQUE_H__
#define __MYQUE_H__
#include <pthread.h>
typedef struct qnode{
	pthread_t data;
	struct node nxt;
}qnode; 
bool qempty(qnode head); 	//shi fou wei kong
void qpop(qnode &head);  	//tan chu shou ge qnode
void qpush_back(qnode head,pthread_t n);	//dui wei cha ru
pthread_t qfront(qnode head);		//shou ge qnode
void qclean(qnode head);	//qing kong
void claen(qnode head);         //digui shifang jiedian
int qsize(qnode head);   	//return que size
#endif //__MYQUE_H__
