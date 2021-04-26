#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "locker.h"

template<typename T>
class threadpool {
public:
    threadpool(int thread_number = 8, int max_requests = 10000);
    ~threadpool();
    bool append(T* request);
private:
    static void* worker(void* arg);
    void run();
private:
	// 线程池中的线程数
    int m_thread_number;
    // 请求队列允许的最大请求数
    int m_max_requests;
    // 线程池数组
    pthread_t* m_threads;
    // 请求队列
    std::list<T*> m_workqueue;
    // 互斥锁，用于读写请求队列时加锁保护
    locker m_queuelocker;
    // 信号量，用于标识任务数量
    sem m_queuestat;
    // 是否结束线程
    bool m_stop;
};

// 创建线程池，默认8个线程
template<typename T>
threadpool<T>::threadpool(int thread_number, int max_requests) : 
        m_thread_number(thread_number), m_max_requests(max_requests), m_stop(false), m_threads(NULL) {
    if ((thread_number <= 0) || (max_requests <= 0)) {
        throw std::exception();
    }
    m_threads = new pthread_t[m_thread_number];
    if (!m_threads) {
        throw std::exception();
    }
    for (int i = 0; i < thread_number; ++i) {
        printf("create the %dth thread\n", i);
        if (pthread_create(m_threads + i, NULL, worker, this) != 0) {
            delete[] m_threads;
            throw std::exception();
        }
        if (pthread_detach(m_threads[i])) {
            delete[] m_threads;
            throw std::exception();
        }
    }
}

template<typename T>
threadpool<T>::~threadpool() {
    delete[] m_threads;
    m_stop = true;
}

// 当任务到来时，将任务加入请求队列
template<typename T>
bool threadpool<T>::append(T* request) {
    m_queuelocker.lock();
    if (m_workqueue.size() > m_max_requests) {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

// 工作线程调用run函数来工作
template<typename T>
void* threadpool<T>::worker(void* arg) {
    threadpool* pool = (threadpool*)arg;
    pool->run();
    return pool;
}

// 每个工作线程循环处理各项工作
template<typename T>
void threadpool<T>::run() {
    while (!m_stop) {
        // 使用信号量避免惊群效应
        m_queuestat.wait();
        // 使用互斥锁避免多个线程同时修改队列造成不同步
        m_queuelocker.lock();
        if (m_workqueue.empty()) {
            m_queuelocker.unlock();
            continue;
        }
        T* request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if (!request) {
            continue;
        }
        // 调用封装好的请求的process函数
        request->process();
    }
}

#endif //THREADPOOL_H