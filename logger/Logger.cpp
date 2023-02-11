#include "Logger.h"

Logger& httpLogger = Logger::get_instance();

Logger::~Logger()
{
    m_stop = true;
    if (m_fp != NULL)
    {
        fclose(m_fp);
    }
}

Logger::Logger(const char *filename)
{
    m_buf = (char*)malloc(SINGLE_LOG_BUFFER_SIZE);
    // 打开文件
    m_fp = fopen(filename, "a");
    if (m_fp == NULL)
    {
        free(m_buf);
        m_buf = NULL;
        printf("open %s failed!\n", filename);
        throw std::exception();
    }

    // 创建消费者线程来运行写日志的函数
    pthread_t writerThread;
    int ret = pthread_create(&writerThread, NULL, writeLog, this);
    if (ret != 0)
    {
        free(m_buf);
        m_buf = NULL;
        printf("create thread failed!\n");
        throw std::exception();
    }
}

bool Logger::append(std::string singleLog)
{
    logQueueLock.lock();
    logQueue.push(singleLog);
    logQueueLock.unlock();
    logQueueStat.post();
    return true;
}


bool Logger::append(std::string logType, const char *format, ...)
{
    logQueueLock.lock();
    /*获取包含多种信息的日志*/

    std::string singleLog;
    memset(m_buf, '\0', SINGLE_LOG_BUFFER_SIZE);

    auto now = std::chrono::system_clock::now();
    time_t tt = std::chrono::system_clock::to_time_t(now);
    auto time_tm = localtime(&tt);

    

    int n = snprintf(m_buf, 48, "[%d-%02d-%02d %02d:%02d:%02d] ",
                     time_tm->tm_year + 1900, time_tm->tm_mon + 1, time_tm->tm_mday,
                     time_tm->tm_hour, time_tm->tm_min, time_tm->tm_sec);

    // 加上微秒
    // struct timeval time_usec;
    // gettimeofday(&time_usec, NULL);
    // int n = snprintf(m_buf, 48, "[%d-%02d-%02d %02d:%02d:%02d.%05ld] ",
    //                  time_tm->tm_year + 1900, time_tm->tm_mon + 1, time_tm->tm_mday,
    //                  time_tm->tm_hour, time_tm->tm_min, time_tm->tm_sec, time_usec.tv_usec);

    va_list valst;
    va_start(valst, format);
    int m = vsnprintf(m_buf + n, SINGLE_LOG_BUFFER_SIZE - 1, format, valst);
    m_buf[n + m] = '\n';
    m_buf[n + m + 1] = '\0';
    va_end(valst);

    singleLog = m_buf;
    singleLog = "[" + logType + "] " + singleLog;

    logQueue.push(singleLog);
    logQueueLock.unlock();
    logQueueStat.post();
    return true;
}

void *Logger::writeLog(void *arg)
{
    Logger *this_ptr = (Logger *)arg;
    // 从队列中取出一条log来写
    this_ptr->runConsumer();
    return this_ptr;
}

void Logger::runConsumer()
{
    while (!m_stop)
    {
        logQueueStat.wait();
        logQueueLock.lock();
        while (logQueue.empty())
        {
            logQueueLock.unlock();
            continue;
        }
        std::string log = logQueue.front();
        logQueue.pop();
        // 写入并同时打印到控制台
        fputs(log.c_str(), m_fp);
        fflush(m_fp);
        std::cout << log;
        logQueueLock.unlock();
    }
}