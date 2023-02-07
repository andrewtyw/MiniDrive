#include "http_msg.h"

// 类外初始化静态成员
std::unordered_map<int, Request> EventBase::requestStatus;
std::unordered_map<int, Response> EventBase::responseStatus;
SnowFlake EventBase::snowFlakeUtil;

void AcceptConn::process()
{
    clientAddrLen = sizeof(clientAddr);
    accetpFd = accept(m_listenFd, (sockaddr *)&clientAddr, &clientAddrLen);
    if (accetpFd == -1)
    {
        std::cout << outHead("info") << "接受新连接失败" << std::endl;
        perror("accept");
        return;
    }
    int ret = -1;
    // 将连接设置为非阻塞
    ret = setNonBlocking(accetpFd);
    // 将连接加入到监听，客户端套接字都设置为 EPOLLET 和 EPOLLONESHOT
    ret = addWaitFd(m_epollFd, accetpFd, true, true);
    assert(ret == 0);
    std::cout << outHead("info") << "接受新连接 " << accetpFd << " 成功" << std::endl;
}

void HandleSig::process()
{
    std::cout << outHead("info") << "接受到信号事件" << std::endl;
    int sig;
    char signals[1024];
    int ret = recv(m_sigPipeFd, signals, sizeof(signals), 0);
    if (ret <= 0)
    {
        return;
    }
    else
    {
        for (int i = 0; i < ret; ++i)
        {
            switch (signals[i])
            {
            case SIGALRM:
            {
                std::cout << outHead("debug") << "接受到定时信号!" << std::endl;
                m_session.tick();
                break;
            }
            default:
                break;
            }
        }
    }

    //! 由于设置了one-shot, 因此要重新注册监听事件
    modifyWaitFd(m_epollFd, m_sigPipeFd, true, true, false);
}

