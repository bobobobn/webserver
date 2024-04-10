#include "httpConn.h"
#include <cstdio>
#include "util.h"

const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";
int http_conn::m_user_count = 0;

http_conn::LINE_STATE http_conn::parse_line(){
    char temp;
    for(; m_checked_idx < m_read_idx; m_checked_idx++){
        temp = m_read_buff[m_checked_idx];
        if( temp == '\r' ){
            if(m_checked_idx + 1 == m_read_idx){
                return LINE_OPEN;
            }
            else if( m_read_buff[m_checked_idx+1] == '\n'){
                m_read_buff[m_checked_idx++] = '\0';
                m_read_buff[m_checked_idx++] = '\0';
                return LINE_OK;
            }
        }
        if( temp == '\n'){
            if( (m_checked_idx > 1) && (m_read_buff[m_checked_idx-1] == '\r') ){
                m_read_buff[m_checked_idx - 1] = '\0';
                m_read_buff[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }        
    }
    return LINE_OPEN;
}

http_conn::HTTP_CODE http_conn::parse_request(char * text){
    m_url = strpbrk(text, " \t");
    if(!m_url){
        return BAD_REQUEST;
    }
    *m_url++ = '\0';
    char* method = text;
    if( strcasecmp(method, "GET") == 0){
        m_method = GET;
    }
    else if( strcasecmp(method, "POST") == 0 ){
        m_method = POST;
    }
    else{
        return BAD_REQUEST;
    }
    m_url += strspn(m_url, " \t");
    char* version = strpbrk(m_url, " \t");
    if(!version){
        return BAD_REQUEST;
    }
    *version++ = '\0';
    version += strspn(version," \t");
    if(strcasecmp(version,"HTTP/1.1") != 0)
        return BAD_REQUEST;

    if(strncasecmp(m_url, "http://", 7) == 0){
        m_url += 7;
        m_url = strchr(m_url, '/');
    }
    else if(strncasecmp(m_url, "https://", 8) == 0){
        m_url += 8;
        m_url = strchr(m_url, '/');
    }
    if(!m_url || *m_url!='/'){
        return BAD_REQUEST;
    }
    m_check_state = CHECKSTATE_HEAD;   
    return NO_REQUEST;

}

//解析http请求的一个头部信息
http_conn::HTTP_CODE http_conn::parse_head(char *text)
{
    if (text[0] == '\0')
    {
        if (m_content_length != 0)
        {
            m_check_state = CHECKSTATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }
    else if (strncasecmp(text, "Connection:", 11) == 0)
    {
        text += 11;
        text += strspn(text, " \t");
        if (strcasecmp(text, "keep-alive") == 0)
        {
            m_linger = true;
        }
    }
    else if (strncasecmp(text, "Content-length:", 15) == 0)
    {
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text);
    }
    else if (strncasecmp(text, "Host:", 5) == 0)
    {
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    }
    else
    {
        // LOG_INFO("oop!unknow header: %s", text);
    }
    return NO_REQUEST;
}

//判断http请求是否被完整读入
http_conn::HTTP_CODE http_conn::parse_content(char *text)
{
    if (m_read_idx >= (m_content_length + m_checked_idx))
    {
        text[m_content_length] = '\0';
        //POST请求中最后为输入的用户名和密码
        m_string = text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::process_read(){
    char* text = m_read_buff;
    LINE_STATE line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    while(( (m_check_state == CHECKSTATE_CONTENT) && (line_status == LINE_OK) ) || ((line_status = parse_line()) == LINE_OK) ){
        switch( m_check_state ){
            case CHECKSTATE_REQUEST:{
                if(parse_request(text) == BAD_REQUEST){
                    return BAD_REQUEST;
                }
                break;
            }
            case CHECKSTATE_HEAD:{
                ret = parse_head(text);
                if( ret == BAD_REQUEST ){
                    return BAD_REQUEST;
                }
                if( ret == GET_REQUEST ){
                    return do_request();
                }
                break;
            }
            case CHECKSTATE_CONTENT:{
                ret = parse_content(text);
                if( ret == GET_REQUEST ){
                    return do_request();
                }
                line_status = LINE_OPEN;
                break;
            }
            default:
                return INTERNAL_ERROR;
        }
        text = m_read_buff + m_checked_idx;
    }
    return BAD_REQUEST;    
}

bool http_conn::read_once(){
    if(m_read_idx > READ_BUFF_SIZE){
        return false;
    }
    int ret;
    while(1){
        ret = recv(m_sockfd, m_read_buff + m_read_idx, READ_BUFF_SIZE - m_read_idx -1, 0);
        if(ret == -1){
            if((errno == EAGAIN) || (errno == EWOULDBLOCK)){
                break;
            }
            return false;
        }
        else if(ret == 0){
            return false;
        }
        else{
            m_read_idx += ret;
        }
    }
    printf("%s", m_read_buff);
    return true;
}
// 注册、登录、校验（占坑）
http_conn::HTTP_CODE http_conn::do_request(){
    const char* doc_root="/home/bob/webserver/root";
    memset(m_file, '\0', sizeof(m_file));
    strcpy(m_file, doc_root);
    strcat(m_file, m_url);
    if( stat(m_file, &m_file_stat) <0 ){
        return NO_RESOURCE;
    }
    if(!(m_file_stat.st_mode&S_IROTH))
        return FORBIDDEN_REQUEST;
    if(S_ISDIR(m_file_stat.st_mode))
        return BAD_REQUEST;
    
    int fd = open(m_file,O_RDONLY);
    m_file_address=(char*)mmap(0,m_file_stat.st_size,PROT_READ,MAP_PRIVATE,fd,0);
    close(fd);
    return FILE_REQUEST;
}


bool http_conn::add_response(const char* format, ...){
    if(m_write_idx >= WRITE_BUFF_SIZE)
        return false;
    va_list arg_list;
    va_start(arg_list, format);
    int len = vsnprintf(m_write_buff + m_write_idx, WRITE_BUFF_SIZE -1 - m_write_idx, format, arg_list);
    va_end(arg_list);
    if(len >= WRITE_BUFF_SIZE -1 - m_write_idx)
        return false;
    m_write_idx += len;
    return true;

}

bool http_conn::add_status_line(int status, const char* title){
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}

bool http_conn::add_head(int content_length){
    add_response("Content-Length:%d\r\n",content_length);
    add_response("Connection:%s\r\n",(m_linger==true)?"keep-alive":"close");
    add_response("%s","\r\n");
    return true;
}

bool http_conn::add_content(const char* content){
    return add_response("%s", content);
}

bool http_conn::process_write(HTTP_CODE ret){
    switch(ret){
        case(INTERNAL_ERROR):{
            add_status_line(500, error_500_title);
            add_head(strlen(error_500_form));
            add_content(error_500_form);
            break;
        }
        case(BAD_REQUEST):{
            add_status_line(400, error_400_title);
            add_head(strlen(error_400_form));
            add_content(error_400_form);
            break;
        }
        case(FORBIDDEN_REQUEST):{
            add_status_line(403, error_403_title);
            add_head(strlen(error_403_form));
            add_content(error_403_form);
            break;
        }
        case(FILE_REQUEST):{
            add_status_line(200, ok_200_title);
            if( m_file_stat.st_size != 0){
                add_head(m_file_stat.st_size);
                m_iov[0].iov_base = m_write_buff;
                m_iov[0].iov_len = m_write_idx;
                m_iov[1].iov_base = m_file_address;
                m_iov[1].iov_len = m_file_stat.st_size;
                m_iov_count = 2;
                m_bytes_to_send = m_write_idx + m_file_stat.st_size;
                return true;                
            }
            else{
                const char* ok_string = "<html><body></body></html>";
                add_head(strlen(ok_string));
                add_content(ok_string);
                m_bytes_to_send = m_write_idx;
                break;
            }
        }
        default:
            return false;
    }
    m_iov[0].iov_base = m_write_buff;
    m_iov[0].iov_len = m_write_idx;
    m_iov_count = 1;
    m_bytes_to_send = m_write_idx;
    return true;
}

bool http_conn::write(){
    if(!m_bytes_to_send){
        modfd(m_epoll_fd,m_sockfd,EPOLLIN);
        init();
        return true;
    }

    int temp = 0;
    while(1){
        temp = writev(m_sockfd, m_iov, m_iov_count);
        if(temp > 0){
            m_bytes_have_sent += temp;
        }
        else if(temp < 0){
            if(errno == EAGAIN){
                if(m_bytes_have_sent > m_write_idx){
                    m_iov[0].iov_len = 0;
                    m_iov[1].iov_base += m_bytes_have_sent - m_write_idx;
                    m_iov[1].iov_len -= m_bytes_have_sent - m_write_idx;
                }
                else{
                    m_iov[0].iov_base += m_bytes_have_sent;
                    m_iov[0].iov_len -= m_bytes_have_sent;
                }
                modfd(m_epoll_fd, m_sockfd, EPOLLOUT);
                return true;
            }
            unmap();
            return false;
        }
        if(m_bytes_have_sent >= m_bytes_to_send){
            unmap();
            modfd(m_epoll_fd,m_sockfd,EPOLLIN);
            if(m_linger){
                init();
                return true;
            }
            else{
                return false;
            }
        }
    }
}

void http_conn::unmap(){
    if(m_file_address)
    {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = nullptr;
    }
}



void http_conn::close_conn(bool real_close)
{
    if (real_close && (m_sockfd != -1))
    {
        printf("close %d\n", m_sockfd);
        removefd(m_epoll_fd, m_sockfd);
        m_sockfd = -1;
        m_user_count--;
    }
}

void http_conn::process(){
    HTTP_CODE code;
    code = process_read();
    if(code == NO_REQUEST){
        modfd(m_epoll_fd, m_sockfd, EPOLLIN);
        return;
    }
    process_write(code);
    modfd(m_epoll_fd, m_sockfd, EPOLLOUT);
}