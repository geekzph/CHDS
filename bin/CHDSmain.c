#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include<netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/sendfile.h>
#include <time.h> 
#include <netdb.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>
#include <dirent.h>
#include "cfg.h"
#include "log.h"


int MAXFILEPATHSIZE,MAXEVENTS,FILEBUFSIZE,MAXLOGSIZE;
char *ROOTDIR;
const char *CFGPATH = "../etc/config";
/*
struct dirent中的几个成员：

d_type：4表示为目录，8表示为文件

d_reclen：16表示子目录或文件，24表示非子目录

d_name：目录或文件的名称

具体参考:/usr/include/sys/dirent.h (scounix505)
struct dirent {
	ino32_t d_ino;                 
	off_t   d_off;                 
	unsigned short  d_reclen;      
	char    d_name[1];             
};
*/

int findAndSend(char *dir, int depth ,char *fileName,struct stat st,struct epoll_event events)  
{  
    DIR *dp;  
    struct dirent *entry;  
    struct stat statbuf;  
  
    if ((dp = opendir(dir)) == NULL) {  
        fprintf(stderr, "Can`t open directory %s\n", dir);  
        return 0;  
    }  
      
    chdir(dir);  
    while ((entry = readdir(dp)) != NULL) {  
        lstat(entry->d_name, &statbuf);  
        if (S_ISDIR(statbuf.st_mode)) {  
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0 )    
                continue;     
            printf("%*s%s/\n", depth, "", entry->d_name);  
            if(findAndSend(entry->d_name, depth+4,fileName,st,events)) 
            	return 1;  
        } else  {
            printf("%*s%s\n", depth, "", entry->d_name);  
        	if(strcmp(entry->d_name,fileName)==0){
        		printf("find it\n"); 
        		int fp = open(entry->d_name,O_RDONLY);
        		stat(entry->d_name,&st);
            	if(sendfile(events.data.fd,fp, 0, (size_t)st.st_size) < 0){
            		perror("sendfile");
            		close(fp);
            		exit(1);
            	}
            	close(fp);
				printf("[file:%s transfer successful!]\n",entry->d_name);
        		return 1;
        	}    
        }
    }  
    chdir("..");  
    closedir(dp);  
    return 0;   
}  


//去除字符串所有空格
void rmSpace(char *str){

	int i=0;
	for(int j=0;*(str+j)!='\0';++j){
	
		if(*(str+j)!=' ') {
		
			*(str+i)=*(str+j);
			
			++i;
		}
	}
	*(str+i)='\0';
}


//函数:
//功能:创建和绑定一个TCP socket
//参数:端口
//返回值:创建的socket
static int create_and_bind (char *port) {
    struct addrinfo hints;
    struct addrinfo *result,*sInfo;
    int s, sfd;

    memset (&hints, 0, sizeof (struct addrinfo));
    hints.ai_family = AF_UNSPEC;     /* Return IPv4 and IPv6 choices */
    hints.ai_socktype = SOCK_STREAM; /* We want a TCP socket */
    hints.ai_flags = AI_PASSIVE;     /* All interfaces */

    s = getaddrinfo (NULL, port, &hints, &result);
    if (s != 0) {
        fprintf (stderr, "getaddrinfo: %s\n", gai_strerror (s));
        return -1;
    }

    for (sInfo = result; sInfo != NULL; sInfo = sInfo->ai_next) {
        sfd = socket (sInfo->ai_family, sInfo->ai_socktype, sInfo->ai_protocol);
        if (sfd == -1)
            continue;

        s = bind (sfd, sInfo->ai_addr, sInfo->ai_addrlen);
        if (s == 0) {
            /* We managed to bind successfully! */
            
            break;
        }

        close (sfd);
    }

    if (sInfo == NULL) {
        fprintf (stderr, "Could not bind\n");
        return -1;
    }

    freeaddrinfo (result);

    return sfd;
}


