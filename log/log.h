
#ifndef INCLUDE_LOG_H
#define INCLUDE_LOG_H

#include<pthread.h>
#include<string.h>
#include<string>
#include<time.h>
#include<stdio.h>
#include"cycle_queue.h"

#define MAX_NAME_LENGTH 256

using std::string;

enum log_level
{
    log_debug=0,
    log_warn,
    log_info,
    log_error
};

class Log
{
public:
    //饿汉模式，程序结束时自动释放静态对象
    static Log *getInstance()
    {
        static Log log;
        return &log;
    }
    int init(const char *ipLogDir,int iCloseLog,int iLogBufSize=8192,int iMaxLogCount=100000,int iMaxAsyncCount=0);
    int writeLog(log_level iLevel,const char *ipFormat,...);
    int flush();
private:
    static void *AsyncLogThreadFunc(void * ipArg)
    {
        Log::getInstance()->asyncWriteLog();
    }
    int asyncWriteLog()
    {
        string strFrontLog;
        while(m_pLogQueue->pop(strFrontLog))
        {
            m_fpLock.lock();
            fputs(strFrontLog.c_str(),m_pLogFp);
            m_fpLock.unlock();
        }
    }
    Log();
    ~Log();
private:
    char m_pLogDir[MAX_NAME_LENGTH];     //日志文件目录
    int m_iLogCount;                     //当前日志文件日志条数
    int m_iMaxLogCount;                  //每个日志文件最多日志数
    FILE *m_pLogFp;                      //日志文件对象
    int m_iMaxAsyncCount;                //<=0时同步写日志，>0时异步写日志
    char *m_pLogBuf;                     //日志文件缓冲区
    int m_iLogBufSize;                   //日志文件缓冲区大小
    CycleQueue<string>*m_pLogQueue;      //异步日志队列
    Locker m_fpLock;                     //文件的保护锁
    int m_iCloseLog;                     //日志是否关闭
};

#define LOG_DEBUG(format,...) do{if(0==m_iCloseLog) {Log::getInstance()->writeLog(log_debug,format,##__VA_ARGS__);Log::getInstance()->flush();}}while(0)
#define LOG_WARN(format,...) do{if(0==m_iCloseLog) {Log::getInstance()->writeLog(log_warn,format,##__VA_ARGS__);Log::getInstance()->flush();}}while(0)
#define LOG_ERROR(format,...) do{if(0==m_iCloseLog) {Log::getInstance()->writeLog(log_error,format,##__VA_ARGS__);Log::getInstance()->flush();}}while(0)
#define LOG_INFO(format,...) do{if(0==m_iCloseLog) {Log::getInstance()->writeLog(log_info,format,##__VA_ARGS__);Log::getInstance()->flush();}}while(0)
#endif //INCLUDE_LOG_H