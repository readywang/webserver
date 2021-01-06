#include"http_con.h"
#include<sys/epoll.h>
#include<unistd.h>
#include<fcntl.h>
#include<string.h>
#include<stdlib.h>
#include<map>
#include<mysql/mysql.h>
#include<sys/mman.h>
#include<errno.h>
#include"../log/log.h"


using std::map;
using std::pair;

int HttpCon::m_iEpollFd=-1;
int HttpCon::m_iUserCount=0;

Locker m_lock;
map<string, string> users;

/* 定义http响应短语 */
const char *pOk_200_title="OK";
const char *pError_400_title="Bad Request";
const char *pError_400_form="Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *pError_403_title = "Forbidden";
const char *pError_403_form = "You do not have permission to get file form this server.\n";
const char *pError_404_title = "Not Found";
const char *pError_404_form = "The requested file was not found on this server.\n";
const char *pError_500_title = "Internal Error";
const char *pError_500_form = "There was an unusual problem serving the request file.\n";

/**向epoll注册套接字的可读事件，可选择是否开启oneshot以及LT/ET触发*/
int addFd(int iEpollFd,int iSockFd,bool ibOneShot,int iTrigMode)
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
    return epoll_ctl(iEpollFd,EPOLL_CTL_ADD,iSockFd,&event);
}

/* 从epoll移除套接字 */
int removeFd(int iEpollFd,int iSockFd)
{
    epoll_ctl(iEpollFd,EPOLL_CTL_DEL,iSockFd,NULL);
    close(iSockFd);
    return 0;
}

/* 重新设置套接字的可读或者可写事件,开启oneshot选项 */
int modFd(int iEpollFd,int iSockFd,int iEv,int iTrigMode)
{
    struct epoll_event event;
    event.data.fd=iSockFd;
    event.events=iEv|EPOLLONESHOT|EPOLLRDHUP;
    if(1==iTrigMode)
    {
        event.events|=EPOLLET;
    }
    return epoll_ctl(iEpollFd,EPOLL_CTL_MOD,iSockFd,&event);
}

/* 设置套接字非阻塞 */
int setNonblock(int iFd)
{
    int oldMode=fcntl(iFd,F_GETFL);
    int newMode=oldMode|O_NONBLOCK;
    fcntl(iFd,F_SETFL,newMode);
    return oldMode;
}

int HttpCon::initSqlUsers(SqlConnectionPool *ipConnPool)
{
    //先从连接池中取一个连接
    MYSQL *mysql = NULL;
    ConnectionRAII mysqlcon(&mysql, ipConnPool);

    //在user表中检索username，passwd数据，浏览器端输入
    if (mysql_query(mysql, "SELECT username,passwd FROM user"))
    {
        LOG_ERROR("SELECT error:%s\n", mysql_error(mysql));
    }

    //从表中检索完整的结果集
    MYSQL_RES *result = mysql_store_result(mysql);

    //返回结果集中的列数
    int num_fields = mysql_num_fields(result);

    //返回所有字段结构的数组
    MYSQL_FIELD *fields = mysql_fetch_fields(result);

    //从结果集中获取下一行，将对应的用户名和密码，存入map中
    while (MYSQL_ROW row = mysql_fetch_row(result))
    {
        string temp1(row[0]);
        string temp2(row[1]);
        users[temp1] = temp2;
    }
    return 0;
}

int HttpCon::init(int iSockFd, const struct sockaddr_in &iAddr,char *ipDocRoot,int iTrigMode,int iCloseLog,
string istrUser,string istrPasswd,string istrSqlName)
{
    m_iSocketFd=iSockFd;
    m_cliAddr=iAddr;
    //设置套接字非阻塞并添加监听
    setNonblock(m_iSocketFd);
    addFd(m_iEpollFd,m_iSocketFd,true,m_iTrigMode);
    m_iUserCount++;
    m_pDocRoot=ipDocRoot;
    m_iTrigMode=iTrigMode;
    m_iCloseLog=iCloseLog;
    m_strUser=istrUser;
    m_strPasswd=istrPasswd;
    m_strSqlName=istrSqlName;
    init();
    return 0;
}

