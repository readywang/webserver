#include"log.h"
#include<stdarg.h>
#include<sys/unistd.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<sys/time.h>


/* 工具函数，若目录不存在，则逐层创建 */
int createDir(const char *ipDir)
{
    if(NULL==ipDir) return 1;
    string strDir=ipDir;
    int index=1;
    while(index!=string::npos&&index<strDir.size())
    {
        index=strDir.find('/',index);
        string strTemp;
        if(index==string::npos)
        {
            strTemp=strDir;
        }
        else
        {
            strTemp=strDir.substr(0,index);
            index++;
        }
        if(access(strTemp.c_str(),F_OK)!=0)
        {
            if(mkdir(strTemp.c_str(),0777)!=0)
            {
                return 1;
            }
        }
    }
    return 0;
}

Log::Log()
{

}

Log::~Log()
{
    if(m_pLogQueue)
    {
        delete m_pLogQueue;
    }
    if(m_pLogFp)
    {
        fclose(m_pLogFp);
    }
    if(m_pLogBuf)
    {
        delete []m_pLogBuf;
    }
}

int Log::init(const char *ipLogDir,int iCloseLog,int iLogBufSize,int iMaxLogCount,int iMaxAsyncCount)
{
    if(iMaxAsyncCount>0)
    {
        //说明是异步写日志
        m_pLogQueue =new CycleQueue<string>(iMaxAsyncCount);
        if(m_pLogQueue==NULL)
        {
            return 1;
        }
        pthread_t tid;
        if(pthread_create(&tid,NULL,AsyncLogThreadFunc,NULL)!=0)
        {
            return 1;
        } 
    }
    m_iLogBufSize=iLogBufSize;
    m_pLogBuf=new char[m_iLogBufSize];
    if(m_pLogBuf==NULL)
    {
        return 1;
    }
    memset(m_pLogBuf,0,m_iLogBufSize);
    m_iMaxLogCount=iMaxLogCount;
    m_iMaxAsyncCount=iMaxAsyncCount;
    m_iLogCount=0;
    strcpy(m_pLogDir,ipLogDir);
    m_iCloseLog=iCloseLog;

    //创建日志文件路径
    if(createDir(m_pLogDir)!=0)
    {
        printf("log dir isn't exist!\n");
        return 1;
    }

    //根据当前时间创建日志文件
    time_t now=time(NULL);
    struct tm *sys_tm=localtime(&now);

    char pFullPath[MAX_NAME_LENGTH]={0};
    snprintf(pFullPath,MAX_NAME_LENGTH,"%s%s%d_%02d_%02d_%02d_%02d_%02d%s",ipLogDir,"/",
    1900+sys_tm->tm_year,1+sys_tm->tm_mon,sys_tm->tm_mday,sys_tm->tm_hour,sys_tm->tm_min,sys_tm->tm_sec,".log");
    m_pLogFp=fopen(pFullPath,"a");
    if(m_pLogFp==NULL)
    {
        return 1;
    }
    return 0;
}

int Log::writeLog(log_level iLevel,const char *ipFormat,...)
{
    struct timeval tv;
    gettimeofday(&tv,NULL);
    time_t now=tv.tv_sec;
    struct tm *sys_tm=localtime(&now);
    if(m_iLogCount>=m_iMaxLogCount)
    {
        //若达到了最大日志条数，则打开一个新的文件
        char pFullPath[MAX_NAME_LENGTH]={0};
        snprintf(pFullPath,MAX_NAME_LENGTH,"%s%s%d_%02d_%02d_%02d_%02d_%02d%s",m_pLogDir,"/",
        1900+sys_tm->tm_year,1+sys_tm->tm_mon,sys_tm->tm_mday,sys_tm->tm_hour,sys_tm->tm_min,sys_tm->tm_sec,".log");
        m_fpLock.lock();
        if(m_pLogFp)
        {
            fclose(m_pLogFp);
        }
        m_pLogFp=fopen(pFullPath,"a");
        m_fpLock.unlock();
        if(m_pLogFp==NULL)
        {
            return 1;
        }
        m_iLogCount=0;
    }
    //根据日志级别写日志开头
    char pLogHead[10]={0};
    switch (iLevel)
    {
    case log_debug:
        strcpy(pLogHead,"[debug]:");
        break;
    case log_warn:
        strcpy(pLogHead,"[warn]:");
        break;
    case log_info:
        strcpy(pLogHead,"[info]:");
        break;
    case log_error:
        strcpy(pLogHead,"[error]:");
        break;
    default:
        break;
    }

    //日志头加上时间
    va_list ap;
    va_start(ap,ipFormat);
    int m=snprintf(m_pLogBuf,m_iLogBufSize,"%d-%02d-%02d %02d:%02d:%02d.%06ld %s",1900+sys_tm->tm_year,
    1+sys_tm->tm_mon,sys_tm->tm_mday,sys_tm->tm_hour,sys_tm->tm_min,sys_tm->tm_sec,tv.tv_usec,pLogHead);
    if(m<=0)
    {
        return 1;
    }
    int n=vsnprintf(m_pLogBuf+m,m_iLogBufSize,ipFormat,ap);
    if(n<=0)
    {
        return 1;
    }
    m_pLogBuf[m+n]='\n';
    m_pLogBuf[m+n+1]='\0';
    //异步或者同步写日志
    if(m_iMaxAsyncCount>0&&!m_pLogQueue->full())
    {
        m_pLogQueue->push(m_pLogBuf);
    }
    else
    {
        m_fpLock.lock();
        fputs(m_pLogBuf,m_pLogFp);
        m_fpLock.unlock();
    }
    return 0;
}

int Log::flush()
{
    m_fpLock.lock();
    fflush(m_pLogFp);
    m_fpLock.unlock();
}