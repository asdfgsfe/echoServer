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

ssize_t readn(int fd, void *buf, size_t count)
{
        size_t nlength = count;
        ssize_t nRead;
        char *pBuf = (char *)buf;

        while(nlength > 0)
        {
                if((nRead = read(fd,pBuf,nlength)) < 0)
                {
                        if(errno == EINTR)
                                continue;
                        return -1;
                }
                else if(nRead == 0)
                        return count - nlength;
                pBuf += nRead;
                nlength -= nRead;
        }
	return count;
}     

ssize_t writen(int fd, void *buf, size_t count)
{
        size_t nlength = count;
        ssize_t nWritten;
        char *pBuf = (char *)buf;

        while(nlength > 0)
        {
                if((nWritten = write(fd,pBuf,nlength)) < 0)
                {
                        if(errno == EINTR)
                                continue;
                        return -1;
                }
                else if(nWritten == 0)
                       continue;
                pBuf += nWritten;
                nlength -= nWritten;
        }
        return count;
}

ssize_t recv_peek(int sockfd, void *buf, size_t len)
{
        while(1)
        {
                int ret = recv(sockfd,buf,len,MSG_PEEK);
                if(ret == -1 && errno == EINTR)
                        continue;
                return ret;
        }
}

ssize_t readLine(int sockfd, void *buf, size_t maxLine)
{
        ssize_t nRead;
        char *pBuf = (char*)buf;
        int ret;
        ssize_t nLen = maxLine;

        while(1)
        {
                ret = recv_peek(sockfd,pBuf,nLen);
                if(ret < 0)
                        return ret;
                else if(ret == 0)
                        return ret;

                nRead = ret;
                int i;
                for(i=0; i<nRead; i++)
                {
                        if(pBuf[i] == '\n')
                        {
                                ret = readn(sockfd,pBuf,i+1);
				 if(ret != i+1)
                                        exit(EXIT_FAILURE);
                                return ret;
                        }
                }

                if(nRead > nLen)
                        exit(EXIT_FAILURE);
                nLen -= nRead;
                ret = readn(sockfd,pBuf,nRead);
                if(ret != nRead)
                        exit(EXIT_FAILURE);

                pBuf += nRead;
        }
        return -1;
}

void echo_cli(int sock)
{
	/*char sendBuf[1024];
        char recvBuf[1024];
        memset(sendBuf,0,sizeof(sendBuf));
        memset(recvBuf,0,sizeof(recvBuf));

        while(fgets(sendBuf,sizeof(sendBuf),stdin) != NULL)
        {
                writen(sock,&sendBuf,strlen(sendBuf));
                //write(sock,sendBuf,strlen(sendBuf));
                //write(sock,recvBuf,sizeof(recvBuf)); //jie guo deng yu writen

                int ret = readLine(sock,&recvBuf,sizeof(recvBuf));
                if(ret == -1)

                        ERR_EXIT("readLine");
                else if(ret == 0)
                {
                        printf("servic client\n");
			 break;
                }
                fputs(recvBuf,stdout);

                memset(sendBuf,0,sizeof(sendBuf));
                memset(recvBuf,0,sizeof(recvBuf));
        }*/

	fd_set rSet;
	FD_ZERO(&rSet);
	int nReady;
	int maxfd;
	int fd_stdin = fileno(stdin);
	
	if(fd_stdin > sock)
		maxfd = fd_stdin;
	else
		maxfd = sock;

	char sendBuf[1024] = {0};
        char recvBuf[1024] = {0};
	int stdinEof = 0;

	while(1)
	{
		if(stdinEof == 0)
			FD_SET(fd_stdin,&rSet);
		FD_SET(sock,&rSet);
		
		if((nReady = select(maxfd+1,&rSet,NULL,NULL,NULL)) < 0)	
			ERR_EXIT("select");
		if(nReady == 0)
			continue;

		if(FD_ISSET(fd_stdin,&rSet))
		{
			if(fgets(sendBuf,sizeof(sendBuf),stdin) == NULL)
			{
				/* 127
				break;*/
				stdinEof = 1;
				shutdown(sock,SHUT_WR);
			}
			writen(sock,sendBuf,strlen(sendBuf));
			memset(sendBuf,0,sizeof(sendBuf));
		}		
		if(FD_ISSET(sock,&rSet))
		{
			int ret = readLine(sock,&recvBuf,sizeof(recvBuf));
			if(ret == -1)
				ERR_EXIT("readLine");
			else if(ret == 0)
			{
				printf("server close\n");
				break;
			}

			fputs(recvBuf,stdout);
			memset(recvBuf,0,sizeof(recvBuf));
		}
	}	
        /* 127
	close(sock);
	*/
}

int main(void)
{
        int sock;
        if((sock = socket(PF_INET,SOCK_STREAM,IPPROTO_TCP)) < 0)
        //if(listenfd = socket(PF_INET,SOCK_STREAM,0)<0)
                ERR_EXIT("socket");

        struct sockaddr_in servAddr;
        memset(&servAddr,0,sizeof(servAddr));
        servAddr.sin_family = AF_INET;
        servAddr.sin_port = htons(5188);
        //servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
        servAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
        //inet_aton("127.0.0.1",&servAddr.sin_addr);

	if(connect(sock,(struct sockaddr*)&servAddr,sizeof(servAddr)) < 0 )
		ERR_EXIT("connect");

	struct sockaddr_in localAddr;
	socklen_t localAddrLen = sizeof(localAddr);
	if(getsockname(sock,(struct sockaddr*)&localAddr,&localAddrLen) < 0)
		ERR_EXIT("getsockname");	
	printf("ip=%s port=%d\n",inet_ntoa(localAddr.sin_addr),ntohs(localAddr.sin_port));

	echo_cli(sock);				

        return 0;
}

