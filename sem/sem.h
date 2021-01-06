/*
 * @Description: 
 * @Author: wyl
 * @Date: 2020-12-14 11:32:39
 * @LastEditTime: 2020-12-14 11:32:39
 * @LastEditors: wyl
 * @Reference: 
 */

#ifndef INCLUDE_SEM_H
#define INCLUDE_SEM_H

#include<semaphore.h>
#include<stdexcept>

class Sem
{
public:
    Sem()
    {
        if(sem_init(&m_sem,0,0)!=0)
        {
            throw std::runtime_error("init sem error!");
        }
    }
    Sem(int iNum)
    {
        if(sem_init(&m_sem,0,iNum)!=0)
        {
            throw std::runtime_error("init sem error!");
        }
    }
    ~Sem()
    {
        sem_destroy(&m_sem);
    }
    int semWait()
    {
        return sem_wait(&m_sem);
    }
    int semPost()
    {
        return sem_post(&m_sem);
    }
private:
    sem_t m_sem;             //信号量集
};
#endif //INCLUDE_SEM_H
