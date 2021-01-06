#ifndef INCLUDE_HTTP_CON_H
#define INCLUDE_HTTP_CON_H

#include<sys/types.h>
#include<sys/socket.h>
#include<string>
#include<netinet/in.h>
#include<sys/uio.h>
#include<sys/stat.h>
#include"../sql/sql_connection_pool.h"



class HttpCon
{
public:
    /*文件名称长度*/
    static const int FILENAME_LEN = 200;
    /* 读缓冲区 */
    static const int READ_BUFFER_SIZE =2048;
    /* 写缓冲区 */
    static const int WRITE_BUFFER_SIZE =1024;

    //请求报文方法种类
    enum METHOD
    {
        GET=0,
        HEAD,
        POST,
        PUT,
        DELETE,
        CONNECT,
        OPTIONS,
        TRACE,
        PATH,
        ERROR_METHOD
    };

    //主状态机
    enum CHECK_STATE
    {
        CHECK_STATE_REQUESTLINE=0, //解析请求行
        CHECK_STATE_HEADER,        //解析请求头部
        CHECK_STATE_CONTENT        //解析请求正文
    };

    /* 行读取状态 */
    enum LINE_STATUS
    {
        LINE_OK=0,                 //读取到完整一行
        LINE_OPEN,                 //读取到不完整行
        LINE_BAD                   //读取到错误行
    };

    //对于HTTP请求的处理结果
    enum HTTP_CODE
    {
        NO_REQUEST=0,GET_REQUEST,BAD_REQUEST,NO_RESOURCE,FORBIDDEN_REQUSET,FILE_REQUEST,INTERNAL_ERROR,CLOSED_CONNECTION
    };

public:
    HttpCon(){};
    ~HttpCon(){};
    /* 初始化函数 */
    int init(int iSockFd, const struct sockaddr_in &iAddr,char *ipDocRoot,int iTrigMode,int iCloseLog,
string istrUser,string istrPasswd,string istrSqlName);
    /* 一次读完所有可读数据 */
    int readOnce();
    /* 将多个块数据发送出去 */
    int writeV();
    /* 关闭连接 */
    int closeConn(bool ibRealClose);
    /* 初始化数据库用户列表 */
    int initSqlUsers(SqlConnectionPool *ipConnPool);
    /* 处理读写数据 */
    int process();

    sockaddr_in *getAddress()
    {
        return &m_cliAddr;
    }

    int m_iState;                                   //读为0, 写为1
    MYSQL *m_pMysql;
    int m_improv;
    int m_timer_flag;
    static int m_iUserCount;                        //所有用户数目
    static int m_iEpollFd;                          //所有连接的监听套接字
private:
    /* 初始化工具函数方便多次调用 */
    int init();
    /* 解析一行 */
    LINE_STATUS parseLine();
    /* 处理读数据 */
    HTTP_CODE processRead();
    /* 解析请求行 */
    HTTP_CODE parseRequestLine(char *ipText);
    /* 解析头部字段 */
    HTTP_CODE parseHeader(char *ipText);
    /* 解析正文部分 */
    HTTP_CODE parseContent(char *ipText);
    /* 获取请求报文方法 */
    METHOD getMethod(char *ipMethod);
    /* 处理http请求 */
    HTTP_CODE doRequest();
    /* 取消内存映射文件 */
    int unmap();
    /* 添加回复内容到写缓冲区 */
    bool addResponse(const char *ipFormat,...);
    /* 添加响应行 */
    bool addStatusLine(int iStatus,const char *ipTitle);
    /* 添加响应头 */
    bool addHeaders(int iContentLen);
    /* 添加报文长度响应头 */
    bool addContentLen(int iContentLen);
    /* 添加报文类型响应头 */
    bool addContentType();
    /* 添加是否长连接响应头 */
    bool addLongConnect();
    /* 添加空白行 */
    bool addBlankLine();
    /* 添加响应正文 */
    bool addContent(const char *ipContent);
    /* 处理写数据 */
    int processWrite(HTTP_CODE iRet);
    
private:
    int m_iSocketFd;                                //连接对应套接字                              
    struct sockaddr_in m_cliAddr;                   //连接对应的客户端地址
    int m_iTrigMode;                                //该套接字在epoll中的触发模式，LT/ET                              
    char *m_pDocRoot;                               //请求资源的目录
    char m_pReadBuffer[READ_BUFFER_SIZE];           //读缓冲区
    int m_iReadIndex;                               //读缓冲区读入数据的最后一个位置加1
    int m_iCheckIndex;                              //读缓冲区当前解析的数据位置
    int m_iCheckLine;                               //读缓冲区当前解析的行的起始位置
    CHECK_STATE m_checkState;                       //读缓冲区数据的当前解析状态
    char m_pWriteBuffer[WRITE_BUFFER_SIZE];         //写缓冲区
    int m_iWriteIndex;                              //要写数据的起始位置
    struct iovec m_ov[2];                           //要写的数据块
    int m_iOvCount;                                 //数据块个数0,1,2
    int m_iBytesToSend;                             //要发送的数据数
    int m_iBytesHaveSend;                           //已经发送的数据数
    int m_iCloseLog;                                //是否关闭日志    
    string m_strUser;                               //连接对应的sql用户
    string m_strPasswd;                             //连接对应的sql用户密码
    string m_strSqlName;                            //连接对应的sql数据库名称
    char *m_pUrl;                                   //http报文对应的url地址
    METHOD m_httpMethod;                            //http请求报文对应的方法
    char *m_pHttpVersion;                           //http报文对应的版本号
    char *m_pHttpContent;                           //http报文请求正文
    int m_iContentLength;                           //http请求报文长度
    bool m_bLongConnect;                            //是否是长连接
    char *m_pHost;                                  //服务器ip及端口
    char m_pRealFile[FILENAME_LEN];                 //实际请求的文件
    struct stat m_fileStat;                         //请求文件的状态
    char *m_pFileAddress;                           //请求文件映射的内存起始地址
};





#endif // INCLUDE_HTTP_CON_H