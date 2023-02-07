#ifndef HTTP_MSG_H
#define HTTP_MSG_H
#include <iostream>
#include <string>
#include <sstream>
#include <map>
#include <unordered_map>
#include <dirent.h>
#include <fstream>
#include <vector>
#include <cstdio>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/sendfile.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/uio.h>

#include "utils.h"
#include "http_session.h"

// 表示 Request 或 Response 中数据的处理状态
enum MSGSTATUS
{
    HANDLE_INIT,     // 正在接收/发送头部数据（请求行、请求头）
    HANDLE_HEAD,     // 正在接收/发送消息首部
    HANDLE_BODY,     // 正在接收/发送消息体
    HADNLE_COMPLATE, // 所有数据都已经处理完成
    HANDLE_ERROR,    // 处理过程中发生错误
};

// 表示消息体的类型
enum MSGBODYTYPE
{
    FILE_TYPE,  // 消息体是文件
    HTML_TYPE,  // 消息体是 HTML 页面
    EMPTY_TYPE, // 消息体为空
};

// 当接收文件时，消息体会分不同的部分，用该类型表示文件消息体已经处理到哪个部分
enum FILEMSGBODYSTATUS
{
    FILE_BEGIN_FLAG, // 正在获取并处理表示文件开始的标志行
    FILE_HEAD,       // 正在获取并处理文件属性部分
    FILE_CONTENT,    // 正在获取并处理文件内容的部分
    FILE_COMPLATE    // 文件已经处理完成
};

// 定义 Request 和 Response 公共的部分，即消息首部、消息体（可以获取消息首部的某个字段、修改与获取消息体相关的数据）
class Message
{
public:
    Message() : status(HANDLE_INIT)
    {
    }

public:
    // 请求消息和响应消息都需要使用的一些成员
    MSGSTATUS status; // 记录消息的接收状态，表示整个请求报文收到了多少/发送了多少

    std::unordered_map<std::string, std::string> msgHeader; // 保存消息首部

private:
};

// 继承 Message，对请求行的修改和获取，保存收到的首部选项
class Request : public Message
{
public:
    Request() : Message()
    {
    }
    // 输入http request的第一行, 获取请求行中的: 请求方法, 请求资源, http版本
    void setRequestLine(const std::string &requestLine)
    {
        std::istringstream lineStream(requestLine);
        // 获取请求方法
        lineStream >> requestMethod;
        // 获取请求资源
        lineStream >> rquestResourse;
        // 获取http版本
        lineStream >> httpVersion;
    }

    // 对于Request 报文，根据传入的一行首部字符串，向首部保存选项
    void addHeaderOpt(const std::string &headLine)
    {
        static std::istringstream lineStream;
        lineStream.str(headLine); // 以 istringstream 的方式处理头部选项

        std::string key, value; // 保存键和值的临时量

        lineStream >> key; // 获取 key
        key.pop_back();    // 删除键中的冒号
        lineStream.get();  // 删除冒号后的空格

        // 读取空格之后所有的数据，遇到 \n 停止，所以 value 中还包含一个 \r
        getline(lineStream, value);
        value.pop_back(); // 删除其中的 \r

        if (key == "Content-Length")
        {
            // 保存消息体的长度
            contentLength = std::stoll(value);
        }
        else if (key == "Content-Type")
        {
            // 分离消息体类型。消息体类型可能是复杂的消息体，类似 Content-Type: multipart/form-data; boundary=---------------------------24436669372671144761803083960

            // 先找出值中分号的位置
            std::string::size_type semIndex = value.find(';');
            // 根据分号查找的结果，保存类型的结果
            if (semIndex != std::string::npos)
            {
                msgHeader[key] = value.substr(0, semIndex);
                std::string::size_type eqIndex = value.find('=', semIndex);
                key = value.substr(semIndex + 2, eqIndex - semIndex - 2);
                msgHeader[key] = value.substr(eqIndex + 1);
            }
            else
            {
                msgHeader[key] = value;
            }
        }
        else if (key == "Cookie")
        {
            // 找出cookie 的 value
            // e.g. Cookie: userid=DJW498123ASD4FEQF4, key =userid, value = DJW498123ASD4FEQF4
            const static std::string userIdKey = "userid"; // userid的键值
            int startIndex = 0;
            int index;
            std::string cookieKey, cookieValue;
            for (index = 0; index < value.length() + 1; index++)
            {
                if (index >= value.length() || value[index] == ';' || value[index] == ' ')
                {
                    if (cookieKey != "")
                        cookieValue = value.substr(startIndex, index - startIndex);
                    if (cookieKey != "" && cookieValue != "")
                    {
                        break;
                    }
                }
                if (value[index] == '=')
                {
                    cookieKey = value.substr(startIndex, index - startIndex);
                    startIndex = index + 1;
                }
            }

            msgHeader[key] = cookieValue;
            std::cout << outHead("debug") <<"cookie_key|"<< cookieKey << "|cookie_value|" << cookieValue << "|" << std::endl;
        }
        else
        {
            msgHeader[key] = value;
        }
    }

public:
    std::string recvMsg; // 收到但是还未处理的数据

    std::string requestMethod;  // 请求消息的请求方法
    std::string rquestResourse; // 请求的资源
    std::string httpVersion;    // 请求的HTTP版本

