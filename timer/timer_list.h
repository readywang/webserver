

#ifndef INCLUDE_TIMER_LIST_H
#define INCLUDE_TIMER_LIST_H

#include<sys/time.h>
#include<stddef.h>
#include<unistd.h>
#include<fcntl.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<sys/epoll.h>
#include<signal.h>
#include<netinet/ip.h>


//时间链表节点
typedef struct _timer
{
    _timer *next;
    _timer *prev;
    size_t expire_time;                 //超时时间,毫秒为单位
    void *user_data;                    //节点数据
    void (*cb_func)(void *ipClientData);  //超时回调函数
    _timer():next(NULL),prev(NULL){}
    _timer(size_t iMillisecond):next(NULL),prev(NULL)
    {
        struct timeval time;
        gettimeofday(&time,NULL);
        expire_time=time.tv_sec*1000+time.tv_usec/1000+iMillisecond;
    }
}Timer;

class TimerList
{
public:
    TimerList();
    /* 添加计时器节点，按照过期时间大小找到插入位置 */
    int addTimer(Timer *ipTimer);
    /* 调整定时器位置 */
    int adjustTimer(Timer *ipTimer);
    /* 指定节点后插入节点 */
    int addTimer(Timer *ipTimer,Timer *ipPos);
    /* 删除指定节点 */
    int delTimer(Timer *ipTimer);
    /* 心跳函数 */
    int tick();
private:
    Timer *m_pHead;
    Timer *m_pTail;
};

class Utils
{
public:
    Utils(){};
    ~Utils(){};
public:
    /* 初始化计时器最小刻度 */
    int init(int iTimeSlot);

    /* 设置套接字为非阻塞 */
    int setNonblocking(int iFd);

    /* 注册套接字监听事件 */
    int addFd(int iEpollFd,int iSockFd,bool ibOneShot,int iTrigMode);

    /* 信号处理函数 */
    static void sig_handler(int iSig);

    /* 添加信号处理函数 */
    int addSig(int iSig,void (*handler)(int),bool ibRestart=true);

    /* 定时处理函数 */
    int timerHandler();

    /* 发送错误信息 */
    int showError(int iFd,const char *ipInfo);
public:
    static int *m_pPipeFd;
    TimerList m_timerList;
    static int m_iEpollFd;
    int m_iTimeSlot;
};

typedef  struct _client_data
{
    int sockfd;
    struct sockaddr_in client_addr;
    Timer *pTimer; 
}client_data;

void cb_func(void *ipClientData);
#endif //INCLUDE_TIMER_LIST_H