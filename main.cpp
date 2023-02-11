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

#include "threadpool/threadpool.h"
#include "httpevent/http_event.h"
#include "utils/utils.h"
#include "httpsession/http_session.h"


#define MAX_FD 10
#define TIMESLOT 5
#define MAX_EVENT_NUMBER 1024

static int pipefd[2];//! 用于信号通信的管道, 1端写信号, 0端读信号

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

void sig_handler( int sig )
{
    int save_errno = errno;
    int msg = sig;
    send( pipefd[1], ( char* )&msg, 1, 0 );
    errno = save_errno;
}



void show_error( int connfd, const char* info )
{
    printf( "%s", info );
    send( connfd, info, strlen( info ), 0 );
    close( connfd );
}

int main(int argc, char const *argv[])
{
    

    threadpool< EventBase >* pool = NULL;
    try
    {
        pool = new threadpool< EventBase >;
    }
    catch( ... )
    {
        return 1;
    }

    int user_count = 0;

    int listenfd = socket( PF_INET, SOCK_STREAM, 0 );
    assert( listenfd >= 0 );
    struct linger tmp = { 1, 0 };
    setsockopt( listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof( tmp ) );

    int ret = 0;
    int port = 8888;
    struct sockaddr_in address;
    bzero( &address, sizeof( address ) );
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    // inet_pton( AF_INET, ip, &address.sin_addr );


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
    
    //! 创建信号管道
    ret = socketpair( PF_UNIX, SOCK_STREAM, 0, pipefd );
    assert( ret != -1 );
    setNonBlocking( pipefd[1] );
    addWaitFd(epollfd, pipefd[0], true, true);

    //! 添加信号
    addsig( SIGPIPE, SIG_IGN );
    addsig(SIGALRM, sig_handler);
    alarm( TIMESLOT ); // 每次接受到SIGALRM信号之后, 要重新设置alarm()

    
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
            else if(( sockfd == pipefd[0] ) && ( events[i].events & EPOLLIN ))
            {
                event = new HandleSig(pipefd[0], epollfd);
                alarm( TIMESLOT ); // 重新定时
            }
            else if( events[i].events & ( EPOLLRDHUP | EPOLLHUP | EPOLLERR ) )
            {
                //todo
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
    delete pool;
    return 0;
}