    long long contentLength = 0; // 记录消息体的长度
    long long msgBodyRecvLen;    // 已经接收的消息体长度

    std::string recvFileName;        // 如果客户端发送的是文件，记录文件的名字 (http request 中的上传的文件名)
    FILEMSGBODYSTATUS fileMsgStatus; // 记录表示文件的消息体已经处理到哪些部分
private:
};

// 继承 Message，对于状态行修改和获取，设置要发送的首部选项
class Response : public Message
{
public:
    Response() : Message()
    {
    }

public:
    // 保存状态行相关数据
    std::string responseHttpVersion = "HTTP/1.1";
    std::string responseStatusCode;
    std::string responseStatusDes;

    // 以下成员主要用于在发送响应消息时暂存相关的数据

    std::unordered_map<std::string, std::string> postForm; // 存储post请求发送的表单数据

    MSGBODYTYPE bodyType;     // 消息的类型
    std::string bodyFileName; // get请求中, 请求的url(要发送数据的路径)
    std::string cookieValue;  //! http请求中带有的cookie值

    std::string beforeBodyMsg; // 消息体之前的所有数据
    int beforeBodyMsgLen;      // 消息体之前的所有数据的长度

    std::string msgBody;      // 在字符串中保存 HTML 类型的消息体
    unsigned long msgBodyLen; // 消息体的长度

    int fileMsgFd; // 文件类型的消息体保存文件描述符

    unsigned long curStatusHasSendLen; // 记录在当前状态下，这些数据已经发送的长度
private:
};

// 所有事件的基类
class EventBase
{
public:
    EventBase()
    {
    }
    virtual ~EventBase()
    {
    }

protected:
    // 保存文件描述符对应的请求的状态，因为一个连接上的数据可能非阻塞一次读取不完，所以要保存读取状态
    static std::unordered_map<int, Request> requestStatus;

    // 保存文件描述符对应的发送数据的状态，一次proces中非阻塞的写数据可能无法将数据全部传过去，所以保存当前数据发送的状态
    static std::unordered_map<int, Response> responseStatus;

    // 工具类
    static SnowFlake snowFlakeUtil;

public:
    // 不同类型事件中重写该函数，执行不同的处理方法
    virtual void process()
    {
    }
};

// 用于处理定时信号
class HandleSig : public EventBase
{
public:
    HandleSig(int sigPipeFd, int epollFd, HttpSession &session)
        : m_sigPipeFd(sigPipeFd), m_epollFd(epollFd), m_session(session){};
    virtual ~HandleSig(){};

public:
    virtual void process() override;

private:
    int m_sigPipeFd; // 用于接收信号的管道
    int m_epollFd;   // 接收连接后加入的 epoll
    HttpSession &m_session;
};

// 用于接受客户端连接的事件
class AcceptConn : public EventBase
{
public:
    AcceptConn(int listenFd, int epollFd) : m_listenFd(listenFd), m_epollFd(epollFd){};
    ~AcceptConn(){};
    virtual void process() override;

private:
    int m_listenFd;    // 保存监听套接字
    int m_epollFd;     // 接收连接后加入的 epoll
    int accetpFd = -1; // 保存接受的连接

    sockaddr_in clientAddr;  // 客户端地址
    socklen_t clientAddrLen; // 客户端地址长度
};

// 处理客户端发送的请求
class HandleRecv : public EventBase
{
public:
    HandleRecv(int clientFd, int epollFd) : m_clientFd(clientFd), m_epollFd(epollFd){};
    virtual ~HandleRecv(){};

public:
    virtual void process() override;

private:
    int m_clientFd; // 客户端套接字，从该客户端读取数据
    int m_epollFd;  // epoll 文件描述符，在需要重置事件或关闭连接时使用
    const static int READ_BUFFER_SIZE = 4096;
};

// 处理向客户端发送数据
class HandleSend : public EventBase
{
public:
    HandleSend(int clientFd, int epollFd, HttpSession &session)
        : m_clientFd(clientFd), m_epollFd(epollFd), m_httpSession(session){};
    virtual ~HandleSend(){};

public:
    virtual void process() override;

    // 用于构建状态行，参数分别表示状态行的三个部分
    std::string getStatusLine(const std::string &httpVersion, const std::string &statusCode, const std::string &statusDes);

    // 以下两个函数用来构建文件列表的页面，最终结果保存到 fileListHtml 中
    void getFileListPage(std::string &fileListHtml, std::string filedir);
    void getStaticHtmlPage(std::string &fileListHtml, const char *filename);

    // 获取某个目录下的所有文件名
    void getFileVec(const std::string dirName, std::vector<std::string> &resVec);

    // 构建头部字段：
    // contentLength        : 指定消息体的长度
    // contentType          : 指定消息体的类型
    // redirectLoction = "" : 如果是重定向报文，可以指定重定向的地址。空字符串表示不添加该首部。
    // contentRange = ""    : 如果是下载文件的响应报文，指定当前发送的文件范围。空字符串表示不添加该首部。
    std::string getMessageHeader(const std::string contentLength, const std::string contentType, const std::string redirectLoction = "", const std::string contentRange = "");
    std::string getMessageHeaderCookie(std::string userId);

private:
    int m_clientFd; // 客户端套接字，向该客户端写数据
    int m_epollFd;  // epoll 文件描述符，在需要重置事件或关闭连接时使用
    HttpSession &m_httpSession;
};

#endif