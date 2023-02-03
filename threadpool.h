#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include <string>
#include "locker.h"



template <typename T>
class threadpool
{
public:
    threadpool(int thread_number = 2, int max_requests = 10000);
    ~threadpool();
    bool append(T *request); // 添加请求

private:
    static void *worker(void *arg); // 成员函数作为thread运行函数, 必须是static
    void run(int thread_no);

private:
    int m_thread_number;
    int m_max_requests;
    pthread_t *m_threads;       // 线程池的线程
    std::list<T *> m_workqueue; // 请求队列
    locker m_queuelocker;       // 保护请求队列的锁
    sem m_queuestat;            // 用于同步: 被通知queue非空的时候才拿request出来
    bool m_stop;
    int temp_thread_number;
};

template <typename T>
threadpool<T>::threadpool(int thread_number, int max_requests)
    : m_thread_number(thread_number), m_max_requests(max_requests), m_stop(false), m_threads(NULL)
{
    if ((thread_number <= 0) || (max_requests <= 0))
        throw std::exception();
    m_threads = new pthread_t[m_thread_number];
    if (!m_threads)
        throw std::exception();
    // 创建线程
    for (int i = 0; i < m_thread_number; i++)
    {
        printf("create the %dth thread\n", i);        
        this->temp_thread_number = i;
        if (pthread_create(m_threads + i, NULL, worker, this) != 0) // 传this, static函数内能调用
        {
            delete[] m_threads;
            throw std::exception();
        }
        // const char* thread_name = std::to_string(i).c_str();
        // if (pthread_setname_np(*(m_threads + i), thread_name) != 0) // 设置名字
        // {
        //     delete[] m_threads;
        //     throw std::exception();
        // }
        if (pthread_detach(m_threads[i])) // 与主控线程分离
        {
            delete[] m_threads;
            throw std::exception();
        }
        usleep(100000);
    }
}


template <typename T>
threadpool<T>::~threadpool()
{
    delete[] m_threads;
    m_stop = true;
}

template <typename T>
bool threadpool<T>::append(T *request)
{
    m_queuelocker.lock();
    if (m_workqueue.size() > m_max_requests)
    {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

template <typename T>
void *threadpool<T>::worker(void *arg)
{   
    
    threadpool *this_ptr = (threadpool *)arg; // 先获取this指针
    int thread_no = this_ptr->temp_thread_number; // 获取线程号
    this_ptr->run(thread_no);                          // 运行线程
    return this_ptr;
}

template <typename T>
void threadpool<T>::run(int thread_no)
{
    while (!m_stop)
    {
        // 线程通过竞争锁来处理请求
        printf("No %d thread process waiting for signal\n", thread_no);
        m_queuestat.wait(); // 等待有信号
        m_queuelocker.lock();
        if (m_workqueue.empty())
        {
            m_queuelocker.unlock();
            continue;
        }
        T *request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if (!request)
        {
            continue;
        }
        printf("No %d thread process\n", thread_no);
        request->process(); // 进行处理
    }
    printf("No %d thread process END!\n", thread_no);
}

#endif