int HttpCon::init()
{
    memset(m_pReadBuffer,0,READ_BUFFER_SIZE);
    memset(m_pRealFile,0,FILENAME_LEN);
    memset(m_pWriteBuffer,0,WRITE_BUFFER_SIZE);
    m_pMysql=NULL;
    m_iReadIndex=0;
    m_iCheckIndex=0;
    m_iCheckLine=0;
    m_checkState=CHECK_STATE_REQUESTLINE;
    m_pUrl=NULL;
    m_httpMethod=GET;
    m_pHttpVersion=NULL;
    m_pHttpContent=NULL;
    m_iContentLength=0;
    m_bLongConnect=false;
    m_iOvCount=0;
    m_iWriteIndex=0;
    m_iBytesToSend=0;
    m_iBytesHaveSend=0;
    m_improv=0;
    m_timer_flag=0;
}

int HttpCon::readOnce()
{
    if(m_iReadIndex>READ_BUFFER_SIZE)
    {
        return 1;
    }
    int readBytes=0;
    //LT模式
    if(0==m_iTrigMode)
    {
        readBytes=recv(m_iSocketFd,m_pReadBuffer+m_iReadIndex,READ_BUFFER_SIZE-m_iReadIndex,0);
        if(readBytes<=0)
        {
            return 1;
        }
        m_iReadIndex+=readBytes;
        return 0;
    }
    //ET模式
    else
    {
        while(true)
        {
            readBytes=recv(m_iSocketFd,m_pReadBuffer+m_iReadIndex,READ_BUFFER_SIZE-m_iReadIndex,0);
            if(readBytes<0)
            {
                if(errno==EAGAIN||errno==EWOULDBLOCK)
                    break;
                return 1;
            }
            if(readBytes==0) return 1;
            m_iReadIndex+=readBytes;
        }
        return 0;
    }
}

int HttpCon::writeV()
{
    if(m_iBytesToSend==0)
    {
        modFd(m_iEpollFd,m_iSocketFd,EPOLLIN,m_iTrigMode);
        init();
        return 0;
    }
    while(true)
    {
        int n=writev(m_iSocketFd,m_ov,m_iOvCount);
        if(n<0)
        {
            if(errno==EAGAIN)
            {
                modFd(m_iEpollFd,m_iSocketFd,EPOLLOUT,m_iTrigMode);
                return 0;
            }
            unmap();
            return 1;
        }
        m_iBytesHaveSend+=n;
        m_iBytesToSend-=n;
        if(m_iBytesHaveSend>=m_ov[0].iov_len)
        {
            m_ov[0].iov_len=0;
            m_ov[1].iov_len=m_iBytesToSend;
            m_ov[1].iov_base=m_pFileAddress+(m_iBytesHaveSend-m_iWriteIndex);
        }
        else
        {
            m_ov[0].iov_len-=n;
            m_ov[0].iov_base=m_pWriteBuffer+m_iBytesHaveSend;
        }
        if(m_iBytesToSend<=0)
        {
            unmap();
            modFd(m_iEpollFd,m_iSocketFd,EPOLLIN,m_iTrigMode);
            if(m_bLongConnect)
            {
                init();
                return 0;
            }
            return 1;
        }
    }
}

int HttpCon::closeConn(bool ibRealClose)
{
    if(ibRealClose&&m_iSocketFd!=-1)
    {
        LOG_INFO("close fd %d\n",m_iSocketFd);
        removeFd(m_iEpollFd,m_iSocketFd);
        m_iSocketFd=-1;
        m_iUserCount--;
    }
}


