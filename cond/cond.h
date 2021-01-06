/*
 * @Author: your name
 * @Date: 2021-01-01 15:34:46
 * @LastEditTime: 2021-01-01 15:34:46
 * @LastEditors: your name
 * @Description: In User Settings Edit
 * @FilePath: /webserver/cond/cond.h
 */
#ifndef INCLUDE_COND_H
#define INCLUDE_COND_H

#include<pthread.h>
#include<stdexcept>

class Cond
{
public:
    Cond()
    {
        if(pthread_cond_init(&m_threadCond,NULL)!=0)
        {
            throw std::runtime_error("create thread cond failed");
        }
    }
    ~Cond()
    {
        pthread_cond_destroy(&m_threadCond);
    }
    int wait(pthread_mutex_t *ipMutex)
    {
        return pthread_cond_wait(&m_threadCond,ipMutex);
    }
    int timeWait(pthread_mutex_t iMutex,struct timespec iTime)
    {
        return pthread_cond_timedwait(&m_threadCond,&iMutex,&iTime);
    }
    int signal()
    {
        return pthread_cond_signal(&m_threadCond);
    }
    int broadcast()
    {
        return pthread_cond_broadcast(&m_threadCond);
    }
private:
    pthread_cond_t m_threadCond;
};

#endif // INCLUDE_COND_H