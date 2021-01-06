/*
 * @Description: 
 * @Author: wyl
 * @Date: 2020-12-21 11:32:13
 * @LastEditTime: 2020-12-22 10:48:58
 * @LastEditors: wyl
 * @Reference: 
 */

#include"webserver.h"
#include"../log/log.h"
#include<unistd.h>
#include<assert.h>
#include<errno.h>
#include<arpa/inet.h>

WebServer::WebServer()
{
    m_pUsers=new HttpCon[MAX_FD];
    //root文件夹路径
    char pCurDir[200]={0};
    getcwd(pCurDir,200);
    char pRoot[6]="/root";
    m_pDocRoot=(char *)malloc(strlen(pCurDir)+strlen(pRoot)+1);
    strcpy(m_pDocRoot,pCurDir);
    strcat(m_pDocRoot,pRoot);
    m_pClientData=new client_data[MAX_FD];
}

WebServer::~WebServer()
{
    close(m_iEpollFd);
    close(m_iListenFd);
    close(m_iPipeFd[0]);
    close(m_iPipeFd[1]);
    delete []m_pDocRoot;
    delete []m_pClientData;
    delete m_pThreadPool;
}

int WebServer::init(int iPort,string istrUser,string istrPasswd,string istrDBName,int iLogWrite,int iOptLinger,int iTrigmode,
    int iSqlNum,int iThreadNum,int iCloseLog,int iActorModel)
{
    m_iPort=iPort;
    m_strUser=istrUser;
    m_strPasswd=istrPasswd;
    m_strDBName=istrDBName;
    m_iLogWrite=iLogWrite;
    m_iOptLinger=iOptLinger;
    m_iTrigMode=iTrigmode;
    m_iSqlNum=iSqlNum;
    m_iThreadNum=iThreadNum;
    m_iCloseLog=iCloseLog;
    m_iActorModel=iActorModel;
}

int WebServer::initTrigMode()
{
    switch (m_iTrigMode)
    {
    case 0:
    {
        //LT+LT
        m_iListenTrigMode=0;
        m_iConnTrigMode=0;
        break;
    }
    case 1:
    {
        //LT+ET
        m_iListenTrigMode=0;
        m_iConnTrigMode=1;
        break;
    }
    case 2:

    {
        //ET+LT
        m_iListenTrigMode=1;
        m_iConnTrigMode=0;
        break;
    }
    case 3:
    {
        //ET+ET
        m_iListenTrigMode=1;
        m_iConnTrigMode=1;
        break;
    }
    default:
        break;
    }
    return 0;
}

int WebServer::initLog()
{
    int errorCode=0;
    if(0==m_iCloseLog)
    {
        if(1==m_iLogWrite)
        {
            //异步写日志
            errorCode=Log::getInstance()->init("./server_log",m_iCloseLog,2000,800000,800);
        }
        else
        {
            //同步写日志
            errorCode=Log::getInstance()->init("./server_log",m_iCloseLog,2000,800000,0);
        }
    }
    return errorCode;
}

int WebServer::initConnPool()
{
    //初始化连接池和数据库用户名和密码
    m_pConnPool=SqlConnectionPool::getInstance();
    m_pConnPool->init("localhost",m_strUser,m_strPasswd,m_strDBName,3306,m_iSqlNum,m_iCloseLog);
    m_pUsers->initSqlUsers(m_pConnPool);
    return 0;
}

int WebServer::initThreadPool()
{
    m_pThreadPool=new ThreadPool<HttpCon>(m_iActorModel,m_pConnPool,m_iThreadNum);
    return 0;
}

