
#ifndef INCLUDE_WEBSERVER_H
#define INCLUDE_WEBSERVER_H

#include"../thread_pool/thread_pool.h"
#include"../timer/timer_list.h"
#include"../sql/sql_connection_pool.h"
#include"../http_con/http_con.h"
#include<sys/epoll.h>

const int MAX_FD=65536;             //最大文件描述符
const int MAX_EVENT_NUMBER=10000;   //最大事件数
const int TIMESLOT=5;               //最小超时单位

class WebServer
{
public:
    WebServer();
    ~WebServer();
    int init(int iPort,string istrUser,string istrPasswd,string istrDBName,int iLogWrite,int iOptLinger,int iTrigmode,
    int iSqlNum,int iThreadNum,int iCloseLog,int iActorModel);
    /* 初始化连接套接字和监听套接字的触发模式 */
    int initTrigMode();

    /* 初始化日志 */
    int initLog();

    /* 初始化数据库连接池 */
    int initConnPool();

    /* 初始化线程池 */
    int initThreadPool();

    /* 初始化事件循环 */
    int initEventLoop();

    /* 初始化定时器 */
    int initTimer(int iConnFd,struct sockaddr_in iAddr);

    /* 套接字上有数据传输时，定时 */
    int adjustTimer(Timer *ipTimer);

    /* 删除定时器 */
    int deleteTimer(Timer *ipTimer,int iFd);

    /* 服务器运行主函数 */
    int Run();
private:
    /* 处理客户端连接 */
    int dealClientConnect();

    /* 处理信号 */
    int dealwithSignal(bool &iTimeOut,bool &iStopServer);

    /* 处理读操作 */
    int dealwithRead(int iFd);

    /* 处理写操作 */
    int dealwithWrite(int iFd);
private:
    /* 基础 */
    int m_iPort;                        //端口号
    char *m_pDocRoot;                   //请求资源目录
    int m_iLogWrite;                    //写日志方式，0同步写 1异步写
    int m_iCloseLog;                    //是否关闭日志
    int m_iActorModel;                  //reactor或者preactor模型
    int m_iPipeFd[2];                   //传递信号管道
    int m_iEpollFd;                     //epoll套接字
    HttpCon *m_pUsers;                   

    /* 数据库相关 */
    SqlConnectionPool *m_pConnPool;     //连接池
    string m_strUser;                   //用户名
    string m_strPasswd;                 //密码
    string m_strDBName;                 //数据库名称
    int m_iSqlNum;                      //连接池数目

    /* 线程池相关 */
    ThreadPool<HttpCon> *m_pThreadPool; //线程池
    int m_iThreadNum;                   //线程池线程数

    /* epoll_event相关 */
    struct epoll_event m_pEvents[MAX_EVENT_NUMBER]; 
    int m_iListenFd;                    //监听套接字
    int m_iOptLinger;                   //优雅关闭连接
    int m_iTrigMode;                    //触发模式
    int m_iListenTrigMode;              //监听套接字触发模式
    int m_iConnTrigMode;                //连接套接字触发模式

    /* 客户端数据 */
    client_data *m_pClientData;
    Utils m_utils;
};

#endif // INCLUDE_WEBSERVER_H
