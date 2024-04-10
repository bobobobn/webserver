#ifndef THREADPOOL
#define THREADPOOL
#include <pthread.h>
#include <cstdio>
#include <list>
#include <exception>
#include "lock/locker.h"

template<typename T>
class threadPool{
public:
    threadPool(int thread_max, int requests_max) : m_thread_max(thread_max), m_requests_max(requests_max), m_pool(nullptr), m_stop(false){
        if(thread_max <= 0 || requests_max <= 0){
            throw std::exception();
        }
        m_pool = new pthread_t[thread_max];
        if(!m_pool){
            throw std::exception();
        }
        for(int i = 0; i < m_thread_max; i++){
            if(pthread_create(m_pool + i, NULL, worker, this) != 0){
                delete[] m_pool;
                throw std::exception();
            }
            if(pthread_detach(*(m_pool+i)) != 0){
                delete[] m_pool;
                throw std::exception();
            }
        }
    }
    ~threadPool(){
        delete[] m_pool;
        m_stop = true;
    }
    bool append(T* request);

private:
    static void* worker(void * args);
    void run();

private:
    std::list<T*> m_workQueue;
    pthread_t * m_pool;
    locker m_queueLocker;
    sem m_queueSem;
    int m_thread_max;
    int m_requests_max;
    bool m_stop;
};

template<typename T>
void* threadPool<T>::worker(void * args){
    threadPool* pool = static_cast<threadPool*> (args);
    pool->run();
    return pool;
}

template<typename T>
void threadPool<T>::run(){
    while(!m_stop){
        this->m_queueSem.wait();
        this->m_queueLocker.lock();
        if(m_workQueue.empty()){
            m_queueLocker.unlock();
            continue;
        }
        T* request = m_workQueue.front();
        m_workQueue.pop_front();
        m_queueLocker.unlock();
        request->process();

    }
}

template<typename T>
bool threadPool<T>::append(T* request){
    m_queueLocker.lock();
    if(m_workQueue.size() > m_requests_max){
        m_queueLocker.unlock();
        return false;
    }
    m_workQueue.push_back(request);
    m_queueLocker.unlock();
    m_queueSem.post();
    return true;
}

#endif