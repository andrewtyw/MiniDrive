#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>

#include "threadpool.h"
#include "http_msg.h"
#include "utils.h"


#define MAX_FD 10
#define MAX_EVENT_NUMBER 1024



void addfd(int epollfd, int fd, bool one_shot)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    if (one_shot)
    {
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setNonBlocking(fd);
}

void addsig( int sig, void( handler )(int), bool restart = true )
{
    struct sigaction sa;
    memset( &sa, '\0', sizeof( sa ) );
    sa.sa_handler = handler;
    if( restart )
    {
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset( &sa.sa_mask );
    assert( sigaction( sig, &sa, NULL ) != -1 );
}

void show_error( int connfd, const char* info )
{
    printf( "%s", info );
    send( connfd, info, strlen( info ), 0 );
    close( connfd );
}

int main(int argc, char const *argv[])
{
    const char* ip = "10.0.4.7"; 
    int port = 8888;
    // int port = atoi( argv[1] );
    printf("124.223.47.124 %d\n", port);
    printf("ip:%s, port:%d\n",ip,port);
    addsig( SIGPIPE, SIG_IGN );

    threadpool< EventBase >* pool = NULL;
    try
    {
        pool = new threadpool< EventBase >;
    }
    catch( ... )
    {
        return 1;
    }

    //! 删除, 或许
    EventBase* users = new EventBase[ MAX_FD ];
    assert( users );

    int user_count = 0;

    int listenfd = socket( PF_INET, SOCK_STREAM, 0 );
    assert( listenfd >= 0 );
    struct linger tmp = { 1, 0 };
    setsockopt( listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof( tmp ) );

    int ret = 0;
    struct sockaddr_in address;
    bzero( &address, sizeof( address ) );
    address.sin_family = AF_INET;
    inet_pton( AF_INET, ip, &address.sin_addr );
    address.sin_port = htons( port );

    ret = bind( listenfd, ( struct sockaddr* )&address, sizeof( address ) );
    assert( ret >= 0 );

    ret = listen( listenfd, 5 );
    assert( ret >= 0 );

    epoll_event events[ MAX_EVENT_NUMBER ];
    int epollfd = epoll_create( 5 );
    assert( epollfd != -1 );

    setNonBlocking(listenfd);
    ret = addWaitFd(epollfd, listenfd, true, false);
    if(ret != 0){
        std::cout << outHead("error") << "添加监控 Listen 套接字失败" << std::endl;
        return -1;
    }
    std::cout << outHead("info") << "epoll 中添加监听套接字成功" << std::endl;
    

    
    EventBase* event = NULL;
    while( true )
    {
        int number = epoll_wait( epollfd, events, MAX_EVENT_NUMBER, -1 );
        if ( ( number < 0 ) && ( errno != EINTR ) )
        {
            printf( "epoll failure\n" );
            break;
        }
        for ( int i = 0; i < number; i++ )
        {
            int sockfd = events[i].data.fd;
            if( sockfd == listenfd )
            {
                event = new AcceptConn(sockfd, epollfd);
            }
            else if( events[i].events & ( EPOLLRDHUP | EPOLLHUP | EPOLLERR ) )
            {
                // users[sockfd].close_conn();
            }
            else if( events[i].events & EPOLLIN )
            {
                event = new HandleRecv(events[i].data.fd, epollfd);
            }
            else if( events[i].events & EPOLLOUT )
            {
                event = new HandleSend(events[i].data.fd, epollfd);
            }
            else
            {}

            pool->append(event);
        }
    }
    close( epollfd );
    close( listenfd );
    delete [] users;
    delete pool;
    return 0;
}