int WebServer::initEventLoop()
{
    //创建监听套接字
    m_iListenFd=socket(AF_INET,SOCK_STREAM,0);
    assert(m_iListenFd>=0);

    //优雅关闭连接
    if(0==m_iOptLinger)
    {
        struct linger tmp={0,1};
        setsockopt(m_iListenFd,SOL_SOCKET,SO_LINGER,&tmp,sizeof(tmp));
    }
    else
    {
        struct linger tmp={1,1};
        setsockopt(m_iListenFd,SOL_SOCKET,SO_LINGER,&tmp,sizeof(tmp));
    }
    //绑定地址。设置地址重复利用
    struct sockaddr_in addr;
    bzero(&addr,sizeof(addr));
    addr.sin_port=htons(m_iPort);
    addr.sin_addr.s_addr=htonl(INADDR_ANY);
    addr.sin_family=AF_INET;
    int flag=1;
    setsockopt(m_iListenFd,SOL_SOCKET,SO_REUSEADDR,&flag,sizeof(flag));
    int ret=bind(m_iListenFd,(struct sockaddr *)&addr,sizeof(addr));
    assert(ret>=0);
    ret = listen(m_iListenFd, 5);
    assert(ret >= 0);

    //设置定时间隔
    m_utils.init(TIMESLOT);

    //创建epoll对象
    m_iEpollFd=epoll_create(5);
    assert(m_iEpollFd>=0);

    //添加监听
    m_utils.addFd(m_iEpollFd,m_iListenFd,false,m_iListenTrigMode);
    HttpCon::m_iEpollFd=m_iEpollFd;
    //创建管道
    ret=socketpair(AF_UNIX,SOCK_STREAM,0,m_iPipeFd);
    assert(ret>=0);
    //注册信号处理函数
    m_utils.setNonblocking(m_iPipeFd[1]);
    m_utils.addFd(m_iEpollFd,m_iPipeFd[0],false,0);
    m_utils.addSig(SIGPIPE,SIG_IGN);
    m_utils.addSig(SIGALRM,m_utils.sig_handler,false);
    m_utils.addSig(SIGTERM,m_utils.sig_handler,false);
    alarm(TIMESLOT);
    Utils::m_iEpollFd=m_iEpollFd;
    Utils::m_pPipeFd=m_iPipeFd;
    return 0;
}

int WebServer::initTimer(int iConnFd,struct sockaddr_in iAddr)
{
    m_pUsers[iConnFd].init(iConnFd,iAddr,m_pDocRoot,m_iConnTrigMode,m_iCloseLog,m_strUser,m_strPasswd,m_strDBName);
    m_pClientData[iConnFd].sockfd=iConnFd;
    m_pClientData[iConnFd].client_addr=iAddr;
    Timer *pTimer=new Timer();
    pTimer->user_data=&(m_pClientData[iConnFd]);
    pTimer->cb_func=cb_func;
    struct timeval now;
    gettimeofday(&now,NULL);
    size_t nowMs=now.tv_sec*1000+now.tv_usec/1000;
    pTimer->expire_time=nowMs+3000*TIMESLOT;
    m_pClientData[iConnFd].pTimer=pTimer;
    m_utils.m_timerList.addTimer(pTimer);
    return 0;
}

int WebServer::adjustTimer(Timer *ipTimer)
{
    struct timeval now;
    gettimeofday(&now,NULL);
    size_t nowMs=now.tv_sec*1000+now.tv_usec/1000;
    ipTimer->expire_time=nowMs+3000*TIMESLOT;
    m_utils.m_timerList.adjustTimer(ipTimer);
    LOG_INFO("%s","adjust timer once");
    return 0;
}

int WebServer::deleteTimer(Timer *ipTimer,int iFd)
{
    ipTimer->cb_func(&m_pClientData[iFd]);
    m_utils.m_timerList.delTimer(ipTimer);
    LOG_INFO("close fd %d",iFd);
    return 0;
}

int WebServer::Run()
{
    bool stopServer=false;
    bool timeOut=false;
    while(!stopServer)
    {
        int num=epoll_wait(m_iEpollFd,m_pEvents,MAX_EVENT_NUMBER,-1);
        if(num<0&&errno!=EINTR)
        {
            LOG_ERROR("%s%s","epoll failture:",strerror(errno));
            break;
        }
        for(int i=0;i<num;++i)
        {
            int sockfd=m_pEvents[i].data.fd;
            if(sockfd==m_iListenFd)
            {
                //处理新连接
                if(dealClientConnect()!=0)
                {
                    continue;
                }
            }
            else if(m_pEvents[i].events&(EPOLLRDHUP|EPOLLHUP|EPOLLERR))
            {
                //关闭连接移除定时器
                Timer *pTimer=m_pClientData[sockfd].pTimer;
                deleteTimer(pTimer,sockfd);
            }
            else if(sockfd==m_iPipeFd[0]&&(m_pEvents[i].events&EPOLLIN))
            {
                if(dealwithSignal(timeOut,stopServer)!=0)
                {
                    LOG_ERROR("%s","dealwith signal failture");
                }
            }
            else if(m_pEvents[i].events&EPOLLIN)
            {
                dealwithRead(sockfd);
            }
            else if(m_pEvents[i].events&EPOLLOUT)
            {
                dealwithWrite(sockfd);
            }
        }
        if(timeOut)
        {
            m_utils.m_timerList.tick();
            LOG_INFO("%s","timer list tick");
            timeOut=false;
        }
    }
    return 0;
}

