#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<errno.h>

#include <sys/select.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include <arpa/inet.h>

#define ERR_EXIT(m) \
do\
{\
        perror(m);\
        exit(EXIT_FAILURE);\
}       while(0)


int main(void)
{
	int num = 0;
	while(1)
	{
        	int sock;
        	if((sock = socket(PF_INET,SOCK_STREAM,IPPROTO_TCP)) < 0)
	        { 
			sleep(4); 
		      	ERR_EXIT("socket");
		}
        	struct sockaddr_in servAddr;
	        memset(&servAddr,0,sizeof(servAddr));
        	servAddr.sin_family = AF_INET;
	        servAddr.sin_port = htons(5188);
	        servAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

		if(connect(sock,(struct sockaddr*)&servAddr,sizeof(servAddr)) < 0 )
			ERR_EXIT("connect");

		struct sockaddr_in localAddr;
		socklen_t localAddrLen = sizeof(localAddr);
		if(getsockname(sock,(struct sockaddr*)&localAddr,&localAddrLen) < 0)
			ERR_EXIT("getsockname");	
		printf("ip=%s port=%d num=%d\n",inet_ntoa(localAddr.sin_addr),ntohs(localAddr.sin_port),++num);
	}
		
        return 0;
}