void HandleRecv::process()
{
    std::cout << outHead("info") << "开始处理客户端 " << m_clientFd << " 的一个 HandleRecv 事件" << std::endl;

    requestStatus[m_clientFd];

    char buf[READ_BUFFER_SIZE];
    int recvLen = 0;

    // 边从socket中读取请求, 边处理
    while (1)
    {
        recvLen = recv(m_clientFd, buf, READ_BUFFER_SIZE, 0);

        if (recvLen == 0)
        {
            std::cout << outHead("info") << "客户端 " << m_clientFd << " 关闭连接" << std::endl;
            requestStatus[m_clientFd].status = HANDLE_ERROR;
            break;
        }

        if (recvLen == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // 数据未就绪, 设置再次接收
                modifyWaitFd(m_epollFd, m_clientFd, true, true, false);
                break;
            }
            else
            {
                // 接收发生错误
                requestStatus[m_clientFd].status = HANDLE_ERROR;
                std::cout << outHead("error") << "接收数据时返回 -1 (errno = " << errno << ")" << std::endl;
                break;
            }
        }

        requestStatus[m_clientFd].recvMsg.append(buf, recvLen);
        // std::cout << outHead("debug") << "读到的数据recvMsg:\n " << requestStatus[m_clientFd].recvMsg << std::endl;

        std::string::size_type endIndex = 0; // 标记请求行的结束边界\r\n

        // 根据不同的读取状态来进行读取
        // 解析http request头
        if (requestStatus[m_clientFd].status == HANDLE_INIT)
        {
            endIndex = requestStatus[m_clientFd].recvMsg.find("\r\n");

            if (endIndex != std::string::npos)
            {
                requestStatus[m_clientFd].setRequestLine(requestStatus[m_clientFd].recvMsg.substr(0, endIndex + 2));
                requestStatus[m_clientFd].recvMsg.erase(0, endIndex + 2);
                requestStatus[m_clientFd].status = HANDLE_HEAD;
                std::cout << outHead("info") << "处理客户端 " << m_clientFd
                          << " 的请求行完成, 请求类型为:" << requestStatus[m_clientFd].requestMethod
                          << " 请求url:" << requestStatus[m_clientFd].rquestResourse
                          << std::endl;
            }
        }

        // 解析http request的头部选项
        if (requestStatus[m_clientFd].status == HANDLE_HEAD)
        {
            std::string curLine; // 头部选项的某一行
            while (1)
            {
                endIndex = requestStatus[m_clientFd].recvMsg.find("\r\n");
                if (endIndex == std::string::npos)
                    break; // http request还没读完, 继续接收

                curLine = requestStatus[m_clientFd].recvMsg.substr(0, endIndex + 2);
                requestStatus[m_clientFd].recvMsg.erase(0, endIndex + 2);

                // 如果当前行为空行, 说明请求头读取完毕, 下一行为http request body
                if (curLine == "\r\n")
                {
                    requestStatus[m_clientFd].status = HANDLE_BODY;
                    if (requestStatus[m_clientFd].msgHeader["Content-Type"] == "multipart/form-data")
                    { // 如果接收的是文件，设置消息体中文件的处理状态
                        requestStatus[m_clientFd].fileMsgStatus = FILE_BEGIN_FLAG;
                    }
                    std::cout << outHead("info") << "处理客户端 " << m_clientFd << " 的消息首部完成，开始处理请求体" << std::endl;
                    break;
                }
                else
                {
                    // 非结束行, 则分析头部选项并保存
                    requestStatus[m_clientFd].addHeaderOpt(curLine);
                    //! 给repsonse添加cookie
                    if (requestStatus[m_clientFd].msgHeader.find("Cookie") != requestStatus[m_clientFd].msgHeader.end())
                    {
                        responseStatus[m_clientFd].cookieValue = requestStatus[m_clientFd].msgHeader["Cookie"];
                    }
                }
            }
        }

        // 解析http request的请求体
        if (requestStatus[m_clientFd].status == HANDLE_BODY)
        {
            if (requestStatus[m_clientFd].requestMethod == "GET")
            {
                // 设置http response中的返回内容
                responseStatus[m_clientFd].bodyFileName = requestStatus[m_clientFd].rquestResourse;
                //! 开始监听可写事件
                modifyWaitFd(m_epollFd, m_clientFd, true, true, true);
                requestStatus[m_clientFd].status = HADNLE_COMPLATE;
                break;
            }

            if (requestStatus[m_clientFd].requestMethod == "POST")
            {
                std::string::size_type beginSize = requestStatus[m_clientFd].recvMsg.size();

                // 发送的是文件
                if (requestStatus[m_clientFd].msgHeader["Content-Type"] == "multipart/form-data")
                {
                    responseStatus[m_clientFd].bodyFileName = "/redirect";
                    modifyWaitFd(m_epollFd, m_clientFd, true, true, true);
                    requestStatus[m_clientFd].status = HADNLE_COMPLATE;
                    std::cout << outHead("error") << "客户端 " << m_clientFd << " 的 POST 请求中接收到不能处理的数据，添加 Response 写事件，返回重定向到文件列表的报文" << std::endl;
                    break;
                }
                // 发送的是表单数据
                else if (requestStatus[m_clientFd].msgHeader["Content-Type"] == "application/x-www-form-urlencoded")
                {

                    std::cout << outHead("debug") << "开始处理form表单请求体, 当前剩余的内容:\n"
                              << requestStatus[m_clientFd].recvMsg << std::endl;
                    if (requestStatus[m_clientFd].contentLength == requestStatus[m_clientFd].recvMsg.length())
                    {
                        std::cout << outHead("debug") << "接收到了请求体, 开始处理" << std::endl;
                        responseStatus[m_clientFd].bodyFileName = requestStatus[m_clientFd].rquestResourse;       // 记录request url
                        parseRequestForm(responseStatus[m_clientFd].postForm, requestStatus[m_clientFd].recvMsg); // 提取form中的key-value对
                        std::cout << outHead("debug") << "表单Key-Value size: " << responseStatus[m_clientFd].postForm.size() << std::endl;
                        for (auto it = responseStatus[m_clientFd].postForm.begin(); it != responseStatus[m_clientFd].postForm.end(); it++)
                        {
                            std::cout << outHead("debug") << "表单Key-Value: " << it->first << " : " << it->second << std::endl;
                        }

                        // 提取信息处理完成, 开始监听写事件
                        modifyWaitFd(m_epollFd, m_clientFd, true, true, true);
                        requestStatus[m_clientFd].status = HADNLE_COMPLATE;
                        std::cout << outHead("error") << "客户端 " << m_clientFd << " 的请求处理完成, 是POST请求表单" << std::endl;
                        break;
                    }
                    else
                    {
                        std::cout << outHead("debug") << "没有找到请求体" << std::endl; //! 需要重新设置监听吗?
                    }
                }
                else
                {
                    responseStatus[m_clientFd].bodyFileName = "/redirect";
                    modifyWaitFd(m_epollFd, m_clientFd, true, true, true);
                    requestStatus[m_clientFd].status = HADNLE_COMPLATE;
                    std::cout << outHead("error") << "客户端 " << m_clientFd << " 的 POST 请求中接收到不能处理的数据，添加 Response 写事件，返回重定向到文件列表的报文" << std::endl;
                    break;
                }
            }
        }
    }

    if (requestStatus[m_clientFd].status == HADNLE_COMPLATE)
    {
        std::cout << outHead("info") << "处理客户端 " << m_clientFd << " 的请求消息成功" << std::endl;
        requestStatus.erase(m_clientFd); // 从map中删除
    }
    else if (requestStatus[m_clientFd].status == HANDLE_ERROR)
    {
        std::cout << outHead("error") << "客户端 " << m_clientFd << " 的请求消息处理失败，关闭连接" << std::endl;
        // 先删除监听的文件描述符
        deleteWaitFd(m_epollFd, m_clientFd);
        // 再关闭文件描述符
        shutdown(m_clientFd, SHUT_RDWR);
        close(m_clientFd);
        requestStatus.erase(m_clientFd);
        responseStatus.erase(m_clientFd);
    }
}

