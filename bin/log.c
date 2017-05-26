#include "log.h"
#include <stdio.h>
#include <stdlib.h>
void writeLog(const char *logFilePath,const char *content){
	FILE *fp = NULL; 
	fp = fopen(logFilePath,"a");
	if(fp==NULL){
		printf("\t\t----------------->log.c\n\t\tlogFilePath = [%s]\n",logFilePath);
		perror("fopen");
		abort();
	}
	fprintf(fp,"%s\n",content);
	fclose(fp);
}
