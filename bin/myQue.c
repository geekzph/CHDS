#include <stdio.h>
#include "myQue.h"

bool qempty(qnode head){
	if(head==NULL) return true;
	return false;
}

void qpop(qnode &head){
	qnode tmp = head;
	head=head->nxt;
	free(tmp);
}
void qpush_back(qnode head,pthread_t n){
	if(head==NULL){
		head.data=n;
		head->nxt=NULL;
		qnode_n=1;
		return;
	}
	qnode newqnode;
	newqnode->nxt=NULL;
	newqnode->data=n;
	while(head->nxt!=NULL){
		head=head->nxt;
	}
	head->nxt=newqnode;
}
pthread_t qfront(qnode head){
	return head.data;
}
void clean(qnode head){
	if(head->nxt!=NULL){
		clean(head->nxt);
	}
	free(head);
}
void qclean(qnode head){
	if(head->nxt!=NULL)
		clean(head->nxt);
	head=NULL;
}
int qsize(qnode head){
	if(head==NULL) return 0;
	int n=1;
	while(head->nxt!=NULL){
		++n;
		head=head->nxt;
	}
	return n;
}