void HandleSend::process()
{
    std::cout << outHead("info") << "开始处理客户端 " << m_clientFd << " 的一个 HandleSend 事件" << std::endl;
    if (responseStatus.find(m_clientFd) == responseStatus.end())
    {
        std::cout << outHead("info") << "客户端 " << m_clientFd << " 没有要处理的响应消息" << std::endl;
        return;
    }

    // 构建需要发送的数据
    if (responseStatus[m_clientFd].status == HANDLE_INIT)
    {
        // 从请求url读取 操作方法, 和操作的对象文件
        std::string opera, filename;
        if (responseStatus[m_clientFd].bodyFileName == "/")
        {
            // 如果是访问根目录，下面会直接返回文件列表
            opera = "/";
        }
        else if (responseStatus[m_clientFd].bodyFileName == "/login")
        {
            opera = "login";
        }
        else
        {
            // 如果不是访问根目录，根据 / 对URL中的路径（如 /delete/filename）进行分隔，找到要执行的操作和操作的文件
            //                                           01     (i)
            // 文件名的查找中间 / 的索引
            int i = 1;
            while (i < responseStatus[m_clientFd].bodyFileName.size() && responseStatus[m_clientFd].bodyFileName[i] != '/')
            {
                ++i;
            }
            // 检查是否包含操作和对应的文件名，如果不满足 操作+文件名 的格式，设置为重定向操作，将页面重定向到文件列表页面
            if (i < responseStatus[m_clientFd].bodyFileName.size() - 1)
            {
                opera = responseStatus[m_clientFd].bodyFileName.substr(1, i - 1); // e.g. delete
                filename = responseStatus[m_clientFd].bodyFileName.substr(i + 1); // e.g., filename
            }
            else
            {
                opera = "redirect";
            }
        }
        std::cout << outHead("tywang") << "操作方法opera: " << opera << " filename:" << filename << std::endl;

        // 请求根目录, 返回login.html
        if (opera == "/")
        {
            // 响应行
            responseStatus[m_clientFd].beforeBodyMsg = getStatusLine("HTTP/1.1", "200", "OK");

            // 响应体
            getStaticHtmlPage(responseStatus[m_clientFd].msgBody, "html/login.html");
            std::cout << outHead("debug") << "msg body:\n"
                      << responseStatus[m_clientFd].msgBody << std::endl;
            responseStatus[m_clientFd].msgBodyLen = responseStatus[m_clientFd].msgBody.size();

            // 头部
            // contentLength, contentType
            responseStatus[m_clientFd].beforeBodyMsg += getMessageHeader(std::to_string(responseStatus[m_clientFd].msgBodyLen), "html");
            responseStatus[m_clientFd].beforeBodyMsg += "\r\n";
            responseStatus[m_clientFd].beforeBodyMsgLen = responseStatus[m_clientFd].beforeBodyMsg.size();

            // 设置标识，转换到发送数据的状态
            responseStatus[m_clientFd].bodyType = HTML_TYPE;    // 设置消息体的类型
            responseStatus[m_clientFd].status = HANDLE_HEAD;    // 设置状态为等待发送消息头
            responseStatus[m_clientFd].curStatusHasSendLen = 0; // 设置当前已发送的数据长度为0
            std::cout << outHead("info") << "客户端 " << m_clientFd << " 的响应消息用来返回文件列表页面，状态行和消息体已构建完成" << std::endl;
        }
        else if (opera == "login")
        {
            bool isValidUser = false;
            std::string unid = "";
            if ((responseStatus[m_clientFd].postForm["username"] == "tywang") && (responseStatus[m_clientFd].postForm["password"] == "123456"))
            {
                isValidUser = true;

                // 如果cookieValue在session中存在, 那么重新set就好了
                if (m_httpSession.findAttribute(responseStatus[m_clientFd].cookieValue))
                {
                    unid = responseStatus[m_clientFd].cookieValue;
                }
                // 否则set一个新的value
                else
                {
                    unid = std::to_string(snowFlakeUtil.UniqueId());
                }
                m_httpSession.setAttribute(unid, "cookie");
            }

            //! 如果没有通过账号密码登录, 那么在http session 中检查是否有cookie的值
            if (!isValidUser && m_httpSession.findAttribute(responseStatus[m_clientFd].cookieValue))
            {
                unid = responseStatus[m_clientFd].cookieValue;
                std::cout << outHead("debug") << "Cookie存在, 通过" << std::endl;
                isValidUser = true;
            }

            if (isValidUser)
            {
                std::cout << outHead("debug") << "用户名和密码正确, 即将处理登录, 返回filelist页面" << filename << std::endl;
                // 响应行
                responseStatus[m_clientFd].beforeBodyMsg = getStatusLine("HTTP/1.1", "200", "OK");
                // 响应体
                std::cout << outHead("debug") << "Mark1: " << std::endl;
                getFileListPage(responseStatus[m_clientFd].msgBody, "filedir");
                std::cout << outHead("debug") << "Mark2: " << std::endl;
                responseStatus[m_clientFd].msgBodyLen = responseStatus[m_clientFd].msgBody.size();
                // 响应头
                responseStatus[m_clientFd].beforeBodyMsg += getMessageHeader(std::to_string(responseStatus[m_clientFd].msgBodyLen), "html");
                //! 设置cookie
                if (unid != "")
                    responseStatus[m_clientFd].beforeBodyMsg += getMessageHeaderCookie(unid);
                responseStatus[m_clientFd].beforeBodyMsg += "\r\n";
                responseStatus[m_clientFd].beforeBodyMsgLen = responseStatus[m_clientFd].beforeBodyMsg.size();
                // 设置标识，转换到发送数据的状态
                responseStatus[m_clientFd].bodyType = HTML_TYPE;    // 设置消息体的类型
                responseStatus[m_clientFd].status = HANDLE_HEAD;    // 设置状态为等待发送消息头
                responseStatus[m_clientFd].curStatusHasSendLen = 0; // 设置当前已发送的数据长度为0
                std::cout << outHead("info") << "客户端 " << m_clientFd << " 的响应消息用来返回文件列表页面，状态行和消息体已构建完成" << std::endl;
            }
            else
            {
                // todo 优化代码结构: 返回login.html页面这坨可以单独挑出来
                std::cout << outHead("debug") << "用户名和密码错误, 即将处理登录, 返回登录页面" << filename << std::endl;
                // 响应行
                responseStatus[m_clientFd].beforeBodyMsg = getStatusLine("HTTP/1.1", "200", "OK");

                // 响应体
                getStaticHtmlPage(responseStatus[m_clientFd].msgBody, "html/login.html");
                // html写入错误信息
                std::string errorMark = "<!--ERROR-->";
                std::string errorMsg = "<span style=\"color: red;font-size: 15px\">wrong username or password, try again</span>";
                int replacePos = responseStatus[m_clientFd].msgBody.find(errorMark);
                if (replacePos != std::string::npos)
                    responseStatus[m_clientFd].msgBody.replace(replacePos, errorMark.length(), errorMsg);

                // std::cout << outHead("debug") << "msg body:\n"
                //           << responseStatus[m_clientFd].msgBody << std::endl;
                responseStatus[m_clientFd].msgBodyLen = responseStatus[m_clientFd].msgBody.size();

                // 头部
                // contentLength, contentType
                responseStatus[m_clientFd].beforeBodyMsg += getMessageHeader(std::to_string(responseStatus[m_clientFd].msgBodyLen), "html");
                responseStatus[m_clientFd].beforeBodyMsg += "\r\n";
                responseStatus[m_clientFd].beforeBodyMsgLen = responseStatus[m_clientFd].beforeBodyMsg.size();

                // 设置标识，转换到发送数据的状态
                responseStatus[m_clientFd].bodyType = HTML_TYPE;    // 设置消息体的类型
                responseStatus[m_clientFd].status = HANDLE_HEAD;    // 设置状态为等待发送消息头
                responseStatus[m_clientFd].curStatusHasSendLen = 0; // 设置当前已发送的数据长度为0
                std::cout << outHead("info") << "客户端 " << m_clientFd << " 的响应消息用来返回文件列表页面，状态行和消息体已构建完成" << std::endl;
            }
        }
        else if (opera == "download")
        {
            // todo 代码重复, 需要抽出来写个API
            bool isValidUser = false;
            std::string unid = "";
            //! 首先检查cookie
            if (m_httpSession.findAttribute(responseStatus[m_clientFd].cookieValue))
            {
                unid = responseStatus[m_clientFd].cookieValue;
                std::cout << outHead("debug") << "Cookie存在, 通过" << std::endl;
                isValidUser = true;
            }

            if (isValidUser)
            {
                std::cout << outHead("info") << "客户端 " << m_clientFd << " 选择下载某个文件:" << filename << std::endl;
                responseStatus[m_clientFd].beforeBodyMsg = getStatusLine("HTTP/1.1", "200", "OK");
                responseStatus[m_clientFd].fileMsgFd = open(("filedir/" + filename).c_str(), O_RDONLY);
                if (responseStatus[m_clientFd].fileMsgFd == -1)
                { // 文件打开失败时，退出当前函数（避免下面关闭文件造成错误），并重置写事件，在下次进入时回复重定向报文
                    std::cout << outHead("error") << "客户端 " << m_clientFd << " 的请求消息要下载文件 " << filename << " ，但是文件打开失败，退出当前函数，重新进入用于返回重定向报文，重定向到文件列表" << std::endl;
                    responseStatus[m_clientFd] = Response();               // 重置 Response
                    responseStatus[m_clientFd].bodyFileName = "/login";    //! redirect
                    modifyWaitFd(m_epollFd, m_clientFd, true, true, true); // 重置写事件
                    return;
                }
                else
                {
                    // 获取文件信息
                    struct stat fileStat;
                    fstat(responseStatus[m_clientFd].fileMsgFd, &fileStat);

                    // 获取文件长度，作为消息体长度
                    responseStatus[m_clientFd].msgBodyLen = fileStat.st_size;
                    // 根据消息体构建消息首部
                    responseStatus[m_clientFd].beforeBodyMsg += getMessageHeader(std::to_string(responseStatus[m_clientFd].msgBodyLen), "file", std::to_string(responseStatus[m_clientFd].msgBodyLen - 1));
                    if (unid != "")
                        responseStatus[m_clientFd].beforeBodyMsg += getMessageHeaderCookie(unid);
                    // 加入空行
                    responseStatus[m_clientFd].beforeBodyMsg += "\r\n";
                    responseStatus[m_clientFd].beforeBodyMsgLen = responseStatus[m_clientFd].beforeBodyMsg.size();

                    // 设置标识，转换到发送数据的状态
                    responseStatus[m_clientFd].bodyType = FILE_TYPE;    // 设置消息体的类型
                    responseStatus[m_clientFd].status = HANDLE_HEAD;    // 设置状态为处理消息头
                    responseStatus[m_clientFd].curStatusHasSendLen = 0; // 设置当前已发送的数据长度为0
                }
            }
            else
            {
                // 重定向到登录页面
                responseStatus[m_clientFd].beforeBodyMsg = getStatusLine("HTTP/1.1", "302", "Moved Temporarily");

                // 构建重定向的消息首部
                responseStatus[m_clientFd].beforeBodyMsg += getMessageHeader("0", "html", "/", "");

                // 加入空行
                responseStatus[m_clientFd].beforeBodyMsg += "\r\n";
                responseStatus[m_clientFd].beforeBodyMsgLen = responseStatus[m_clientFd].beforeBodyMsg.size();

                // 设置标识，转换到发送数据的状态
                responseStatus[m_clientFd].bodyType = EMPTY_TYPE;   // 设置消息体的类型
                responseStatus[m_clientFd].status = HANDLE_HEAD;    // 设置状态为处理消息头
                responseStatus[m_clientFd].curStatusHasSendLen = 0; // 设置当前已发送的数据长度为0
                std::cout << outHead("info") << "客户端 " << m_clientFd << " 的响应报文是重定向报文，状态行和消息首部已构建完成" << std::endl;
            }
        }
        else if (opera == "delete")
        {
            bool isValidUser = false;
            std::string unid = "";
            //! 首先检查cookie
            if (m_httpSession.findAttribute(responseStatus[m_clientFd].cookieValue))
            {
                unid = responseStatus[m_clientFd].cookieValue;
                std::cout << outHead("debug") << "Cookie存在, 通过" << std::endl;
                isValidUser = true;
            }

            if (isValidUser)
            {
                std::cout << outHead("info") << "客户端 " << m_clientFd << " 选择删除某个文件:" << filename << std::endl;
                int ret = remove(("filedir/" + filename).c_str());
                std::cout << outHead("info") << "客户端 " << m_clientFd << " 删除文件" << ((ret == 0) ? ("成功") : ("失败")) << std::endl;

                // 重定向到/login
                responseStatus[m_clientFd].beforeBodyMsg = getStatusLine("HTTP/1.1", "302", "Moved Temporarily");

                // 构建重定向的消息首部
                responseStatus[m_clientFd].beforeBodyMsg += getMessageHeader("0", "html", "/login", "");
                if (unid != "")
                    responseStatus[m_clientFd].beforeBodyMsg += getMessageHeaderCookie(unid);
                // 加入空行
                responseStatus[m_clientFd].beforeBodyMsg += "\r\n";
                responseStatus[m_clientFd].beforeBodyMsgLen = responseStatus[m_clientFd].beforeBodyMsg.size();

                // 设置标识，转换到发送数据的状态
                responseStatus[m_clientFd].bodyType = EMPTY_TYPE;   // 设置消息体的类型
                responseStatus[m_clientFd].status = HANDLE_HEAD;    // 设置状态为处理消息头
                responseStatus[m_clientFd].curStatusHasSendLen = 0; // 设置当前已发送的数据长度为0
                std::cout << outHead("info") << "客户端 " << m_clientFd << " 的响应报文是重定向报文，状态行和消息首部已构建完成" << std::endl;
            }
            else // todo 代码优化 重定向到 /
            {
                responseStatus[m_clientFd].beforeBodyMsg = getStatusLine("HTTP/1.1", "302", "Moved Temporarily");

                // 构建重定向的消息首部
                responseStatus[m_clientFd].beforeBodyMsg += getMessageHeader("0", "html", "/", "");

                // 加入空行
                responseStatus[m_clientFd].beforeBodyMsg += "\r\n";
                responseStatus[m_clientFd].beforeBodyMsgLen = responseStatus[m_clientFd].beforeBodyMsg.size();

                // 设置标识，转换到发送数据的状态
                responseStatus[m_clientFd].bodyType = EMPTY_TYPE;   // 设置消息体的类型
                responseStatus[m_clientFd].status = HANDLE_HEAD;    // 设置状态为处理消息头
                responseStatus[m_clientFd].curStatusHasSendLen = 0; // 设置当前已发送的数据长度为0
                std::cout << outHead("info") << "客户端 " << m_clientFd << " 的响应报文是重定向报文，状态行和消息首部已构建完成" << std::endl;
            }
        }
        else // 重定向到 /
        {
            responseStatus[m_clientFd].beforeBodyMsg = getStatusLine("HTTP/1.1", "302", "Moved Temporarily");

            // 构建重定向的消息首部
            responseStatus[m_clientFd].beforeBodyMsg += getMessageHeader("0", "html", "/", "");

            // 加入空行
            responseStatus[m_clientFd].beforeBodyMsg += "\r\n";
            responseStatus[m_clientFd].beforeBodyMsgLen = responseStatus[m_clientFd].beforeBodyMsg.size();

            // 设置标识，转换到发送数据的状态
            responseStatus[m_clientFd].bodyType = EMPTY_TYPE;   // 设置消息体的类型
            responseStatus[m_clientFd].status = HANDLE_HEAD;    // 设置状态为处理消息头
            responseStatus[m_clientFd].curStatusHasSendLen = 0; // 设置当前已发送的数据长度为0
            std::cout << outHead("info") << "客户端 " << m_clientFd << " 的响应报文是重定向报文，状态行和消息首部已构建完成" << std::endl;
        }
    }

    // 发送消息, 直到发送完成或者缓冲区满
    while (1)
    {
        long long sentLen = 0;
        // 发送状态为还没发送response 头部
        if (responseStatus[m_clientFd].status == HANDLE_HEAD)
        {
            sentLen = responseStatus[m_clientFd].curStatusHasSendLen;
            sentLen = send(m_clientFd, responseStatus[m_clientFd].beforeBodyMsg.c_str() + sentLen, responseStatus[m_clientFd].beforeBodyMsgLen - sentLen, 0);
            if (sentLen == -1)
            {
                if (errno == EAGAIN)
                {
                    break; //! 下面会重置epollout继续写
                }
                else
                {
                    // 如果不是缓冲区满，设置发送失败状态，并退出循环
                    requestStatus[m_clientFd].status = HANDLE_ERROR;
                    std::cout << outHead("error") << "发送响应体和消息首部时返回 -1 (errno = " << errno << ")" << std::endl;
                    break;
                }
            }
            responseStatus[m_clientFd].curStatusHasSendLen += sentLen;

            // 判断response的请求头部(line+header)是否发送完毕
            if (responseStatus[m_clientFd].curStatusHasSendLen >= responseStatus[m_clientFd].beforeBodyMsgLen)
            {
                responseStatus[m_clientFd].status = HANDLE_BODY;    // 设置为正在处理消息体的状态
                responseStatus[m_clientFd].curStatusHasSendLen = 0; // 设置已经发送的数据长度为 0
                std::cout << outHead("info") << "客户端 " << m_clientFd << " 响应消息的状态行和消息首部发送完成，正在发送消息体..." << std::endl;
            }
        }

        if (responseStatus[m_clientFd].status == HANDLE_BODY)
        {
            if (responseStatus[m_clientFd].bodyType == HTML_TYPE)
            {
                sentLen = responseStatus[m_clientFd].curStatusHasSendLen;
                sentLen = send(m_clientFd, responseStatus[m_clientFd].msgBody.c_str() + sentLen, responseStatus[m_clientFd].msgBodyLen - sentLen, 0);
                if (sentLen == -1)
                {
                    if (errno == EAGAIN)
                    {
                        break; //! 下面会重置epollout继续写
                    }
                    else
                    {
                        // 如果不是缓冲区满，设置发送失败状态，并退出循环
                        requestStatus[m_clientFd].status = HANDLE_ERROR;
                        std::cout << outHead("error") << "发送响应体和消息首部时返回 -1 (errno = " << errno << ")" << std::endl;
                        break;
                    }
                }
                responseStatus[m_clientFd].curStatusHasSendLen += sentLen;
                if (responseStatus[m_clientFd].curStatusHasSendLen >= responseStatus[m_clientFd].msgBodyLen)
                {
                    responseStatus[m_clientFd].status = HADNLE_COMPLATE; // 设置为正在处理消息体的状态
                    responseStatus[m_clientFd].curStatusHasSendLen = 0;  // 设置已经发送的数据长度为 0
                    std::cout << outHead("info") << "客户端 " << m_clientFd << " 请求的是 HTML 文件，文件发送成功" << std::endl;
                    break;
                }
            }
            else if (responseStatus[m_clientFd].bodyType == FILE_TYPE)
            {
                sentLen = responseStatus[m_clientFd].curStatusHasSendLen;
                sentLen = sendfile(m_clientFd, responseStatus[m_clientFd].fileMsgFd, (off_t*)sentLen, responseStatus[m_clientFd].msgBodyLen - sentLen);
                if (sentLen == -1)
                {
                    if (errno != EAGAIN)
                    {
                        // 如果不是缓冲区满，设置发送失败状态
                        requestStatus[m_clientFd].status = HANDLE_ERROR;
                        std::cout << outHead("error") << "发送文件时返回 -1 (errno = " << errno << ")" << std::endl;
                        break;
                    }
                    // 如果缓冲区已满，退出循环，下面会重置 EPOLLOUT 事件，等待下次进入函数继续发送
                    break;
                }

                // 累加已发送的数据长度
                responseStatus[m_clientFd].curStatusHasSendLen += sentLen;

                // 文件发送完成后，重置 Response 为访问根目录的响应，向客户端传递文件列表
                if (responseStatus[m_clientFd].curStatusHasSendLen >= responseStatus[m_clientFd].msgBodyLen)
                {
                    responseStatus[m_clientFd].status = HADNLE_COMPLATE; // 设置为事件处理完成
                    responseStatus[m_clientFd].curStatusHasSendLen = 0;  // 设置已经发送的数据长度为 0

                    std::cout << outHead("info") << "客户端 " << m_clientFd << " 请求的文件发送完成" << std::endl;
                    break;
                }

            }
            else if (responseStatus[m_clientFd].bodyType == EMPTY_TYPE) //! 这里其实它没发送
            {
                responseStatus[m_clientFd].status = HADNLE_COMPLATE; // 设置为事件处理完成
                responseStatus[m_clientFd].curStatusHasSendLen = 0;  // 设置已经发送的数据长度为 0
                std::cout << outHead("info") << "客户端 " << m_clientFd << " 的重定向报文发送成功" << std::endl;
                break;
            }
        }

        if (responseStatus[m_clientFd].status == HANDLE_ERROR)
        { // 如果是出错状态，退出 while 处理
            break;
        }
    }

    if (responseStatus[m_clientFd].status == HADNLE_COMPLATE)
    {
        // 完成发送数据后删除该响应
        responseStatus.erase(m_clientFd);
        modifyWaitFd(m_epollFd, m_clientFd, true, false, false); // 不再监听写事件
        std::cout << outHead("info") << "客户端 " << m_clientFd << " 的响应报文发送成功" << std::endl;
    }
    else if (responseStatus[m_clientFd].status == HANDLE_ERROR)
    {
        responseStatus.erase(m_clientFd);
        // 不再监听写事件
        modifyWaitFd(m_epollFd, m_clientFd, true, false, false);
        // 关闭文件描述符
        shutdown(m_clientFd, SHUT_WR);
        close(m_clientFd);
        std::cout << outHead("error") << "客户端 " << m_clientFd << " 的响应报文发送失败，关闭相关的文件描述符" << std::endl;
    }
    else
    {
        modifyWaitFd(m_epollFd, m_clientFd, true, true, true);
        return; // 目前写入的状态, 以及已经写入的字节已经被记录, 因此可以退出函数, 等待下一次可写事件, 让另一个线程来处理.
    }

    // 处理成功或非文件打开失败时需要关闭文件
    if (responseStatus[m_clientFd].bodyType == FILE_TYPE)
    {
        close(responseStatus[m_clientFd].fileMsgFd);
    }
}

