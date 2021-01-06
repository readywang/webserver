
#ifndef INCLUDE_LOCKER_H
#define INCLUDE_LOCKER_H

#include<pthread.h>
#include<stdexcept>

class Locker
{
public:
    Locker()
    {
        if(pthread_mutex_init(&m_threadMutex,NULL)!=0)
        {
            throw std::runtime_error("error mutex init");
        }
    }
    ~Locker()
    {
        pthread_mutex_unlock(&m_threadMutex);
    }
    int lock()
    {
        if(pthread_mutex_lock(&m_threadMutex)==0)
        {
            return 0;
        }
        return 1;
    }
    int unlock()
    {
        if(pthread_mutex_unlock(&m_threadMutex)==0)
        {
            return 0;
        }
        return 1;
    }
    pthread_mutex_t* get()
    {
        return &m_threadMutex;
    }
private:
    pthread_mutex_t m_threadMutex;
};

#endif //INCLUDE_LOCKER_H