#include "connectionPool.h"

connectionPool* connectionPool::get_instance()
{
        static connectionPool instance;
        return &instance;
}

void connectionPool::DestroyPool(){

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