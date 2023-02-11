#include "utils.h"

// 以 "09:50:19.0619 2022-09-26 [logType]: " 格式返回当前的时间和输出类型，logType 指定输出的类型：
// init  : 表示服务器的初始化过程
// error : 表示服务器运行中的出错消息
// info  : 表示程序的运行信息
std::string outHead(const std::string logType){
    // 获取并输出时间
    auto now = std::chrono::system_clock::now();
    time_t tt = std::chrono::system_clock::to_time_t(now);
    auto time_tm = localtime(&tt);
    
    struct timeval time_usec;
    gettimeofday(&time_usec, NULL);

    char strTime[30] = { 0 };
    sprintf(strTime, "%02d:%02d:%02d.%05ld %d-%02d-%02d",
            time_tm->tm_hour, time_tm->tm_min, time_tm->tm_sec, time_usec.tv_usec, 
            time_tm->tm_year + 1900, time_tm->tm_mon + 1, time_tm->tm_mday);
    
    std::string outStr;
    // 添加时间部分
    outStr += strTime;
    // 根据传入的参数指定输出的类型
    if(logType == "init"){
        outStr += " [init]: ";
    }else if(logType == "error"){
        outStr += " [erro]: ";
    }else{
        outStr += " ["+logType+"]: ";
    }
    
    //! 提醒有日志系统使用
    outStr = "[!注意, cout太多改不过来了, 请使用日志系统的宏函数] " + outStr;
    return outStr;
}


// 向 epollfd 添加文件描述符，并指定监听事件。edgeTrigger：边缘触发，isOneshot：EPOLLONESHOT
int addWaitFd(int epollFd, int newFd, bool edgeTrigger, bool isOneshot){
    epoll_event event;
    event.data.fd = newFd;

    event.events = EPOLLIN;
    if(edgeTrigger){
        event.events |= EPOLLET;
    }
    if(isOneshot){
        event.events |= EPOLLONESHOT;
    }

    int ret = epoll_ctl(epollFd, EPOLL_CTL_ADD, newFd, &event);
    if(ret != 0){
        std::cout << outHead("error") << "添加文件描述符失败" << std::endl;
        return -1;
    }
    return 0;
}

// 修改正在监听文件描述符的事件。edgeTrigger:是否为边沿触发，resetOneshot:是否设置 EPOLLONESHOT，addEpollout:是否监听输出事件
int modifyWaitFd(int epollFd, int modFd, bool edgeTrigger, bool resetOneshot, bool addEpollout){
    epoll_event event;
    event.data.fd = modFd;

    event.events = EPOLLIN;

    if(edgeTrigger){
        event.events |= EPOLLET;
    }
    if(resetOneshot){
        event.events |= EPOLLONESHOT;
    }
    if(addEpollout){
        event.events |= EPOLLOUT;
    }

    int ret = epoll_ctl(epollFd, EPOLL_CTL_MOD, modFd, &event);
    if(ret != 0){
        std::cout << outHead("error") << "修改文件描述符失败" << std::endl;
        return -1;
    }
    return 0;
}

// 删除正在监听的文件描述符
int deleteWaitFd(int epollFd, int deleteFd){
    int ret = epoll_ctl(epollFd, EPOLL_CTL_DEL, deleteFd, nullptr);
    if(ret != 0){
        std::cout << outHead("error") << "删除监听的文件描述符失败" << std::endl;
        return -1;
    }
    return 0;
}

// 设置文件描述符为非阻塞

int setNonBlocking(int fd){
    // int oldFlag = fcntl(fd, F_GETFL);
    // int ret = fcntl(fd, F_GETFL, oldFlag | O_NONBLOCK);
    // if(ret != 0){
    //     perror("fcntl");
    //     return -1;
    // }
    // return 0;
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

// 把http request表单中的form提取到一个map, e.g., 
// e.g. 把username=12357&password=99999\n构建成一个map: {{username,12357},{password,99999}}
void parseRequestForm(std::unordered_map<std::string, std::string> &map, std::string rawMsg)
{
    int index = 0;
    int start_index = index;
    std::string key, value;
    std::string msg = rawMsg+"\n"; //为了方便操作, 加多个\n表示结束
    for(;index<msg.length(); index++)
    {
        // std::cout<<msg[index]<<std::endl;
        if(msg[index]=='=')
        {
            key = msg.substr(start_index, index-start_index);
            start_index = index+1;
        }
        if(msg[index]=='&' || msg[index]=='\n')
        {
            value = msg.substr(start_index, index-start_index);
            start_index = index+1;
            assert(key!="" && value!="");
            map[key] = value;
            key = value = "";
        }
    }
}


int64_t SnowFlake::TimeMs()
{
    std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> tpMicro = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
    int64_t timeMs = tpMicro.time_since_epoch().count();
    return timeMs;
}

int64_t SnowFlake::NextMs(int64_t lastTimeMillis)
{
    int64_t currentTimeMillis = TimeMs();
    while (currentTimeMillis <= lastTimeMillis)
    {
        currentTimeMillis = TimeMs();
    }
    return currentTimeMillis;
}

int64_t SnowFlake::UniqueId()
{
    int64_t now = TimeMs();
    if (now == m_lasttm)
    {
        m_seq = (m_seq + 1) & m_seqMask;
        if (m_seq == 0)
        {
            now = NextMs(now);
        }
    }
    else
    {
        m_seq = 0; // 最大为1024
    }
    m_lasttm = now;
    int64_t uid = (now - m_epoch) << 22 | m_mechine << 16 | pid << 10 | m_seq;
    return uid;
}