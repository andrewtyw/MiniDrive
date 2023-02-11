#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <iostream>
#include <string>
#include <stdarg.h>
#include <pthread.h>
#include <queue>
#include <chrono>
#include <sys/time.h>
#include <string.h>
#include "../threadpool/locker.h"
#define SINGLE_LOG_BUFFER_SIZE 1024
#define LOG_INFO(format, ...) Logger::get_instance().append(" INFO", format, ##__VA_ARGS__)
#define LOG_ERROR(format, ...) Logger::get_instance().append("ERROR", format, ##__VA_ARGS__)
#define LOG_DEBUG(format, ...) Logger::get_instance().append("DEBUG", format, ##__VA_ARGS__)
#define LOG_DIRECT(msg) Logger::get_instance().append(msg)


class Logger;
extern Logger &httpLogger; // 单例全局变量

class Logger
{
public:
    ~Logger();
    Logger(const Logger &) = delete;            // 禁止赋值构造
    Logger &operator=(const Logger &) = delete; // 禁止拷贝构造
    static Logger &get_instance()
    {
        static Logger instance;
        return instance;
    }
    static void *writeLog(void *args);                         // 写日志线程运行的函数
    bool append(std::string);                                  // 添加一条log, 最简单的版本
    bool append(std::string logType, const char *format, ...); // 添加一条log, 它包含更复杂的格式

private:
    Logger(const char *filename = "../server.log");
    void runConsumer();
    std::queue<std::string> logQueue; // 用于存储将要写入的log
    locker logQueueLock;              // 用于保护队列的锁
    sem logQueueStat;                 // 用于同步: 通知写
    bool m_stop;
    char *m_buf; // 用于存储log的缓冲 (append的重载函数使用)
    FILE *m_fp;  // 打开log的文件指针
};

#endif