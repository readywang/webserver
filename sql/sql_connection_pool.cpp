#include "sql_connection_pool.h"

SqlConnectionPool::~SqlConnectionPool()
{
    destoryPool();
}

int SqlConnectionPool::init(string istrUrl,string istrUser,string istrPasswd,string istrDBName,int iPort,int iMaxConn,int iCloseLog)
{
    m_strUrl=istrUrl;
    m_strUser=istrUser;
    m_strPasswd=istrPasswd;
    m_strDBName=istrDBName;
    m_iPort=iPort;
    m_iCloseLog=iCloseLog;
    int errorCode=0;
    for(int i=0;i<iMaxConn;++i)
    {
        MYSQL *pCon=NULL;
        pCon=mysql_init(pCon);
        if(pCon==NULL)
        {
            LOG_ERROR("mysql error");
            errorCode=1;
            continue;
        }
        pCon=mysql_real_connect(pCon,m_strUrl.c_str(),m_strUser.c_str(),m_strPasswd.c_str(),m_strDBName.c_str(),m_iPort,NULL,0);
        if(pCon==NULL)
        {
            LOG_ERROR("mysql error");
            errorCode=1;
            continue;
        }
        m_connlist.push_back(pCon);
        ++m_iFreeConnNum;
    }
    m_iMaxConnNum=m_iFreeConnNum;
    m_conSem=Sem(m_iMaxConnNum);
    return errorCode;
}

MYSQL * SqlConnectionPool::getConnection()
{
    if(0==m_connlist.size()) return NULL;
    m_conSem.semWait();
    m_listLock.lock();
    m_iFreeConnNum--;
    m_iWorkConnNum++;
    MYSQL * pConn=m_connlist.front();
    m_connlist.pop_front();
    m_listLock.unlock();
    return pConn;
}

int SqlConnectionPool::freeConnection(MYSQL* ipConn)
{
    if(ipConn=NULL)
    {
        return 1;
    }
    m_listLock.lock();
    m_iFreeConnNum++;
    m_iWorkConnNum--;
    m_connlist.push_back(ipConn);
    m_listLock.unlock();
    m_conSem.semPost();
    return 0;
}

int SqlConnectionPool::GetFreeConnNum()
{
    m_listLock.lock();
    int num=m_iFreeConnNum;
    m_listLock.unlock();
    return num;
}

int SqlConnectionPool::destoryPool()
{
    m_listLock.lock();
    while(!m_connlist.empty())
    {
        MYSQL*pConn=m_connlist.front();
        m_connlist.pop_front();
        mysql_close(pConn);
    }
    m_iMaxConnNum=0;
    m_iWorkConnNum=0;
    m_iFreeConnNum=0;
    m_listLock.unlock();
    return 0;
}

ConnectionRAII::ConnectionRAII(MYSQL **opConn, SqlConnectionPool *ipConnPool){
	*opConn = ipConnPool->getConnection();
	
	m_pConRAII = *opConn;
	m_PoolRAII = ipConnPool;
}

ConnectionRAII::~ConnectionRAII(){
	m_PoolRAII->freeConnection(m_pConRAII);
}