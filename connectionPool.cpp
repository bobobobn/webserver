#include "connectionPool.h"

connectionPool* connectionPool::get_instance()
{
        static connectionPool instance;
        return &instance;
}

void connectionPool::DestroyPool(){

    printf("poolsize %d\n",connList.size());
    m_lock.lock();
    if (connList.size() > 0)
    {
        std::list<MYSQL *>::iterator it;
        for (it = connList.begin(); it != connList.end(); ++it)
        {
            MYSQL *con = *it;
            mysql_close(con);
        }
        connList.clear();
    }

    m_lock.unlock();
    delete m_sem;
    
}

connectionPool::~connectionPool(){
    DestroyPool();
}

void connectionPool::init(string url, string User, string PassWord, string DBName, int Port, unsigned int MaxConn){
    freeConn = 0;
    m_url = url;
    m_user = User;
    m_password = PassWord;
    m_DBName = DBName;
    m_max_conn = MaxConn;
    for (int i = 0; i < MaxConn; i++)
	{
		MYSQL *con = NULL;
		con = mysql_init(con);

		if (con == NULL)
		{
			LOG_ERROR("MySQL Error");
			exit(1);
		}
		con = mysql_real_connect(con, url.c_str(), User.c_str(), PassWord.c_str(), DBName.c_str(), Port, NULL, 0);

		if (con == NULL)
		{
			LOG_ERROR("MySQL Error");
			exit(1);
		}
		connList.push_back(con);
		++freeConn;
	}
    m_sem = new sem(freeConn);
}

MYSQL* connectionPool::getConn(){
    MYSQL* ret = nullptr;
    m_sem->wait();
    m_lock.lock();
    if(connList.empty()){
        m_lock.unlock();
        throw std::exception();
    }
    ret = connList.front();
    connList.pop_front();
    freeConn--;
    m_lock.unlock();
    return ret;
}
bool connectionPool::releaseConn(MYSQL* conn)
{
        if(!conn){
            LOG_ERROR("%s", "releasing a null MYSQL CONN");
            return false;
        }
        m_lock.lock();
        if(connList.size() >= m_max_conn){
            m_lock.unlock();
            throw std::exception();
        }
        connList.push_back(conn);
        freeConn++;
        m_lock.unlock();
        m_sem->post();
        return true;
}