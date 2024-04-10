#ifndef HTTPCONN
#define HTTPCONN
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>
#include <cstdlib>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/unistd.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <sys/epoll.h>
#include <sys/uio.h>

class http_conn{
public:
    static const int READ_BUFF_SIZE {2048};
    static const int WRITE_BUFF_SIZE {1024};
    int m_sockfd;
    sockaddr_in m_addr;
    char m_read_buff[READ_BUFF_SIZE];
    char m_write_buff[WRITE_BUFF_SIZE];
    static int m_user_count;
    enum METHOD{GET=0,POST,HEAD,PUT,DELETE,TRACE,OPTIONS,CONNECT,PATH};
    // static int m_epoll_fd;
    enum CHECK_STATE {CHECKSTATE_REQUEST = 0, CHECKSTATE_HEAD, CHECKSTATE_CONTENT};
    enum LINE_STATE {LINE_OK = 0, LINE_BAD, LINE_OPEN};
    enum HTTP_CODE{NO_REQUEST,GET_REQUEST,BAD_REQUEST,NO_RESOURCE,FORBIDDEN_REQUEST,FILE_REQUEST,INTERNAL_ERROR,CLOSED_CONNECTION};

public:
    http_conn(){};
    ~http_conn(){};
    void init(int sockfd, sockaddr_in addr, int epoll_fd){
        m_sockfd = sockfd;
        m_addr = addr;
        m_epoll_fd = epoll_fd;
        m_user_count++;
        int reuse = 1;
        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

        init();
    }
    bool read_once();
    bool write();
    void close_conn(bool real_close);
    void process();


private:
    int m_read_idx;
    int m_write_idx;
    int m_checked_idx;
    METHOD m_method;
    CHECK_STATE m_check_state;
    int m_content_length;
    bool m_linger;
    char * m_url;
    char * m_host;
    char * m_string;
    struct stat m_file_stat;
    char* m_file_address;
    iovec m_iov[2];
    int m_iov_count;
    int m_bytes_to_send;
    int m_bytes_have_sent;
    int m_epoll_fd;
    char m_file[100];

private:
    void init(){
        m_read_idx = 0;
        m_write_idx = 0;
        m_checked_idx = 0;
        m_method = GET;
        m_check_state = CHECKSTATE_REQUEST;
        m_url = nullptr;
        m_host = nullptr;
        m_string = nullptr;
        m_content_length = 0;
        m_linger = false;
        m_iov_count = 0;
        m_bytes_to_send = 0;
        m_bytes_have_sent = 0;
        
        memset(m_read_buff, '\0', sizeof(m_read_buff));
        memset(m_write_buff, '\0', sizeof(m_write_buff));
    }
    LINE_STATE parse_line();
    HTTP_CODE parse_request(char * text);
    HTTP_CODE parse_head(char * text);
    HTTP_CODE parse_content(char * text);
    HTTP_CODE process_read();
    bool process_write(HTTP_CODE);
    HTTP_CODE do_request();
    bool add_response(const char* format, ...);
    bool add_status_line(int status, const char* title);
    bool add_head(int content_length);
    bool add_content(const char* content);
    void unmap();
    

};

#endif