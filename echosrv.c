#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<errno.h>

#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include <arpa/inet.h>
#include<signal.h>
#include <sys/wait.h>

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

/*void echo_srv(int conn)
{
	char recvBuf[1024];
	while(1)
	{
		memset(recvBuf,0,sizeof(recvBuf));
		//int ret = read(conn,recvBuf,sizeof(recvBuf));
		int ret = readLine(conn,recvBuf,sizeof(recvBuf));
		if(ret == -1)
			ERR_EXIT("readLine");
		else if(ret == 0)
		{
			printf("close client\n");
			break;
		}

		fputs(recvBuf,stdout);		
		//write(conn,recvBuf,ret);
		writen(conn,recvBuf,strlen(recvBuf));
	}
	close(conn);
}*/

void handle_sigchld(int sig)
{
	//wait(NULL);
	while(waitpid(-1,NULL,WNOHANG) > 0);
}

int main(void)
{
	//signal(SIGCHLD,SIG_IGN);
	signal(SIGCHLD,handle_sigchld);

        int listenfd;
        if((listenfd = socket(PF_INET,SOCK_STREAM,IPPROTO_TCP)) < 0)
        //if(listenfd = socket(PF_INET,SOCK_STREAM,0)<0)
                ERR_EXIT("socket");

        struct sockaddr_in servAddr;
        memset(&servAddr,0,sizeof(servAddr));
        servAddr.sin_family = AF_INET;
        servAddr.sin_port = htons(5188);
        servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
       //servAddr.sin_addr.s_addr = inet_addr("192.168.58.129");
        //inet_aton("127.0.0.1",&servAddr.sin_addr);
	int on = 1;
	if(setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on)) < 0)
		ERR_EXIT("setsockopt");

        if(bind(listenfd,(struct sockaddr *)&servAddr,sizeof(servAddr)) < 0) 
        	ERR_EXIT("bind");
        if(listen(listenfd,SOMAXCONN) < 0)
        	ERR_EXIT("bind");
        	
/*	struct sockaddr_in peerAddr;
	socklen_t peerlen = sizeof(peerAddr);
	int conn;

	pid_t pid;
	while(1)
	{

		if((conn = accept(listenfd,(struct sockaddr*)&peerAddr,&peerlen)) < 0)
			ERR_EXIT("accept");
		printf("ip=%s port=%d\n",inet_ntoa(peerAddr.sin_addr),ntohs(peerAddr.sin_port));	
		
		pid = fork();
		if(pid == -1)
			ERR_EXIT("fork");
		else if(pid == 0)
		{
			close(listenfd);
			echo_srv(conn);
			exit(EXIT_SUCCESS);
		}
		else
			close(conn);
		
	}*/
	int nClient[FD_SETSIZE];
	int i;
	int maxi = 0;
	for(i=0; i<FD_SETSIZE; i++)
		nClient[i] = -1;

	struct sockaddr_in peerAddr;
	socklen_t peerlen = sizeof(peerAddr);
	int conn;
	int nReady;
	int maxfd = listenfd;	
	fd_set rSet;
	fd_set allSet;
	FD_ZERO(&rSet);
	FD_ZERO(&allSet);			
	FD_SET(listenfd,&allSet);
	
	while(1)
	{
		rSet = allSet;
		nReady = select(maxfd+1, &rSet, NULL, NULL, NULL);
		if(nReady == -1)
		{
			if(errno == EINTR)
				continue;
			ERR_EXIT("select");
		}
		if(nReady == 0)
			continue;
		if(FD_ISSET(listenfd,&rSet))
		{
			conn = accept(listenfd,(struct sockaddr*)&peerAddr,&peerlen);
			if(conn < 0)
				ERR_EXIT("accept");
		
			for(i=0; i<FD_SETSIZE; i++)
			{
				if(nClient[i] == -1)
				{
					nClient[i] = conn; 	
					if(i > maxi)
						maxi = i;	
					break;
				}
			}

			if(i == FD_SETSIZE)
			{		
				fprintf(stderr,"too many clients\n");
				exit(EXIT_FAILURE);
			}
			printf("Ip=%s, port=%d\n", inet_ntoa(peerAddr.sin_addr),ntohs(peerAddr.sin_port));

			FD_SET(conn,&allSet);
			if(conn > maxfd)
				maxfd = conn;

			if(--nReady < 0)
				continue;
		}

		for(i=0; i<=maxi; i++)
		{	
			conn = nClient[i];
			if(conn < 0)
				continue;		

			if(FD_ISSET(conn,&rSet))
			{
				char recvBuf[1024] = {0};
				int ret = readLine(conn,recvBuf,sizeof(recvBuf));
		                if(ret == -1)
                		        ERR_EXIT("readLine");
		                else if(ret == 0)
               			{
                        		printf("close client\n");
		                        FD_CLR(conn, &allSet);
					nClient[i] = -1;
					close(conn);
                		}

		                fputs(recvBuf,stdout);
				/*sleep(4);*/  // 127          
		                writen(conn,recvBuf,strlen(recvBuf));		
			
				if(--nReady <= 0)
					break;
			}
		}


	}




        return 0;
}
	