void HandleSend::getStaticHtmlPage(std::string &fileListHtml, const char *filename)
{
    std::ifstream fileListStream(filename, std::ios::in);
    std::string tempLine;
    while (getline(fileListStream, tempLine))
    {
        fileListHtml += tempLine + "\n";
    }
}

std::string HandleSend::getStatusLine(const std::string &httpVersion, const std::string &statusCode, const std::string &statusDes)
{
    std::string statusLine;
    // 记录状态行相关的参数
    responseStatus[m_clientFd].responseHttpVersion = httpVersion;
    responseStatus[m_clientFd].responseStatusCode = statusCode;
    responseStatus[m_clientFd].responseStatusDes = statusDes;
    // 构建状态行
    statusLine = httpVersion + " ";
    statusLine += statusCode + " ";
    statusLine += statusDes + "\r\n";

    return statusLine;
}

std::string HandleSend::getMessageHeaderCookie(std::string userId)
{
    return "Set-Cookie: userid=" + userId + "; path=/; Max-Age=600\r\n";
}

std::string HandleSend::getMessageHeader(const std::string contentLength, const std::string contentType, const std::string redirectLoction, const std::string contentRange)
{
    std::string headerOpt;

    // 添加消息体长度字段
    if (contentLength != "")
    {
        headerOpt += "Content-Length: " + contentLength + "\r\n";
    }

    // 添加消息体类型字段
    if (contentType != "")
    {
        if (contentType == "html")
        {
            headerOpt += "Content-Type: text/html;charset=UTF-8\r\n"; // 发送网页时指定的类型
        }
        else if (contentType == "file")
        {
            headerOpt += "Content-Type: application/octet-stream\r\n"; // 发送文件时指定的类型
        }
    }

    // 添加重定向位置字段
    if (redirectLoction != "")
    {
        headerOpt += "Location: " + redirectLoction + "\r\n";
    }

    // 添加文件范围的字段
    if (contentRange != "")
    {
        headerOpt += "Content-Range: 0-" + contentRange + "\r\n";
    }

    headerOpt += "Connection: keep-alive\r\n";

    return headerOpt;
}