//函数
//功能:设置socket为非阻塞的
static int make_socket_non_blocking (int sfd) {
    int flags, s;

    //得到文件状态标志
    flags = fcntl (sfd, F_GETFL, 0);
    if (flags == -1) {
        perror ("fcntl");
        return -1;
    }

    //设置文件状态标志
    flags |= O_NONBLOCK;
    s = fcntl (sfd, F_SETFL, flags);
    if (s == -1) {
        perror ("fcntl");
        return -1;
    }

    return 0;
}

//while (1) {\n        r
//响应事件
void responseEvent(int efd,int sfd,struct epoll_event event,struct stat st){
	char *logFilePath = (char *)malloc(MAXFILEPATHSIZE);
	char *content = (char *)malloc(MAXLOGSIZE);
	
	time_t now;  
	struct tm *tm_now;
 
	time(&now);
	tm_now = localtime(&now);
	

	char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
    if ((event.events & EPOLLERR) || (event.events & EPOLLHUP) || (!(event.events & EPOLLIN))) {
        /* An error has occured on this fd, or the socket is not
           ready for reading (why were we notified then?) */
        fprintf (stderr, "epoll error\n");
        close (event.data.fd);
        return ;
    }

    else if (sfd == event.data.fd) {
        /* We have a notification on the listening socket, which
           means one or more incoming connections. */
        
        while (1) {
            struct sockaddr in_addr;
            socklen_t in_len;
            int infd;
            

            in_len = sizeof in_addr;
            infd = accept (sfd, &in_addr, &in_len);
            if (infd == -1) {
                if ((errno == EAGAIN) ||
                        (errno == EWOULDBLOCK)) {
                    /* We have processed all incoming
                       connections. */
                    break;
                } else {
                    perror ("accept");
                    break;
                }
            }

            //将地址转化为主机名或者服务名
            int s = getnameinfo (&in_addr, in_len,
                             hbuf, sizeof hbuf,
                             sbuf, sizeof sbuf,
                             NI_NUMERICHOST | NI_NUMERICSERV);//flag参数:以数字名返回
            //主机地址和服务地址

            if (s == 0) {
                printf("Accepted connection on descriptor %d (host=%s, port=%s)\n", infd, hbuf, sbuf);
                
                /* 写入日志 */
                sprintf(logFilePath,"%s%04d-%02d-%02d","/var/www/CHDS/log/",tm_now->tm_year+1900, tm_now->tm_mon+1, tm_now->tm_mday);
                
				sprintf(content,"[%04d-%02d-%02d %02d:%02d:%02d] (host=%s\tport=%s)"
								" : Connection",tm_now->tm_year+1900, tm_now->tm_mon+1,
								 tm_now->tm_mday, tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec, hbuf, sbuf);
								 
				writeLog(logFilePath,content);
                
            }

            /* Make the incoming socket non-blocking and add it to the
               list of fds to monitor. */
            s = make_socket_non_blocking (infd);
            if (s == -1)
                abort ();
			struct epoll_event event;
            event.data.fd = infd;
            event.events = EPOLLIN | EPOLLET;
            s = epoll_ctl (efd, EPOLL_CTL_ADD, infd, &event);
            if (s == -1) {
                perror ("epoll_ctl");
                abort ();
            }
        }
    } else {
        /* We have data on the fd waiting to be read. Read and
           display it. We must read whatever data is available
           completely, as we are running in edge-triggered mode
           and won't get a notification again for the same
           data. */
        int done = 0;

        while (1) {
            ssize_t count;
            char buf[512];

            count = read (event.data.fd, buf, sizeof(buf));
            if (count == -1) {
                /* If errno == EAGAIN, that means we have read all
                   data. So go back to the main loop. */
                if (errno != EAGAIN) {
                	
                    perror ("read");
                    done = 1;
                }
                break;
            } else if (count == 0) {
                /* End of file. The remote has closed the
                   connection. */
                done = 1;
                break;
            }

            						
           	buf[count-2]='\0';
           	printf("buf1 = [%s]\n",buf);
           	rmSpace(buf);
           	printf("buf2 = [%s]\n",buf);                   	
           	
           	
           	if(!strcmp(buf,"exit") || !strcmp(buf,"quit")){  //客户端退出
           		done=1;
           		sprintf(logFilePath,"%s%04d-%02d-%02d","/var/www/CHDS/log/",tm_now->tm_year+1900, tm_now->tm_mon+1, tm_now->tm_mday);\
           		
           		sprintf(content,"[%04d-%02d-%02d %02d:%02d:%02d] (host=%s\tport=%s)"
           						" : exit",tm_now->tm_year+1900, tm_now->tm_mon+1,
           						 tm_now->tm_mday, tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec,hbuf, sbuf);
           						 
				writeLog(logFilePath,content);
           	}else if(sizeof(buf)>3 && buf[0]=='G' && buf[1]=='E' && buf[2]=='T'){ //客户端请求文件
           		if(findAndSend(ROOTDIR,0,buf+3,st,event)){
           			sprintf(logFilePath,"%s%04d-%02d-%02d","/var/www/CHDS/log/",tm_now->tm_year+1900, tm_now->tm_mon+1, tm_now->tm_mday);
					printf("logFilePath=[%s]\n",logFilePath);
				
               		sprintf(content,"[%04d-%02d-%02d %02d:%02d:%02d] (host=%s\tport=%s)"
               				" : [GET]:%s transfer successful!",
               				tm_now->tm_year+1900, tm_now->tm_mon+1, tm_now->tm_mday, 
               				tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec,hbuf, sbuf,buf+3);
               		printf("content=[%s]\n",content);
					writeLog(logFilePath,content);
           		}else{
            		sprintf(logFilePath,"%s%04d-%02d-%02d",
            				"/var/www/CHDS/log/",tm_now->tm_year+1900, tm_now->tm_mon+1, tm_now->tm_mday);
           		
           			sprintf(content,
           					"[%04d-%02d-%02d %02d:%02d:%02d] (host=%s\tport=%s) :"
           					" Receive:[%s] Returned 405 Reason:No such file or directory",
           					tm_now->tm_year+1900, tm_now->tm_mon+1, tm_now->tm_mday, 
           					tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec,hbuf, sbuf,buf);
           		
					writeLog(logFilePath,content);
            		
            		strcpy(buf,"Usage:GET fileName\a\n");
            		send(event.data.fd,buf,strlen(buf),0);
           		}
            	
            }else{   //客户端发送错误请求
            	fprintf(stderr,"error:405\a\nUsage:GET fileName\a\n");
            	
            	sprintf(logFilePath,"%s%04d-%02d-%02d","/var/www/CHDS/log/",tm_now->tm_year+1900, tm_now->tm_mon+1, tm_now->tm_mday);
           		
       			sprintf(content,"[%04d-%02d-%02d %02d:%02d:%02d] (host=%s\tport=%s)"
       							" : Receive:[%s] Returned 405",
       							tm_now->tm_year+1900, tm_now->tm_mon+1, tm_now->tm_mday,
       							 tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec,hbuf, sbuf,buf);
       		
				writeLog(logFilePath,content);
            	strcpy(buf,"error:405\a\nUsage:GET fileName\a\n");
            	send(event.data.fd,buf,strlen(buf),0);
            }
           
        }

        if (done) {
            printf ("Closed connection on descriptor %d\n",
                    event.data.fd);

            /* Closing the descriptor will make epoll remove it
               from the set of descriptors which are monitored. */
            close (event.data.fd);
            

        }
    }
    
}



