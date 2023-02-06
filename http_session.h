#ifndef HTTL_SESSION_H
#define HTTL_SESSION_H
#include <string>
#include <unistd.h>
#include <iostream>
#include <chrono>
#include <vector>
#include <queue>
#include <unordered_set>

class HttpSessionItem
{
public:
    HttpSessionItem(std::string key, std::string value)
    {
        this->key = key;
        this->value = value;
        timeout = refreshTimeout();
    }
    static int64_t refreshTimeout()
    {
        std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>
            tpMicro = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
        int64_t time = tpMicro.time_since_epoch().count();
        time = (time / 1000) + TIMEOUT_SLOT; // 转换成秒
        return time;
    }

public:
    std::string key;
    std::string value;
    int64_t timeout;                             // 过期时间(时间戳)
    const static int64_t TIMEOUT_SLOT = 5; // 过期时间

public:
    std::string DEBUG_toString()
    {
        std::string returnVal = "key: " + key + " value: " + value +" Timeout: "+std::to_string(timeout);
        return returnVal;
    }
};

// 单例模式
class HttpSession
{
public:
    ~HttpSession() {}
    HttpSession(const HttpSession &) = delete;            // 禁止赋值构造
    HttpSession &operator=(const HttpSession &) = delete; // 禁止拷贝构造
    static HttpSession &get_instance()
    {
        static HttpSession instance;
        return instance;
    }

    // 将一个key-value对插入
    void setAttribute(std::string key, std::string value);

    // 获取key对应的value
    std::string  getAttribute(std::string key);

    // 查询是否有该key (常用, 因此在set中查找)
    bool findAttribute(std::string key);

    // 从会话中删除name属性，如果不存在不会执行，也不会抛出错误
    void removeAttribute(std::string key);

    // 执行定时任务, 删除timeout<current_time的HttpSessionItem
    void tick();

    void DEBUG_printAll()
    {
        std::vector<HttpSessionItem>::iterator it;
        std::cout<< "In vector:"<<std::endl;
        for (it = sessionItems.begin(); it != sessionItems.end(); it++)
        {
            std::cout<< (*it).DEBUG_toString() <<std::endl;
        }
        std::cout<< "In set:"<<std::endl;
        std::unordered_set<std::string>::iterator it_S;
        for (it_S = sessionItemsSet.begin(); it_S != sessionItemsSet.end(); it_S++)
        {
            std::cout<< (*it_S) <<", ";
        }
        std::cout<<"size: "<<sessionItemsSet.size()<<std::endl;
    }

private:
    HttpSession() {}
    std::vector<HttpSessionItem> sessionItems;
    std::unordered_set<std::string> sessionItemsSet;
    std::vector<HttpSessionItem>::iterator getTargetIterator(std::string key);
};

#endif