HttpCon::METHOD HttpCon::getMethod(char *ipMethod)
{
    if(ipMethod==NULL)
    {
        return ERROR_METHOD;
    }
    else if(strcasecmp(ipMethod,"get")==0)
    {
        return GET;
    }
    else if(strcasecmp(ipMethod,"head")==0)
    {
        return HEAD;
    }
    else if(strcasecmp(ipMethod,"post")==0)
    {
        return POST;
    }
    else if(strcasecmp(ipMethod,"put")==0)
    {
        return PUT;
    }
    else if(strcasecmp(ipMethod,"delete")==0)
    {
        return DELETE;
    }
    else if(strcasecmp(ipMethod,"connect")==0)
    {
        return CONNECT;
    }
    else if(strcasecmp(ipMethod,"options")==0)
    {
        return OPTIONS;
    }
    else if(strcasecmp(ipMethod,"trace")==0)
    {
        return TRACE;
    }
    else if(strcasecmp(ipMethod,"path")==0)
    {
        return PATH;
    }
    else
    {
        return ERROR_METHOD;
    }
}

HttpCon::LINE_STATUS HttpCon::parseLine()
{
    //遇到\r\n说明解析到完整一行
    while(m_iCheckIndex<m_iReadIndex)
    {
        if(m_pReadBuffer[m_iCheckIndex]=='\r')
        {
            if(m_iCheckIndex+1==m_iReadIndex)
            {
                return LINE_OPEN;
            }
            else if(m_pReadBuffer[m_iCheckIndex+1]=='\n')
            {
                m_pReadBuffer[m_iCheckIndex++]='\0';
                m_pReadBuffer[m_iCheckIndex++]='\0';
                return LINE_OK;
            }
            else
            {
                return LINE_BAD;
            }
            
        }
        else if(m_pReadBuffer[m_iCheckIndex]=='\n')
        {
            if(m_iCheckIndex>1&&m_pReadBuffer[m_iCheckIndex-1]=='\r')
            {
                m_pReadBuffer[m_iCheckIndex-1]='\0';
                m_pReadBuffer[m_iCheckIndex++]='\0';
                return LINE_OK;
            }
            else
            {
                return LINE_BAD;
            }
            
        }
        ++m_iCheckIndex;
    }
    //若解析到末尾说明一行没有解析完
    return LINE_OPEN;
}

HttpCon::HTTP_CODE HttpCon::parseRequestLine(char *ipText)
{
    if(ipText==NULL) return BAD_REQUEST;
    //先获取请求方法
    m_pUrl=strpbrk(ipText," \t");
    if(m_pUrl==NULL) return BAD_REQUEST;
    *m_pUrl++='\0';
    m_httpMethod=getMethod(ipText);
    //目前暂时只支持get和post
    if(m_httpMethod!=GET&&m_httpMethod!=POST)
    {
        return BAD_REQUEST;
    }

    //跳过多余空格或者制表符，再获取版本号
    m_pUrl+=strspn(m_pUrl," \t");
    m_pHttpVersion=strpbrk(m_pUrl," \t");
    if(m_pHttpVersion==NULL) return BAD_REQUEST;
    *m_pHttpVersion++='\0';
    m_pHttpVersion+=strspn(m_pHttpVersion," \t");
    /* 仅支持http1.1 */
    if(strcasecmp(m_pHttpVersion,"HTTP/1.1")!=0)
    {
        return BAD_REQUEST;
    }

    //去掉url地址首部
    if(strncasecmp(m_pUrl,"http://",7)==0)
    {
        m_pUrl+=7;
        m_pUrl=strchr(m_pUrl,'/');
    }
    else if(strncasecmp(m_pUrl,"https://",8)==0)
    {
        m_pUrl+=8;
        m_pUrl=strchr(m_pUrl,'/');
    }
    if(m_pUrl==NULL||m_pUrl[0]!='/')
    {
        return BAD_REQUEST;
    }
    //url为/时显示判断界面
    if(strlen(m_pUrl)==1)
    {
        strcat(m_pUrl,"judge.html");
    }
    //状态转换
    m_checkState=CHECK_STATE_HEADER;
    return NO_REQUEST;
}