void HandleSend::getFileListPage(std::string &fileListHtml, std::string filedir)
{
    std::vector<std::string> fileVec;
    getFileVec(filedir, fileVec);
    std::ifstream fileListStream("html/filelist.html", std::ios::in);
    std::string tempLine;
    std::string replaceMarker = "<!--FILELIST-->";
    while (1)
    {
        getline(fileListStream, tempLine);
        if (tempLine == replaceMarker)
        {
            break;
        }
        fileListHtml += tempLine + "\n";
    }
    for (const auto &filename : fileVec)
    {
        fileListHtml += "            <tr><td>" + filename +
                        "</td><td><a href=\"download/" + filename +
                        "\">Download</a></td> <td><a href=\"delete/" + filename +
                        "\" onclick=\"return confirmDelete();\">Delete</a></td></tr>" + "\n";
    }
    // 将文件列表注释后的语句加入后面
    while (getline(fileListStream, tempLine))
    {
        fileListHtml += tempLine + "\n";
    }
}

void HandleSend::getFileVec(const std::string dirName, std::vector<std::string> &resVec)
{
    // 使用 dirent 获取文件目录下的所有文件
    DIR *dir; // 目录指针
    dir = opendir(dirName.c_str());
    struct dirent *stdinfo;
    while (1)
    {
        // 获取文件夹中的一个文件
        stdinfo = readdir(dir);
        if (stdinfo == nullptr)
        {
            break;
        }
        resVec.push_back(stdinfo->d_name);
        if (resVec.back() == "." || resVec.back() == "..")
        {
            resVec.pop_back();
        }
    }
}