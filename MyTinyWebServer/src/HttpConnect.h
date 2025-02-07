#pragma once
/// Readme
/// 处理http协议的请求
/// 独立解耦客户端和业务处理逻辑
/// 客户端协议切换由客户端处理
/// 
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <map>
#include <unordered_map>
#include "Log.h"

#ifndef HTTP_CONNECT_H
#define HTTP_CONNECT_H

/// ----begin HTTP协议处理类缓冲区大小限制----
// 初始大小2KB
#define HTTP_READ_BUFFER_INIT_SIZE 2048
// 最大限制2MB
#define HTTP_READ_BUFFER_MAX_SIZE 2048
// 初始大小2KB
#define HTTP_WRITE_BUFFER_INIT_SIZE 2048
// 最大限制2MB
#define HTTP_WRITE_BUFFER_MAX_SIZE 2048
/// ----end HTTP协议处理类缓冲区大小限制----


/// ----begin 触发模式----
#define LT 0
#define ET 1
/// ----end 触发模式----

// 请求文件目录缓冲区长度
#define FILENAME_LEN 200

// HTTP请求方法
enum HTTP_METHOD
{
    GET = 0,  // GET请求
    POST,     // POST请求
    HEAD,     // HEAD请求
    PUT,      // PUT请求
    DELETE,   // DELETE请求
    TRACE,    // TRACE请求
    OPTIONS,  // OPTIONS请求
    CONNECT,  // CONNECT请求
    PATH      // PATH请求
};

// HTTP协议状态码
enum HTTPStatusCode
{
    // 自定义
    INCOMPLETE_REQUEST,    // 请求不完整，需要继续读取客户端数据
    GET_REQUEST,           // 获得了完整的客户端请求
    FILE_REQUEST,         // 文件请求，获取文件成功

    // 1xx：信息响应
    CONTINUE = 100,        // 继续：客户端应继续请求
    SWITCHING_PROTOCOLS,   // 切换协议：服务器接受请求并将协议切换到另一个协议
    PROCESSING,            // 处理：服务器已经收到请求并正在处理

    // 2xx：成功响应
    OK = 200,              // OK：请求成功，返回数据（如果有）
    CREATED,               // 已创建：请求成功，并创建了新资源
    ACCEPTED,              // 已接受：请求已接受，但未处理完
    NON_AUTHORITATIVE_INFORMATION,  // 非权威信息：请求成功，返回的信息可能是来自缓存的
    NO_CONTENT,            // 无内容：请求成功，但没有内容返回
    RESET_CONTENT,         // 重置内容：请求成功，客户端应该重置当前文档视图
    PARTIAL_CONTENT,       // 部分内容：请求成功，但返回的是部分数据

    // 3xx：重定向
    MOVED_PERMANENTLY = 301,   // 永久移动：请求的资源已被永久移动到新位置
    FOUND,                    // 临时移动：资源临时移动，客户端需要继续使用原 URI
    SEE_OTHER,                // 查看其他：客户端应访问其他 URI 来获取请求的资源
    NOT_MODIFIED,             // 未修改：资源未被修改，客户端可使用缓存的副本
    USE_PROXY,                // 使用代理：客户端应该使用代理来访问资源
    TEMPORARY_REDIRECT,       // 临时重定向：请求资源临时重定向到另一个 URI

