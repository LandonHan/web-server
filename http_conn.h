#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H

#include <unistd.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <assert.h>
#include <pthread.h>
#include <sys/uio.h>
#include "locker.h"

class http_conn {
public:
    // 文件名的最大长度
    static const int FILENAME_LEN = 200;
    // 读写缓冲区的大小
    static const int READ_BUFFER_SIZE = 2048;
    static const int WRITE_BUFFER_SIZE = 1024;
    // HTTP请求方法，目前仅支持GET
    enum METHOD { GET = 0, POST, HEAD, PUT, DELETE,
                  TRACE, OPTIONS, CONNECT, PATCH };
    // 用于解析客户请求的主状态机的状态
    enum CHECK_STATE { CHECK_STATE_REQUESTLINE = 0,
                       CHECK_STATE_HEADER,
                       CHECK_STATE_CONTENT };
    // 针对客户的请求可能有的处理结果，根据这些结果返回响应的状态码
    enum HTTP_CODE { NO_REQUEST, GET_REQUEST, BAD_REQUEST,
                     NO_RESOURCE, FORBIDDEN_REQUEST, FILE_REQUEST,
                     INTERNAL_ERROR, CLOSED_CONNECTION };
    // 用于解析每一行的从状态机的状态
    enum LINE_STATUS { LINE_OK = 0, LINE_BAD, LINE_OPEN };

public:
    http_conn(){}
    ~http_conn(){}

public:
    // 初始化新连接
    void init( int sockfd, const sockaddr_in& addr );
    // 关闭连接
    void close_conn( bool real_close = true );
    // 处理客户请求
    void process();
    // 监测到可读事件时调用，非阻塞读
    bool read();
    // 监测到可写事件时调用，非阻塞写
    bool write();

private:
    // 初始化连接
    void init();
    // 处理收到的HTTP请求
    HTTP_CODE process_read();
    // 根据process_read处理解析的结果生成HTTP应答报文
    bool process_write( HTTP_CODE ret );

    // 这组函数将被process_read调用，在不同状态下处理请求
    HTTP_CODE parse_request_line( char* text );
    HTTP_CODE parse_headers( char* text );
    HTTP_CODE parse_content( char* text );
    HTTP_CODE do_request();
    char* get_line() { return m_read_buf + m_start_line; }
    LINE_STATUS parse_line();

    // 这组函数将被process_write调用，往HTTP应答报文中添加内容
    void unmap();
    bool add_response(const char* format, ... );
    bool add_content( const char* content );
    bool add_status_line( int status, const char* title );
    bool add_headers( int content_length );
    bool add_content_length( int content_length );
    bool add_linger();
    bool add_blank_line();

public:
    // 标识内核事件表的文件描述符
    static int m_epollfd;
    // 用户数量
    static int m_user_count;

private:
    // 标识当前HTTP连接的socket
    int m_sockfd;
    // 当前HTTP连接对方的socket地址
    sockaddr_in m_address;

    // 读缓冲区
    char m_read_buf[ READ_BUFFER_SIZE ];
    // 标识读缓冲区中已读入数据的下一个位置
    int m_read_idx;
    // 标识当前正在分析处理的字符在读缓冲区中的位置
    int m_checked_idx;
    // 标识当前正在解析的行的起始位置
    int m_start_line;
    // 写缓冲区
    char m_write_buf[ WRITE_BUFFER_SIZE ];
    // 标识写缓冲区中待发送的字节数
    int m_write_idx;

    // 主状态机当前所处的状态
    CHECK_STATE m_check_state;
    // 请求方法
    METHOD m_method;

    // 客户请求的目标文件的完整路径（请求文件功能将在之后陆续实现）
    char m_real_file[ FILENAME_LEN ];
    // 客户端请求的目标文件名
    char* m_url;
    // HTTP协议版本号，目前仅支持1.1
    char* m_version;
    // 主机名
    char* m_host;
    // HTTP请求的消息体的长度
    int m_content_length;
    // HTTP请求是否要求保持连接
    bool m_linger;

    // 客户端请求的目标文件被mmap到内存中的起始位置
    char* m_file_address;
    // 目标文件的状态
    struct stat m_file_stat;
    // 采用writev写的时候需要使用到的iovec结构体
    struct iovec m_iv[2];
    // 被写内存块的数量
    int m_iv_count;
};

#endif