HttpCon::HTTP_CODE HttpCon::parseHeader(char *ipText)
{
    if(ipText==NULL)
    {
        return BAD_REQUEST;
    }
    //遇到空行说明结束了
    if(ipText[0]=='\0')
    {
        if(m_iContentLength==0)
        {
            return GET_REQUEST;
        }
        m_checkState=CHECK_STATE_CONTENT;
        return NO_REQUEST;
    }
    else if(strncasecmp(ipText,"Connection:",11)==0)
    {
        //判断是否是长连接
        ipText+=11;
        ipText+=strspn(ipText," \t");
        if(strcasecmp(ipText,"keep-alive")==0)
        {
            m_bLongConnect=true;
        }
        else
        {
            m_bLongConnect=false;
        }
        
    }
    else if(strncasecmp(ipText,"host:",5)==0)
    {
        //保存服务器ip及端口
        ipText+=5;
        ipText+=strspn(ipText," \t");
        m_pHost=ipText;
    }
    else if(strncasecmp(ipText, "Content-length:", 15)==0)
    {
        //保存报文长度
        ipText+=15;
        ipText+=strspn(ipText," \t");
        m_iContentLength=atoi(ipText);
    }
    else
    {
        LOG_INFO("unknown header:%s",ipText);
    }
    return NO_REQUEST;
}