    // 4xx：客户端错误
    BAD_REQUEST = 400,       // 错误请求：服务器无法理解客户端请求
    UNAUTHORIZED,            // 未授权：请求需要身份验证
    FORBIDDEN = 403,               // 禁止：服务器理解请求，但拒绝执行
    NOT_FOUND,               // 未找到：请求的资源未找到
    METHOD_NOT_ALLOWED,      // 方法不允许：请求方法被禁止
    NOT_ACCEPTABLE,          // 不可接受：服务器无法根据请求的内容返回响应
    PROXY_AUTHENTICATION_REQUIRED, // 代理身份验证：需要代理服务器的身份验证
    REQUEST_TIMEOUT,         // 请求超时：客户端请求超时
    CONFLICT,                // 冲突：请求的资源与当前状态发生冲突
    GONE,                    // 已删除：请求的资源不再可用
    LENGTH_REQUIRED,         // 需要内容长度：请求缺少必要的内容长度头
    PRECONDITION_FAILED,     // 先决条件失败：请求的先决条件失败
    PAYLOAD_TOO_LARGE,       // 有效负载过大：请求的实体过大，服务器无法处理
    URI_TOO_LONG,            // URI过长：请求的 URI 超过了服务器处理的限制
    UNSUPPORTED_MEDIA_TYPE, // 不支持的媒体类型：服务器不支持请求的媒体格式
    RANGE_NOT_SATISFIABLE,   // 无法满足的范围：客户端请求的范围不可满足
    EXPECTATION_FAILED,      // 期望失败：服务器无法满足请求的期望

    // 5xx：服务器错误
    INTERNAL_SERVER_ERROR = 500,  // 服务器内部错误：服务器遇到错误，无法完成请求
    NOT_IMPLEMENTED,              // 未实现：服务器不支持请求的功能
    BAD_GATEWAY,                  // 错误网关：服务器作为网关或代理时收到无效响应
    SERVICE_UNAVAILABLE,          // 服务不可用：服务器暂时不可用，通常是过载或维护
    GATEWAY_TIMEOUT,              // 网关超时：服务器作为网关或代理时未能及时收到响应
    HTTP_VERSION_NOT_SUPPORTED,   // HTTP 版本不受支持：服务器不支持请求中使用的 HTTP 版本
};

// 从状态机状态，行的读取状态
enum LINE_STATUS
{
    LINE_BAD = -1,  // 行出错
    LINE_OK,        // 读取到完整的行 
    LINE_OPEN       // 行数据尚不完整
};

// 主状态机的状态
enum CHECK_STATE
{
    CHECK_STATE_REQUESTLINE = 0,  // 解析请求行
    CHECK_STATE_HEADER,           // 解析请求头
    CHECK_STATE_CONTENT           // 解析请求体
};

// 响应报文的content-type
enum  ContentType 
{
    TEXT_HTML,          // text/html
    TEXT_CSS,           // text/css
    TEXT_PLAIN,         // text/plain
    TEXT_JAVASCRIPT,    // text/javascript
    APPLICATION_JSON,   // application/json
    APPLICATION_XML,    // application/xml
    APPLICATION_PDF,    // application/pdf
    APPLICATION_ZIP,    // application/zip
    APPLICATION_OCTET,  // application/octet-stream (二进制文件)
    IMAGE_JPEG,         // image/jpeg
    IMAGE_PNG,          // image/png
    IMAGE_GIF,          // image/gif
    IMAGE_SVG,          // image/svg+xml
    IMAGE_WEBP,         // image/webp
    AUDIO_MPEG,         // audio/mpeg
    AUDIO_OGG,          // audio/ogg
    VIDEO_MP4,          // video/mp4
    VIDEO_WEBM,         // video/webm
    VIDEO_OGG,          // video/ogg
    MULTIPART_FORMDATA, // multipart/form-data
    APPLICATION_X_WWW_FORM_URLENCODED, // application/x-www-form-urlencoded
    UNKNOWN             // 未知类型
};

struct HttpStatusCorrespondText
{
    const char* title;
    const char* content;
};

// 声明全局 HTTP 状态映射表
extern unordered_map<int, HttpStatusCorrespondText> http_status_map;

class HttpConnect
{
public:

    HttpConnect();
    ~HttpConnect();

    // 外部初始化
    void init(int sockfd, const sockaddr_in& addr, char* root, int TRIGMode = ET,
        int close_log = 0, string user = "", string passwd = "", string sqlname = "");

    // 读写fd总流程
    void process();

    // 非阻塞读操作,LT和ET两种模式，
    // Q: 非阻塞体现在哪？
    bool readOnce();

    // 非阻塞写操作
    bool write();

