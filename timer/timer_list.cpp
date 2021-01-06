
#include"timer_list.h"
#include<assert.h>
#include<errno.h>
#include"../http_con/http_con.h"


void cb_func(void *ipClientData)
{
    client_data*pData=(client_data *)ipClientData;
    assert(pData);
    epoll_ctl(Utils::m_iEpollFd,EPOLL_CTL_DEL,pData->sockfd,0);
    close(pData->sockfd);
    HttpCon::m_iUserCount--;
}

TimerList::TimerList()
{
    m_pHead=NULL;
    m_pTail=NULL;
}

int TimerList::addTimer(Timer *ipTimer)
{
    if (!ipTimer)
    {
        return 1;
    }
    if (!m_pHead)
    {
        m_pHead = m_pTail = ipTimer;
        return 0;
    }
    if (ipTimer->expire_time < m_pHead->expire_time)
    {
        ipTimer->next = m_pHead;
        m_pHead->prev = ipTimer;
        m_pHead = ipTimer;
        return 0;
    }
    return addTimer(ipTimer, m_pHead);
}

int TimerList::addTimer(Timer *ipTimer,Timer *ipPos)
{
    Timer *prev = ipPos;
    Timer *tmp = prev->next;
    while (tmp)
    {
        if (ipTimer->expire_time < tmp->expire_time)
        {
            prev->next = ipTimer;
            ipTimer->next = tmp;
            tmp->prev = ipTimer;
            ipTimer->prev = prev;
            break;
        }
        prev = tmp;
        tmp = tmp->next;
    }
    if (!tmp)
    {
        prev->next = ipTimer;
        ipTimer->prev = prev;
        ipTimer->next = NULL;
        m_pTail = ipTimer;
    }
    return 0;
}

int TimerList::adjustTimer(Timer *ipTimer)
{
    if (!ipTimer)
    {
        return 1;
    }
    Timer *tmp = ipTimer->next;
    if (!tmp || (ipTimer->expire_time < tmp->expire_time))
    {
        return 0;
    }
    if (ipTimer == m_pHead)
    {
        m_pHead = m_pHead->next;
        m_pHead->prev = NULL;
        ipTimer->next = NULL;
        addTimer(ipTimer, m_pHead);
    }
    else
    {
        ipTimer->prev->next = ipTimer->next;
        ipTimer->next->prev = ipTimer->prev;
        addTimer(ipTimer, ipTimer->next);
    }
    return 0;
}

int TimerList::delTimer(Timer *ipTimer)
{
    if(ipTimer==NULL) return 1;
    if(ipTimer==m_pHead)
    {
        m_pHead=ipTimer->next;
        if(m_pHead)
        {
            m_pHead->prev=NULL;
        }
    }
    else if(ipTimer==m_pTail)
    {
        m_pTail=ipTimer->prev;
        if(m_pTail)
        {
            m_pTail->next=NULL;
        }
    }
    else
    {
        ipTimer->prev->next=ipTimer->next;
        ipTimer->next->prev=ipTimer->prev;
    }
    delete ipTimer;
    return 0;
}

int TimerList::tick()
{
    if(m_pHead==NULL)
    {
        return 0;
    }
    struct timeval now;
    gettimeofday(&now,NULL);
    size_t nowMs=now.tv_sec*1000+now.tv_usec/1000;
    Timer *pCur=m_pHead;
    int count=0;
    while(pCur&&pCur->expire_time<=nowMs)
    {
        pCur->cb_func(pCur->user_data);
        count++;
        Timer *pTemp=pCur;
        pCur=pCur->next;
        delete pTemp;
    }
    return count;
}


int *Utils::m_pPipeFd=NULL;
int Utils::m_iEpollFd=0;

int Utils::init(int iTimeSlot)
{
    m_iTimeSlot=iTimeSlot;
}

int Utils::setNonblocking(int iFd)
{
    int oldMode=fcntl(iFd,F_GETFL);
    int newMode=oldMode|O_NONBLOCK;
    fcntl(iFd,F_SETFL,newMode);
    return oldMode;
}

int Utils::addFd(int iEpollFd,int iSockFd,bool ibOneShot,int iTrigMode)
{
    struct epoll_event event;
    event.data.fd=iSockFd;
    event.events=EPOLLIN|EPOLLRDHUP;
    if(ibOneShot)
    {
        event.events|=EPOLLONESHOT;
    }
    if(1==iTrigMode)
    {
        event.events|=EPOLLET;
    }
    int ret=epoll_ctl(iEpollFd,EPOLL_CTL_ADD,iSockFd,&event);
    setNonblocking(iSockFd);
    return 0;
}

void Utils::sig_handler(int iSig)
{
    int oldErrNo=errno;
    int msg=iSig;
    send(*m_pPipeFd,(char *)&msg,1,0);
    errno=oldErrNo;
} 

int Utils::addSig(int iSig,void (*handler)(int),bool ibRestart)
{
    struct sigaction act;
    memset(&act,0,sizeof(act));
    act.sa_handler=handler;
    if(ibRestart)
    {
        act.sa_flags|=SA_RESTART;
    }
    sigfillset(&act.sa_mask);
    assert(sigaction(iSig,&act,NULL)!=-1);
    return 0;
}

int Utils::timerHandler()
{
    m_timerList.tick();
    alarm(m_iTimeSlot);
}

int Utils::showError(int iFd,const char *ipInfo)
{
    send(iFd,ipInfo,strlen(ipInfo),0);
    close(iFd);
    return 0;
}