HttpCon::HTTP_CODE HttpCon::parseContent(char *ipText)
{
    //判断是否读完
    if(m_iReadIndex>=m_iContentLength+m_iCheckIndex)
    {
        ipText[m_iContentLength]='\0';
        m_pHttpContent=ipText;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

HttpCon::HTTP_CODE HttpCon::processRead()
{
    LINE_STATUS status=LINE_OK;
    while((m_checkState==CHECK_STATE_CONTENT&&status==LINE_OK)||(status=parseLine())==LINE_OK)
    {
        HTTP_CODE httpCode;
        char *pText=m_pReadBuffer+m_iCheckLine;
        m_iCheckLine=m_iCheckIndex;
        LOG_INFO("parse %s",pText);
        switch (m_checkState)
        {
        case CHECK_STATE_REQUESTLINE:
        {
            httpCode=parseRequestLine(pText);
            if(httpCode==BAD_REQUEST)
            {
                return BAD_REQUEST;
            } 
            break;
        }
        case CHECK_STATE_HEADER:
        {
            httpCode=parseHeader(pText);
            if(httpCode==BAD_REQUEST)
            {
                return BAD_REQUEST;
            }
            else if(httpCode==GET_REQUEST)
            {
                return doRequest();
            }
            break;
        }
        case CHECK_STATE_CONTENT:
        {
            httpCode=parseContent(pText);
            if(httpCode==GET_REQUEST)
            {
                return doRequest();
            }
            status=LINE_OPEN;
            break;
        }
        default:
            return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;
}

HttpCon::HTTP_CODE HttpCon::doRequest()
{
    strcpy(m_pRealFile, m_pDocRoot);
    int len = strlen(m_pDocRoot);
    //printf("m_pUrl:%s\n", m_pUrl);
    const char *p = strrchr(m_pUrl, '/');

    //处理cgi
    if (m_httpMethod == POST && (*(p + 1) == '2' || *(p + 1) == '3'))
    {

        //根据标志判断是登录检测还是注册检测
        char flag = m_pUrl[1];

        char *m_pUrl_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_pUrl_real, "/");
        strcat(m_pUrl_real, m_pUrl + 2);
        strncpy(m_pRealFile + len, m_pUrl_real, FILENAME_LEN - len - 1);
        free(m_pUrl_real);

        //将用户名和密码提取出来
        //user=123&passwd=123
        char name[100], password[100];
        int i;
        for (i = 5; m_pHttpContent[i] != '&'; ++i)
            name[i - 5] = m_pHttpContent[i];
        name[i - 5] = '\0';

        int j = 0;
        for (i = i + 10; m_pHttpContent[i] != '\0'; ++i, ++j)
            password[j] = m_pHttpContent[i];
        password[j] = '\0';

        if (*(p + 1) == '3')
        {
            //如果是注册，先检测数据库中是否有重名的
            //没有重名的，进行增加数据
            char *sql_insert = (char *)malloc(sizeof(char) * 200);
            strcpy(sql_insert, "INSERT INTO user(username, passwd) VALUES(");
            strcat(sql_insert, "'");
            strcat(sql_insert, name);
            strcat(sql_insert, "', '");
            strcat(sql_insert, password);
            strcat(sql_insert, "')");

            if (users.find(name) == users.end())
            {
                m_lock.lock();
                int res = mysql_query(m_pMysql, sql_insert);
                users.insert(pair<string, string>(name, password));
                m_lock.unlock();

                if (!res)
                    strcpy(m_pUrl, "/log.html");
                else
                    strcpy(m_pUrl, "/registerError.html");
            }
            else
                strcpy(m_pUrl, "/registerError.html");
        }
        //如果是登录，直接判断
        //若浏览器端输入的用户名和密码在表中可以查找到，返回1，否则返回0
        else if (*(p + 1) == '2')
        {
            if (users.find(name) != users.end() && users[name] == password)
                strcpy(m_pUrl, "/welcome.html");
            else
                strcpy(m_pUrl, "/logError.html");
        }
    }

    if (*(p + 1) == '0')
    {
        char *m_pUrl_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_pUrl_real, "/register.html");
        strncpy(m_pRealFile + len, m_pUrl_real, strlen(m_pUrl_real));

        free(m_pUrl_real);
    }
    else if (*(p + 1) == '1')
    {
        char *m_pUrl_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_pUrl_real, "/log.html");
        strncpy(m_pRealFile + len, m_pUrl_real, strlen(m_pUrl_real));

        free(m_pUrl_real);
    }
    else if (*(p + 1) == '5')
    {
        char *m_pUrl_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_pUrl_real, "/picture.html");
        strncpy(m_pRealFile + len, m_pUrl_real, strlen(m_pUrl_real));

        free(m_pUrl_real);
    }
    else if (*(p + 1) == '6')
    {
        char *m_pUrl_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_pUrl_real, "/video.html");
        strncpy(m_pRealFile + len, m_pUrl_real, strlen(m_pUrl_real));

        free(m_pUrl_real);
    }
    else if (*(p + 1) == '7')
    {
        char *m_pUrl_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_pUrl_real, "/fans.html");
        strncpy(m_pRealFile + len, m_pUrl_real, strlen(m_pUrl_real));

        free(m_pUrl_real);
    }
    else
        strncpy(m_pRealFile + len, m_pUrl, FILENAME_LEN - len - 1);

    if (stat(m_pRealFile, &m_fileStat) < 0)
        return NO_RESOURCE;

    if (!(m_fileStat.st_mode & S_IROTH))
        return FORBIDDEN_REQUSET;

    if (S_ISDIR(m_fileStat.st_mode))
        return BAD_REQUEST;

    int fd = open(m_pRealFile, O_RDONLY);
    m_pFileAddress = (char *)mmap(0, m_fileStat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    return FILE_REQUEST;
}

int HttpCon::unmap()
{
    if(m_pFileAddress==NULL) return 1;
    int ret=munmap(m_pFileAddress,m_fileStat.st_size);
    m_pFileAddress=NULL;
    return ret;
}