    // 设置epoll实例fd
    static void setHttpEpollfd(int epollfd);

    // 关闭连接
    void close_conn(bool real_close = true);

public:
    static int m_epollfd;// epoll实例表fd，全部http实例公用

private:
    //初始化HTTP处理类
    void init();

    //解析HTTP请求（不处理）
    HTTPStatusCode processRead();
    //生成HTTP响应报文
    bool processWrite(HTTPStatusCode res);

    ///--------- 解析请求函数
    // 解析请求行
    HTTPStatusCode parseRequestLine(char* text);
    // 解析请求头
    HTTPStatusCode parseHeaders(char* text);
    // 解析请求体
    HTTPStatusCode parseContent(char* text);
    // 解析一行内容
    LINE_STATUS parseLine();


    ///--------- 生成HTTP响应函数
    // 添加响应内容
    bool add_response(const char* format, ...);
    // 添加响应体
    bool add_content(const char* content);
    // 添加状态行
    bool add_status_line(int status, const char* title);
    // 添加响应头
    bool add_headers(int content_length, ContentType type = UNKNOWN);
    // 添加内容类型
    bool add_content_type(ContentType type = UNKNOWN);
    // 添加内容长度
    bool add_content_length(int content_length);
    // 添加保持连接
    bool add_linger();
    // 添加空行
    bool add_blank_line();

    // 非文件请求响应函数
    bool non_file_request_responce(int status);
    // 文件请求响应
    bool file_request_responce(int status);

    // 获取响应报文类型的字符串
    const char* getContentTypeString(ContentType type);

    // 查找函数
    const HttpStatusCorrespondText* getHttpStatusText(int status);

    // 获取当前解析的行
    char* get_line() { return m_read_buf + m_start_line; };

private:
    int m_sockfd;  // 套接字文件描述符
    sockaddr_in m_address;  // 客户端地址

    CHECK_STATE m_check_state;  // 主状态机的状态
    HTTP_METHOD m_method;  // HTTP请求方法

    // 读写缓冲区
    char m_read_buf[HTTP_READ_BUFFER_MAX_SIZE];  // 读缓冲区,动态增大缓冲区大小
    long m_read_idx;  // 已经读取的字节数
    long m_checked_idx;  // 当前正在分析的字节位置
    int m_start_line;  // 当前正在解析的行的起始位置
    char m_write_buf[HTTP_WRITE_BUFFER_MAX_SIZE];  // 写缓冲区
    int m_write_idx;  // 写缓冲区中待发送的字节数

    int m_TRIGMode;  // 触发模式,LT or ET

    char m_real_file[FILENAME_LEN];  // 客户端请求的目标文件的完整路径
    char* m_url;  // 请求的目标文件的文件名
    char* m_version;  // HTTP协议版本
    char* m_host;  // 主机名
    long m_content_length;  // HTTP请求的消息体长度


    bool m_linger;  // 是否保持连接, 保持连接则重置数据

    // 响应报文属性
    char* m_file_address;  // 客户端请求的文件被mmap到内存中的起始位置
    struct stat m_file_stat;  // 目标文件的状态信息
    struct iovec m_iv[2];  // 用于写操作的iovec
    int m_iv_count;  // 被写内存块的数量
    int cgi;  // 是否启用POST方法
    char* m_string;  // 存储请求头数据
    int bytes_to_send;  // 将要发送的数据字节数
    int bytes_have_send;  // 已经发送的数据字节数
    char* doc_root;  // 网站的根目录

    int m_close_log; // 是否关闭当前类日志记录
};

/// ----begin HTTP协议类使用的epoll操作函数封装----

//对文件描述符设置非阻塞
int setnonblocking(int fd);
//将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);
//从内核时间表删除描述符
void removefd(int epollfd, int fd);
//将事件重置为EPOLLONESHOT
void modfd(int epollfd, int fd, int ev, int TRIGMode);

/// ----end HTTP协议类使用的epoll操作函数封装----

#endif // !HTTP_CONNECT_H


