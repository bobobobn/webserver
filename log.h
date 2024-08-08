#ifndef LOG_H
#define LOG_H
#include <pthread.h>
#include <exception>
#include <list>
#include <cstring>
#include <string>
#include <cstdio>
#include "lock/locker.h"
#include <time.h>
#include <stdarg.h>
#include <sys/time.h>
#include <sys/uio.h>


template <typename T>
class block_queue{
public:
    block_queue(int max_size){
        m_size = 0;
        if(max_size < 0){
            throw std::exception();
        }
        m_max_size = max_size;
        pthread_mutex_init(&m_mutex, NULL);
        pthread_cond_init(&m_cond, NULL);
    }

    bool push(const T& item){
        pthread_mutex_lock(&m_mutex);
        if(m_max_size < m_size){
            pthread_cond_broadcast(&m_cond);
            pthread_mutex_unlock(&m_mutex);
            return false;
        }
        m_queue.push_back(item);
        m_size++;
        pthread_cond_broadcast(&m_cond);
        pthread_mutex_unlock(&m_mutex);
        return true;
    }
    
    bool pop(T& item){
        pthread_mutex_lock(&m_mutex);
        if(m_size<=0){
            if(pthread_cond_wait(&m_cond, &m_mutex) != 0)
            {
                pthread_mutex_unlock(&m_mutex);
                return false;
            }
        }
        item = m_queue.front();
        m_queue.pop_front();
        m_size--;
        pthread_mutex_unlock(&m_mutex);
        return true;
    }

private:
    int m_max_size;
    int m_size;
    std::list <T> m_queue;
    pthread_mutex_t m_mutex;
    pthread_cond_t m_cond;

};

class Log{
public:
    static Log* get_instance(){
        static Log instance;
        return &instance;
    }
    bool init(char* file_name, int log_buff_size, int max_lines, int max_queue_size){
        m_level = 0;
        memset(m_file_name, '\0', 100);
        memset(m_dir_name, '\0', 100);
        const char* p = strrchr(file_name, '/');
        char full_file_name[256] = {0}; 
        time_t t = time(NULL);
        struct tm my_tm = *localtime(&t);
        if(!p){
            strcpy(m_file_name, file_name);
            snprintf(full_file_name, 255, "%d_%02d_%02d_%s", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, file_name);
        }
        else{
            strcpy(m_file_name, p+1);
            strncpy(m_dir_name, file_name, p-file_name+1);
            snprintf(full_file_name, 255, "%s%d_%02d_%02d_%s", m_dir_name, my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, m_file_name);
        }
        m_today = my_tm.tm_mday;
        m_log_buff_size = log_buff_size;
        m_buf = new char[log_buff_size];
        memset(m_buf, '\0', log_buff_size);
        m_max_lines = max_lines;
        m_line_count = 0;
        m_queue = new block_queue<std::string>(max_queue_size);
        m_file = fopen(full_file_name, "a");
        pthread_t thread;
        if((pthread_create(&thread, NULL, flush_write_log, NULL)!=0) || (pthread_detach(thread))!=0  ){
            throw std::exception();
            return false;
        }
        if(!m_file){
            return false;
        }
        return true;

        
    }
    bool write_log(int level, const char* format, ...){
        if(level < m_level){
            return false;
        }
        struct timeval now = {0, 0};
        gettimeofday(&now, NULL);
        time_t t = time(NULL);
        struct tm my_tm = *localtime(&t);
        char s[16] = {0};
        switch (level)
        {
        case 0:
            strcpy(s, "[debug]:");
            break;
        case 1:
            strcpy(s, "[info]:");
            break;
        case 2:
            strcpy(s, "[warn]:");
            break;
        case 3:
            strcpy(s, "[erro]:");
            break;
        default:
            strcpy(s, "[info]:");
            break;
        }
        va_list valist;
        va_start(valist, format);
        std::string str;
        m_lock.lock();
        // 写入日志内容到buf
        int n = snprintf(m_buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                    my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                    my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);
        int m = vsnprintf(m_buf+n, m_log_buff_size-1-n, format, valist);
        m_buf[m+n] = '\n';
        m_buf[m+n+1] = '\0';
        str = m_buf;
        m_queue->push(str);
        m_line_count++;    
        
        // 处理日期变化、行数溢出
        if(my_tm.tm_mday!=m_today || m_line_count >= m_max_lines){
            fflush(m_file);
            fclose(m_file);
            char full_file_name[256] = {0}; 
            if(m_line_count >= m_max_lines){                
                snprintf(full_file_name, 255, "%s%d_%02d_%02d_%s_%lld", m_dir_name, my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, m_file_name, m_line_count/m_max_lines);
            }
            else{
                snprintf(full_file_name, 255, "%s%d_%02d_%02d_%s", m_dir_name, my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday, m_file_name);
                m_line_count = 0;
                m_today = my_tm.tm_mday;
            }
            m_file = fopen(full_file_name, "a");
        }

        m_lock.unlock();
        va_end(valist);
        return true;

    }
    static void* flush_write_log(void* args){
        Log::get_instance()->async_write();
    }
    void set_level(int level){
        m_level = level;
    }


private:
    char m_file_name[100];
    char m_dir_name[100];
    char * m_buf;
    int m_log_buff_size;
    int m_max_lines;
    int m_today;
    int m_line_count;
    block_queue<std::string>* m_queue;
    locker m_lock;
    FILE* m_file;
    void* async_write(){
        std::string single_log;
        while(m_queue->pop(single_log)){
            m_lock.lock();
            fputs(single_log.c_str(), m_file);
            fflush(m_file);
            m_lock.unlock();
        }
    }
    Log(){};
    ~Log(){
        if(m_file)
            fclose(m_file);
        delete m_queue;
    };
    int m_level;    
};


#define LOG_DEBUG(format, ...) Log::get_instance()->write_log(0, format, ##__VA_ARGS__)
#define LOG_INFO(format, ...) Log::get_instance()->write_log(1, format, ##__VA_ARGS__)
#define LOG_WARN(format, ...) Log::get_instance()->write_log(2, format, ##__VA_ARGS__)
#define LOG_ERROR(format, ...) Log::get_instance()->write_log(3, format, ##__VA_ARGS__)

#endif