bool HttpCon::addResponse(const char *ipFormat,...)
{
    if(m_iWriteIndex>WRITE_BUFFER_SIZE) return false;
    va_list ap;
    va_start(ap,ipFormat);
    int len=vsnprintf(m_pWriteBuffer+m_iWriteIndex,WRITE_BUFFER_SIZE-m_iWriteIndex-1,ipFormat,ap);
    if(len>=WRITE_BUFFER_SIZE-m_iWriteIndex-1)
    {
        va_end(ap);
        return false;
    }
    m_iWriteIndex+=len;
    va_end(ap);
    LOG_INFO("response:%s",m_pWriteBuffer);
    return true;
}

bool HttpCon::addStatusLine(int iStatus,const char *ipTitle)
{
    return addResponse("%s %d %s\r\n","HTTP/1.1",iStatus,ipTitle);
}

bool HttpCon::addHeaders(int iContentLen)
{
    return addContentLen(iContentLen)&&addLongConnect()&&addBlankLine();
}

bool HttpCon::addContentLen(int iContentLen)
{
    return addResponse("%s %d\r\n","Content-Length:",iContentLen);
}

bool HttpCon::addContentType()
{
    return addResponse("Content-Type:%s\r\n","text/html");
}

bool HttpCon::addLongConnect()
{
    return addResponse("Connection:%s\r\n",m_bLongConnect==true?"keep-alive":"close");
}

bool HttpCon::addBlankLine()
{
    return addResponse("%s","\r\n");
}

bool HttpCon::addContent(const char *ipContent)
{
    return addResponse("%s",ipContent);
}

int HttpCon::processWrite(HTTP_CODE iRet)
{
    switch (iRet)
    {
    case INTERNAL_ERROR:
    {
        addStatusLine(500,pError_500_title);
        addHeaders(strlen(pError_500_form));
        if(!addContent(pError_500_form))
        {
            return 1;
        }
        break;
    }
    case BAD_REQUEST:
    {
        addStatusLine(400,pError_400_title);
        addHeaders(strlen(pError_400_form));
        if(!addContent(pError_400_form))
        {
            return 1;
        }
        break;
    }
    case FORBIDDEN_REQUSET:
    {
        addStatusLine(403,pError_403_title);
        addHeaders(strlen(pError_403_form));
        if(!addContent(pError_403_form))
        {
            return 1;
        }
        break;
    }
    case NO_RESOURCE:
    {
        addStatusLine(404,pError_404_title);
        addHeaders(strlen(pError_404_form));
        if(!addContent(pError_404_form))
        {
            return 1;
        }
        break;
    }
    case FILE_REQUEST:
    {
        addStatusLine(200,pOk_200_title);
        if(m_fileStat.st_size!=0)
        {
            //设置发送的缓冲区直接退出
            addHeaders(m_fileStat.st_size);
            m_ov[0].iov_len=m_iWriteIndex;
            m_ov[0].iov_base=m_pWriteBuffer;
            m_ov[1].iov_len=m_fileStat.st_size;
            m_ov[1].iov_base=m_pFileAddress;
            m_iOvCount=2;
            m_iBytesToSend=m_fileStat.st_size+m_iWriteIndex;
            return 0;
        }
        else
        {
            const char *pOkContent="<html><body></body></html>";
            addHeaders(strlen(pOkContent));
            if(!addContent(pOkContent))
            {
                return 1;
            }
            break;
        }
        
    }
    default:
        return 1;
    }
    //除了发送文件大小不为0以外的情况都在此设置发送缓冲区
    m_ov[0].iov_len=m_iWriteIndex;
    m_ov[0].iov_base=m_pWriteBuffer;
    m_iOvCount=1;
    m_iBytesToSend=m_iWriteIndex;
    return 0;
}

int HttpCon::process()
{
    HTTP_CODE ret=processRead();
    if(NO_REQUEST==ret)
    {
        modFd(m_iEpollFd,m_iSocketFd,EPOLLIN,m_iTrigMode);
        return 0;
    }
    int res=processWrite(ret);
    if(0!=res)
    {
        closeConn(true);
    }
    modFd(m_iEpollFd,m_iSocketFd,EPOLLOUT,m_iTrigMode);
    return 0;
}