//端口由参数argv[1]指定
int main (int argc, char *argv[]) {


	//struct addrinfo *sInfo = (struct addrinfo *)malloc(sizeof(struct addrinfo)+1);
    int sfd, s;
    int efd;
    struct epoll_event event;
    struct epoll_event* events;
	struct stat st;
    if (argc != 2) {
        fprintf (stderr, "Usage: %s [port]\n", argv[0]);
        exit (EXIT_FAILURE);
    }

    sfd = create_and_bind (argv[1]);
    if (sfd == -1)
        abort ();

    s = make_socket_non_blocking (sfd);
    if (s == -1)
        abort ();

    s = listen (sfd, SOMAXCONN);
    if (s == -1) {
        perror ("listen");
        abort ();
    }
    //除了参数size被忽略外,此函数和epoll_create完全相同
    efd = epoll_create1 (0);
    if (efd == -1) {
        perror ("epoll_create");
        abort ();
    }

    event.data.fd = sfd;
    event.events = EPOLLIN | EPOLLET;//读入,边缘触发方式
    s = epoll_ctl (efd, EPOLL_CTL_ADD, sfd, &event);
    if (s == -1) {
        perror ("epoll_ctl");
        abort ();
    }

	/* 读取配置文件  */
	char *buf=(char *)malloc(32);
	read_conf_value("maxFilePathSize",buf,CFGPATH);
	MAXFILEPATHSIZE = stoi(buf);
	printf("[maxFilePathSize] = [%d]\n",MAXFILEPATHSIZE);
	
	read_conf_value("maxEvents",buf,CFGPATH);
	MAXEVENTS = stoi(buf);
	printf("[MAXEVENTS] = [%d]\n",MAXEVENTS);
	
	read_conf_value("fileBufSize",buf,CFGPATH);
	FILEBUFSIZE = stoi(buf);
	printf("[FILEBUFSIZE] = [%d]\n",FILEBUFSIZE);
	
	read_conf_value("maxLogSize",buf,CFGPATH);
	MAXLOGSIZE = stoi(buf);
	printf("[MAXLOGSIZE] = [%d]\n",MAXLOGSIZE);
	
	read_conf_value("rootDir",buf,CFGPATH);
	ROOTDIR = (char *)malloc(sizeof(char)*32);
	strcpy(ROOTDIR,buf);
	printf("[ROOTDIR] = [%s]\n",ROOTDIR);
	
	
	free(buf);

    /* Buffer where events are returned */
    events = calloc (MAXEVENTS, sizeof event);
	
	
	/* 书写服务器开启日志 */
	struct sockaddr in_addr;
    socklen_t in_len;
    char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];

    in_len = sizeof in_addr;

    //将地址转化为主机名或者服务名
    s = getnameinfo (&in_addr, in_len,
                     hbuf, sizeof hbuf,
                     sbuf, sizeof sbuf,
                     NI_NUMERICHOST | NI_NUMERICSERV);//flag参数:以数字名返回
    //主机地址和服务地址

    if (s == 0) {
        printf("(host=%s, port=%s)\n", hbuf, sbuf);
    }

	
	time_t now;  
	struct tm *tm_now;
 
	time(&now);
	tm_now = localtime(&now);
 
	//printf("now datetime: %d-%d-%d %d:%d:%d\n", 
	//		tm_now->tm_year+1900, tm_now->tm_mon+1, tm_now->tm_mday, tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec);
	
	char *logFilePath = (char *)malloc(MAXFILEPATHSIZE);
	char *content = (char *)malloc(MAXLOGSIZE);
	sprintf(logFilePath,"%s%04d-%02d-%02d","/var/www/CHDS/log/",tm_now->tm_year+1900, tm_now->tm_mon+1, tm_now->tm_mday);
	sprintf(content,"[%04d-%02d-%02d %02d:%02d:%02d] (host=139.199.98.75 port=%s)"
					" : Server opend",tm_now->tm_year+1900, tm_now->tm_mon+1,
					 tm_now->tm_mday, tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec, argv[1]);

    writeLog(logFilePath,content);
     
	
    /* The event loop */
    while (1) {
		int n = epoll_wait (efd, events, MAXEVENTS, -1);
		for (int i = 0; i < n; i++) {
        	responseEvent(efd,sfd,events[i],st);
        }
    }

    free (events);

    close (sfd);
	sprintf(logFilePath,"%s%04d-%02d-%02d","/var/www/CHDS/log/",tm_now->tm_year+1900, tm_now->tm_mon+1, tm_now->tm_mday);
	
	sprintf(content,"[%04d-%02d-%02d %02d:%02d:%02d] (host=139.199.98.75 port=%s)"
					" : Server closed",tm_now->tm_year+1900, tm_now->tm_mon+1, 
					tm_now->tm_mday, tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec, argv[1]);
					
	writeLog(logFilePath,content);
    return EXIT_SUCCESS;
}

