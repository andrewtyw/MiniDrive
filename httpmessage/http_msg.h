#ifndef HTTP_MSG_H
#define HTTP_MSG_H
#include <unordered_map>
#include <iostream>
#include <string>
#include <sstream>
#include "../utils/utils.h"
#include "../logger/Logger.h"



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
    void setRequestLine(const std::string &requestLine);

    // 对于Request 报文，根据传入的一行首部字符串，向首部保存选项
    void addHeaderOpt(const std::string &headLine);

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

#endif