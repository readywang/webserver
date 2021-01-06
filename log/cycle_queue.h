#ifndef INCLUDE_CYCLE_QUEUE_H
#define INCLUDE_CYCLE_QUEUE_H

#include<vector>
#include"../locker/locker.h"
#include"../cond/cond.h"

using std::vector;

template<typename T>
class CycleQueue
{
public:
    CycleQueue(int iSize);
    bool empty();
    bool full();
    int size();
    bool push(T iData);
    bool pop(T &oData);
    bool front(T &oData);
private:
    vector<T>m_vDataBlocks;  //循环队列数据块
    int m_iStartIndex;       //数据起始位置
    int m_iEndIndex;         //数据终止位置
    int m_iDataNum;          //存储的数据数目
    Locker m_queueLock;      //互斥锁，存取数据都要加锁
    Cond m_queueCond;        //条件变量，通知队列为空或者满
};

template<typename T>
bool CycleQueue<T>::full()
{
    bool isFull=false;
    m_queueLock.lock();
    if(m_iDataNum==m_vDataBlocks.size())
    {
        isFull=true;
    }
    m_queueLock.unlock();
    return isFull;
}

template<typename T>
bool CycleQueue<T>::empty()
{
    bool isEmpty=false;
    m_queueLock.lock();
    if(m_iDataNum==0)
    {
        isEmpty=true;
    }
    m_queueLock.unlock();
    return isEmpty;
}

template<typename T>
int CycleQueue<T>::size()
{
    int size=0;
    m_queueLock.lock();
    size=m_iDataNum;
    m_queueLock.unlock();
    return size;
}

template<typename T>
CycleQueue<T>::CycleQueue(int iSize):m_iStartIndex(0),m_iEndIndex(0)
{
    m_vDataBlocks.resize(iSize);
}

template<typename T>
bool CycleQueue<T>::push(T iData)
{
    m_queueLock.lock();
    int size=m_vDataBlocks.size();
    if(m_iDataNum==size)
    {
        m_queueCond.broadcast();
        m_queueLock.unlock();
        return false;
    }
    m_vDataBlocks[m_iEndIndex]=iData;
    m_iEndIndex=(m_iEndIndex+1)%size;
    m_iDataNum++;
    m_queueLock.unlock();
    return true;
}

template<typename T>
bool CycleQueue<T>::front(T &oData)
{
    m_queueLock.lock();
    if(m_iDataNum==0)
    {
        m_queueLock.unlock();
        return false;
    }
    oData=m_vDataBlocks[m_iStartIndex];
    m_queueLock.unlock();
    return true; 
}

template<typename T>
bool CycleQueue<T>::pop(T &oData)
{
    m_queueLock.lock();
    while(m_iDataNum<=0)
    {
        if (!m_queueCond.wait(m_queueLock.get()))
        {
            m_queueLock.unlock();
            return false;
        }
    }
    oData=m_vDataBlocks[m_iStartIndex];
    m_iStartIndex=(m_iStartIndex+1)%m_vDataBlocks.size();
    m_iDataNum--;
    m_queueLock.unlock();
    return true; 
}

#endif //INCLUDE_CYCLE_QUEUE_H