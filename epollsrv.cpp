#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<errno.h>

#include <fcntl.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<signal.h>
#include<sys/wait.h>
#include<sys/epoll.h>

#include<vector>
#include<algorithm>

typedef std::vector<struct epoll_event> eventList;

#define ERR_EXIT(m) \
do\
{\
        perror(m);\
        exit(EXIT_FAILURE);\
}       while(0)

void activate_nonblock(int fd)
{
        int ret;
        int flags = fcntl(fd, F_GETFL);
        if(flags == -1)
                ERR_EXIT("fcntl");

        flags |= O_NONBLOCK;

        ret = fcntl(fd, F_SETFL, flags);
        if(ret == -1)
                ERR_EXIT("fcntl");
}

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

void handle_sigchld(int sig)
{
        //wait(NULL);
        while(waitpid(-1,NULL,WNOHANG) > 0);
}

void handle_sigpipe(int sig)
{
	printf("recv a sigpipe sig=%d\n", sig);
}

int main(void)
{
	signal(SIGCHLD,handle_sigchld);
	signal(SIGPIPE,handle_sigpipe);

        int listenfd;
	if((listenfd = socket(PF_INET,SOCK_STREAM,IPPROTO_TCP)) < 0)
                ERR_EXIT("socket");

        struct sockaddr_in servAddr;
        memset(&servAddr,0,sizeof(servAddr));
        servAddr.sin_family = AF_INET;
        servAddr.sin_port = htons(5188);
        servAddr.sin_addr.s_addr = htonl(INADDR_ANY);

	int on = 1;
        if(setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on)) < 0)
                ERR_EXIT("setsockopt");

        if(bind(listenfd,(struct sockaddr *)&servAddr,sizeof(servAddr)) < 0)
                ERR_EXIT("bind");
        
	if(listen(listenfd,SOMAXCONN) < 0)
                ERR_EXIT("bind");
	
	std::vector<int> clients;
	int epollfd;
	epollfd = epoll_create1(EPOLL_CLOEXEC);
	
	struct epoll_event event;
	event.data.fd = listenfd;
	event.events = EPOLLIN | EPOLLET;

	epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, &event);
	
	eventList events(16);
	struct sockaddr_in peerAddr;
	socklen_t peerlen;
	int conn;
	int i;
	int nReady;
	int srvConnNum = 0;	

	while(1)
	{
		nReady = epoll_wait(epollfd, &*events.begin(), static_cast<int>(events.size()), -1);
                if(nReady == -1)
                {
                        if(errno == EINTR)
                                continue;
                        ERR_EXIT("epoll_wait");
                }
                if(nReady == 0)
                        continue;


		
		for(i=0; i<nReady; i++)
		{
			if(event.data.fd == listenfd)
			{
				peerlen = sizeof(peerAddr);
				conn = accept(listenfd, (struct sockaddr*)&peerAddr, &peerlen);
				if(conn < 0)
					ERR_EXIT("accept");
				printf("Ip=%s, port=%d, srvConnNum=%d\n", inet_ntoa(peerAddr.sin_addr),ntohs(peerAddr.sin_port), ++srvConnNum);
				
				clients.push_back(conn);
				activate_nonblock(conn);
				
				event.data.fd = conn;				
				event.events = EPOLLIN | EPOLLET;
				
				epoll_ctl(epollfd, EPOLL_CTL_ADD, conn, &event);
			}
			else if(events[i].events & EPOLLIN)
			{
				conn = events[i].data.fd;
				if(conn < 0)
					continue;
			
				char recvBuf[1024] = {0};
                                int ret = readLine(conn, recvBuf, sizeof(recvBuf));
                                if(ret == -1)
                                        ERR_EXIT("readLine");
				else if(ret == 0)
                                {
                                        printf("close client\n");
                                        close(conn);

					event = events[i];
					epoll_ctl(epollfd, EPOLL_CTL_DEL, conn, &event);
					clients.erase(std::remove(clients.begin(), clients.end(), conn), clients.end());
                                }

                                fputs(recvBuf,stdout);
                                writen(conn, recvBuf, strlen(recvBuf));
			}
		}
	}
	return 0;
}
























