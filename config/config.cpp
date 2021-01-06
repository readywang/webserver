/*
 * @Description: 
 * @Author: wyl
 * @Date: 2021-01-06 17:19:16
 * @LastEditTime: 2021-01-06 17:19:17
 * @LastEditors: wyl
 * @Reference: 
 */
#include"config.h"
#include<stdlib.h>
#include<unistd.h>

Config::Config()
{
    m_iPort=9006;           //端口号默认9006
    m_iLogWrite=0;          //同步写日志
    m_iTrigMode=0;          //组合触发模式 LT+LT
    m_iListenTrigMode=0;    //监听套接字触发模式LT
    m_iConnTrigMode=0;      //连接套接字触发模式LT
    m_iOptLinger=0;         //优雅关闭连接,采用默认
    m_iSqlNum=8;            //连接池连接数目默认8
    m_iThreadNum=8;         //线程池线程数目默认8
    m_iCloseLog=0;          //默认不关闭关闭日志
    m_iActorModel=0;        //并发模型，默认proactor
}

void Config::parseArg(int argc,char *argv[])
{
    int opt=0;
    const char *pStr="p:l:m:o:s:t:c:a:";
    while((opt=getopt(argc,argv,pStr))!=-1)
    {
        switch (opt)
        {
        case 'p':
        {
            m_iPort=atoi(optarg);break;
        }
        case 'l':
        {
            m_iLogWrite=atoi(optarg);break;
        }
        case 'm':
        {
            m_iTrigMode=atoi(optarg);break;
        }
        case 'o':
        {
            m_iOptLinger=atoi(optarg);break;
        }
        case 's':
        {
            m_iSqlNum=atoi(optarg);break;
        }
        case 't':
        {
            m_iThreadNum=atoi(optarg);break;
        }
        case 'c':
        {
            m_iCloseLog=atoi(optarg);break;
        }
        case 'a':
        {
            m_iActorModel=atoi(optarg);break;
        }
        default:
            break;
        }
    }
}