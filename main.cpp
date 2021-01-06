#include<stdio.h>
#include"./webserver/webserver.h"
#include"./config/config.h"
int main(int argc, char *argv[])
{
    //设置数据库信息
    string strUser="root";
    string strPasswd="wyl1314";
    string strDBName="love";

    //解析参数
    Config config;
    config.parseArg(argc,argv);
    WebServer server;

    //初始化server 
    server.init(config.m_iPort,strUser,strPasswd,strDBName,config.m_iLogWrite,config.m_iOptLinger,
    config.m_iTrigMode,config.m_iSqlNum,config.m_iThreadNum,config.m_iCloseLog,config.m_iActorModel);

    //初始化日志
    server.initLog();

    //初始化连接池
    server.initConnPool();

    //初始化线程池
    server.initThreadPool();

    //初始化触发模式
    server.initTrigMode();

    //初始化事件循环
    server.initEventLoop();

    //运行
    server.Run();

    return 0;
}