#ifndef INCLUDE_THREAD_POOL_H
#define INCLUDE_THREAD_POOL_H

#include<queue>
#include<unistd.h>
#include<stdexcept>
#include"../locker/locker.h"
#include"../cond/cond.h"
#include"../sql/sql_connection_pool.h"

#define MAX_THREAD_NUM 100
#define MAX_TASK_NUM 1024

template<typename T>
class ThreadPool
{
public:
    ThreadPool(int iActorModel,SqlConnectionPool*ipConnPool,int iThreadNum=8,int iMaxTaskNum=10000);
    ~ThreadPool();
    int addTask(T *ipTask,int iState);
    int addTask_p(T *ipTask);
    bool isStop(){return m_stop;}
    int stop();
private:
    //每个线程调用函数静态方便调用
    static void *thread_start(void *arg);

    //之所以用thread_start调用threadRun是因为静态函数内部无法直接访问数据成员
    int threadRun();
private:
    int m_iActorModel;              //preactor或者reactor模型
    SqlConnectionPool *m_pConnPool; //数据库连接池
    int m_iThreadNum;               //线程总数
    pthread_t *m_pThreads;          //线程池所有线程
    int m_iMaxTaskNum;              //任务队列中最多任务数
    std::queue<T *>m_taskQueue;     //任务队列FIFO
    Locker m_queueLocker;           //任务队列锁
    Cond m_queueCond;               //任务队列条件变量
    bool m_stop;                    //线程是否要终止
    int m_iCloseLog;
};

template<typename T>
ThreadPool<T>::ThreadPool(int iActorModel,SqlConnectionPool*ipConnPool,int iThreadNum,int iMaxTaskNum)
:m_iActorModel(iActorModel),m_pConnPool(ipConnPool),m_stop(false)
{
    m_iCloseLog=0;
    if(iThreadNum<=0)
    {
        throw std::invalid_argument("error thread num");
    }
    if(iMaxTaskNum<=0)
    {
        throw std::invalid_argument("error max task num");
    }
    if(iThreadNum>MAX_THREAD_NUM)
    {
        iThreadNum=MAX_THREAD_NUM;
    }
    if(iMaxTaskNum>MAX_TASK_NUM)
    {
        iMaxTaskNum=MAX_TASK_NUM;
    }
    m_iThreadNum=iThreadNum;
    m_iMaxTaskNum=iMaxTaskNum;
    m_pThreads=new pthread_t[m_iThreadNum];
    if(m_pThreads==NULL)
    {
        throw std::runtime_error("free ram not enough");
    }
    //创建线程并且分离，这样子线程终止时可以自动回收资源
    for(int i=0;i<m_iThreadNum;++i)
    {
        if(pthread_create(&m_pThreads[i],NULL,thread_start,(void *)this)!=0)
        {
            delete []m_pThreads;
            throw std::runtime_error("create thread failed");
        }
        if(pthread_detach(m_pThreads[i])!=0)
        {
            delete []m_pThreads;
            throw std::runtime_error("detach thread failed");
        }
    }
}

template<typename T>
int ThreadPool<T>::stop()
{
    //通知所有线程
    m_stop=true;
    m_queueCond.broadcast();
}

template<typename T>
ThreadPool<T>::~ThreadPool()
{
    delete []m_pThreads;
}

template<typename T>
void * ThreadPool<T>::thread_start(void *arg)
{
    ThreadPool *pPool=(ThreadPool *)(arg);
    pPool->threadRun();
    return pPool;
}

template<typename T>
int ThreadPool<T>::threadRun()
{
    LOG_DEBUG("%d%s",pthread_self()," is runing...");
    while(true)
    {
        m_queueLocker.lock();
        //while循环判断任务队列是否为空或者线程池是否终止，防止假唤醒
        LOG_DEBUG("%d%s",pthread_self()," get lock...");
        while(m_taskQueue.empty()&&m_stop==false)
        {
            LOG_DEBUG("%d%s",pthread_self()," wait, release lock...");
            m_queueCond.wait(m_queueLocker.get());
        }
        //若是线程池终止，则跳出循环
        if(m_stop==true)
        {
            m_queueLocker.unlock();
            break;
        }
        T *pFrontTask=m_taskQueue.front();
        m_taskQueue.pop();
        //取出任务后先解锁再执行任务回调函数
        m_queueLocker.unlock();
        if(!pFrontTask)
        {
            continue;
        }
        sleep(5);
        if(1==m_iActorModel)
        {
            if(0==pFrontTask->m_iState)
            {
                if(pFrontTask->readOnce()==0)
                {
                    pFrontTask->m_improv=1;
                    ConnectionRAII connRaii(&pFrontTask->m_pMysql,m_pConnPool);
                    pFrontTask->process();
                }
                else
                {
                    pFrontTask->m_improv=1;
                    pFrontTask->m_timer_flag=1;
                }
            }
            else
            {
                if(pFrontTask->writeV()==0)
                {
                    pFrontTask->m_improv=1;
                }
                else
                {
                    pFrontTask->m_improv=1;
                    pFrontTask->m_timer_flag=1;
                }
            }
        }
        else
        {
            ConnectionRAII connRaii(&pFrontTask->m_pMysql,m_pConnPool);
            pFrontTask->process();
        }
    }
}

template<typename T>
int ThreadPool<T>::addTask(T*ipTask,int iState)
{
    m_queueLocker.lock();
    if(m_taskQueue.size()>m_iMaxTaskNum)
    {
        m_queueLocker.unlock();
        return 1;
    }
    ipTask->m_iState = iState;
    m_taskQueue.push(ipTask);
    m_queueLocker.unlock();
    //放入任务以后，唤醒一个工作线程
    if(m_queueCond.signal()==0)
    {
        return 0;
    }
    return 1;
}

template<typename T>
int ThreadPool<T>::addTask_p(T*ipTask)
{
    m_queueLocker.lock();
    if(m_taskQueue.size()>m_iMaxTaskNum)
    {
        m_queueLocker.unlock();
        return 1;
    }
    m_taskQueue.push(ipTask);
    m_queueLocker.unlock();
    //放入任务以后，唤醒一个工作线程
    if(m_queueCond.signal()==0)
    {
        return 0;
    }
    return 1;
}

#endif 