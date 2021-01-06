
#ifndef INCLUDE_SQL_CONNECTION_POOL_H
#define INCLUDE_SQL_CONNECTION_POOL_H

#include<string>
#include<list>
#include"../locker/locker.h"
#include"../sem/sem.h"
#include"../log/log.h"
#include<mysql/mysql.h>

using std::list;
using std::string;

/* mysql连接池 */
class SqlConnectionPool
{
public:
    static SqlConnectionPool * getInstance()
    {
        static SqlConnectionPool pool;
        return &pool;
    }
    int init(string istrUrl,string istrUser,string istrPasswd,string istrDBName,int iPort,int iMaxConn,int iCloseLog);
    /* 获得一个连接 */
    MYSQL *getConnection();
    /* 释放可用连接 */
    int freeConnection(MYSQL* ipConn);
    /* 获得可用连接数目 */
    int GetFreeConnNum();
    /* 销毁连接池 */
    int destoryPool();
private:
    SqlConnectionPool(){};
    ~SqlConnectionPool();
private:
    list<MYSQL *>m_connlist;             //连接链表
    Locker m_listLock;                   //链表锁
    Sem m_conSem;                        //连接数目信号量
    int m_iMaxConnNum;                   //连接池连接数
    int m_iWorkConnNum;                  //连接池工作连接数目
    int m_iFreeConnNum;                  //连接池可用连接数目
    string m_strUrl;                     //服务器地址
    int m_iPort;                         //服务器端口
    string m_strUser;                    //登录数据库用户名
    string m_strPasswd;                  //登录数据库用户密码
    string m_strDBName;                  //登录数据库名
    int m_iCloseLog;                     //是否关闭日志
};

class ConnectionRAII{

public:
	ConnectionRAII(MYSQL **opConn, SqlConnectionPool *ipConnPool);
	~ConnectionRAII();
	
private:
	MYSQL *m_pConRAII;
	SqlConnectionPool *m_PoolRAII;
};

#endif //INCLUDE_SQL_CONNECTION_POOL_H
