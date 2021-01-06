
#ifndef INCLUDE_CONFIG_H
#define INCLUDE_CONFIG_H

class Config
{
public:
    Config();
    void parseArg(int argc,char *argv[]);

    int m_iPort;            //端口号
    int m_iLogWrite;        //同步或者异步写日志
    int m_iTrigMode;        //组合触发模式
    int m_iListenTrigMode;  //监听套接字触发模式
    int m_iConnTrigMode;    //连接套接字触发模式
    int m_iOptLinger;       //优雅关闭连接
    int m_iSqlNum;          //连接池连接数目
    int m_iThreadNum;       //线程池线程数目
    int m_iCloseLog;        //是否关闭日志
    int m_iActorModel;      //并发模型
};

#endif // INCLUDE_CONFIG_H