int WebServer::dealClientConnect()
{
    struct sockaddr_in cliAddr;
    socklen_t len=sizeof(cliAddr);
    if(0==m_iListenTrigMode)
    {
        int confd=accept(m_iListenFd,(struct sockaddr *)&cliAddr,&len);
        if(confd<0)
        {
            LOG_ERROR("%s%s","accept failtrue:",strerror(errno));
            return 1;
        }
        if(HttpCon::m_iUserCount>MAX_FD)
        {
            LOG_ERROR("%s","too many client");
            m_utils.showError(confd,"server busy");
            return 1;
        }
        initTimer(confd,cliAddr);
    }
    else 
    {
        while(true)
        {
            int confd=accept(m_iListenFd,(struct sockaddr *)&cliAddr,&len);
            if(confd<0)
            {
                LOG_ERROR("%s","accept failtrue");
                break;
            }
            if(HttpCon::m_iUserCount>MAX_FD)
            {
                LOG_ERROR("%s","too many client");
                m_utils.showError(confd,"server busy");
                break;
            }
            initTimer(confd,cliAddr);
        }
        return 1;
    }
    return 0;
}

int WebServer::dealwithSignal(bool &iTimeOut,bool &iStopServer)
{
    char pSignals[100]={0};
    int ret=recv(m_iPipeFd[0],pSignals,sizeof(pSignals),0);
    if(ret<=0)
    {
        return false;
    }
    else 
    {
        for(int i=0;i<ret;++i)
        {
            switch (pSignals[i])
            {
            case SIGTERM:
            {
                iStopServer=true;
                break;
            }
            case SIGALRM:
            {
                iTimeOut=true;
                break;
            }
            default:
                break;
            }
        }
    }
    return true;
}

int WebServer::dealwithRead(int iFd)
{
    Timer *pTimer=m_pClientData[iFd].pTimer;
    if(1==m_iActorModel)
    {
        //reactor
        if(pTimer)
        {
            adjustTimer(pTimer);
        }
        m_pThreadPool->addTask(&m_pUsers[iFd],0);
        while(true)
        {
            if(1==m_pUsers[iFd].m_improv)
            {
                if(1==m_pUsers[iFd].m_timer_flag)
                {
                    deleteTimer(pTimer,iFd);
                    m_pUsers[iFd].m_timer_flag=0;
                }
                m_pUsers[iFd].m_improv=0;
                break;
            }
        }
    }
    else
    {
        //proactor
        if(m_pUsers[iFd].readOnce()==0)
        {
            LOG_INFO("deal with client(%s)",inet_ntoa(m_pUsers[iFd].getAddress()->sin_addr));
            m_pThreadPool->addTask_p(&m_pUsers[iFd]);
            if(pTimer)
            {
                adjustTimer(pTimer);
            }
        }
        else
        {
            deleteTimer(pTimer,iFd);
        }
    }
    return 0;
}

int WebServer::dealwithWrite(int iFd)
{
    Timer *pTimer=m_pClientData[iFd].pTimer;
    if(1==m_iActorModel)
    {
        //reactor
        if(pTimer)
        {
            adjustTimer(pTimer);
        }
        m_pThreadPool->addTask(&m_pUsers[iFd],1);
        while(true)
        {
            if(1==m_pUsers[iFd].m_improv)
            {
                if(1==m_pUsers[iFd].m_timer_flag)
                {
                    deleteTimer(pTimer,iFd);
                    m_pUsers[iFd].m_timer_flag=0;
                }
                m_pUsers[iFd].m_improv=0;
                break;
            }
        }
    }
    else
    {
        //proactor
        if(m_pUsers[iFd].writeV()==0)
        {
            LOG_INFO("send data to client(%s)",inet_ntoa(m_pUsers[iFd].getAddress()->sin_addr));
            m_pThreadPool->addTask_p(&m_pUsers[iFd]);
            if(pTimer)
            {
                adjustTimer(pTimer);
            }
        }
        else
        {
            deleteTimer(pTimer,iFd);
        }
    }
    return 0;
}