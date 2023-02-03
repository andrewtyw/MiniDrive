#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/uio.h>
#include "locker.h"


// 每个http_conn就是一个user
class http_conn
{
public:
    /*文件名的最大长度*/
    static const int FILENAME_LEN = 200;
    static const int READ_BUFFER_SIZE = 2048;
    static const int WRITE_BUFFER_SIZE = 1024;
    enum METHOD
    {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATCH
    };
    /*解析客户请求的时候, 状态机的状态*/
    enum CHECK_STATE
    {
        CHECK_STATE_REQUESTLINE = 0,
        CHECK_STATE_HEADER,
        CHECK_STATE_CONTENT
    };
    enum HTTP_CODE
    {
        NO_REQUEST,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURCE,
        FORBIDDEN_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION
    };
    /*行的读取状态*/
    enum LINE_STATUS
    {
        LINE_OK = 0,
        LINE_BAD,
        LINE_OPEN
    };

public:
    static int m_epollfd;
    static int m_user_count;

private:
    // http 连接的socket和address
    int m_sockfd;
    sockaddr_in m_address;

    char m_read_buf[READ_BUFFER_SIZE];
    char* get_m_read_buf(){return m_read_buf;}
    int m_read_idx;    // 索引: 指向m_read_buf中已读的最后一个字节的下一个
    int m_checked_idx; // 正在分析的字符
    int m_start_line;  // 行的起始位置
    char m_write_buf[WRITE_BUFFER_SIZE]; // http response
    int m_write_idx; // 写缓冲区待发送的字节数

    CHECK_STATE m_check_state; // 状态机的状态
    METHOD m_method;           // 请求方法

    char m_real_file[FILENAME_LEN]; // 客户请求的目标文件的完整路径
    char *m_url;                    // 请求文件的文件名
    char *m_version;                // http 版本号
    char *m_host;
    int m_content_length;
    bool m_linger; // http请求是否要保持连接

    char *m_file_address;    // 客户请求的目标文件在内存中的位置
    struct stat m_file_stat; // 目标文件状态
    struct iovec m_iv[2];    //  传输的文件
    int m_iv_count;          // 被写内存块的数量

public:
    http_conn() {}
    ~http_conn() {}

public:
    /**
     * @brief  初始化新接受的连接
     * @param  sockfd:
     * @param  addr:
     * @retval None
     */
    void init(int sockfd, const sockaddr_in &addr);
    /**
     * @brief  关闭连接
     * @param  real_close:
     * @retval None
     */
    void close_conn(bool real_close = true);
    /**
     * @brief  处理客户请求
     */
    void process();
    /**
     * @brief  在socket fd中读取内容到buffer, 更新m_read_idx
     * @retval 
     */
    bool read();
    /*非阻塞写*/
    bool write();

private:
    /*初始化连接*/
    void init();

    /**
     * @brief  主状态机, 解析http请求, 根据请求的内容判断是否进行do_request
     * @retval 请求码
     */
    HTTP_CODE process_read();
    /**
     * @brief  产生http应答
     * @param  ret:
     * @retval
     */
    bool process_write(HTTP_CODE ret);

    /**
     * @brief  检查请求行: m_method: GET, version是否http/1.1, url格式是否正确, 完成后状态机的状态设为: CHECK_STATE_HEADER
     * @param  text: 请求行的文本
     * @retval NO_REQUEST / BAD_REQUEST
     */
    HTTP_CODE parse_request_line(char *text);

    /**
     * @brief  获取http的头部选项, 改变态机的状态为:CHECK_STATE_CONTENT, 根据头部选项设置m_linger, m_content_length, m_host
     * @param  text: 请求行的文本
     * @retval GET_REQUEST / BAD_REQUEST
     */
    HTTP_CODE parse_headers(char *text);

    /**
     * @brief  判断http content是否被完整地读入了 (并没有解析)
     * @param  text: 请求行的文本
     * @retval  / BAD_REQUEST
     */
    HTTP_CODE parse_content(char *text);

    /**
     * @brief  打开请求的目标文件, 复制文件到内存, 成功则返回FILE_REQUEST
     * @retval 
     */
    HTTP_CODE do_request();
    char *get_line() { return m_read_buf + m_start_line; }

    /**
     * @brief  分析http请求, 把m_checked_idx移动到http请求的header后
     * @retval LINE_OK:完成, LINE_OPEN: m_checked_idx继续移动, LINE_BAD: header错误
     */
    LINE_STATUS parse_line();

    /**
     * @brief  销毁创建的内存映射区
     * @retval None
     */
    void unmap();
    bool add_response(const char *format, ...);
    bool add_content(const char *content);
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_length);
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();
};
#endif