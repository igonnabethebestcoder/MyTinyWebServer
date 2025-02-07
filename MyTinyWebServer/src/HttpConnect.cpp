#include "HttpConnect.h"

// 初始化近态变量m_epollfd
int HttpConnect::m_epollfd = -1;

// 定义 HTTP 状态码和对应信息的哈希表
unordered_map<int, HttpStatusCorrespondText> http_status_map = {
    // 自定义状态
    {INCOMPLETE_REQUEST, {"Incomplete Request", "The request is incomplete and needs more data from the client.\n"}},
    {GET_REQUEST, {"Get Request", nullptr}},

    // 1xx 信息响应
    {CONTINUE, {"Continue", nullptr}},
    {SWITCHING_PROTOCOLS, {"Switching Protocols", nullptr}},
    {PROCESSING, {"Processing", nullptr}},

    // 2xx 成功响应
    {OK, {"OK", nullptr}},
    {CREATED, {"Created", nullptr}},
    {ACCEPTED, {"Accepted", nullptr}},
    {NON_AUTHORITATIVE_INFORMATION, {"Non-Authoritative Information", nullptr}},
    {NO_CONTENT, {"No Content", nullptr}},
    {RESET_CONTENT, {"Reset Content", nullptr}},
    {PARTIAL_CONTENT, {"Partial Content", nullptr}},

    // 3xx 重定向
    {MOVED_PERMANENTLY, {"Moved Permanently", nullptr}},
    {FOUND, {"Found", nullptr}},
    {SEE_OTHER, {"See Other", nullptr}},
    {NOT_MODIFIED, {"Not Modified", nullptr}},
    {USE_PROXY, {"Use Proxy", nullptr}},
    {TEMPORARY_REDIRECT, {"Temporary Redirect", nullptr}},

    // 4xx 客户端错误
    {BAD_REQUEST, {"Bad Request", "Your request has bad syntax or is inherently impossible to satisfy.\n"}},
    {UNAUTHORIZED, {"Unauthorized", "You are not authorized to access this resource.\n"}},
    {FORBIDDEN, {"Forbidden", "You do not have permission to access this resource.\n"}},
    {NOT_FOUND, {"Not Found", "The requested resource could not be found on this server.\n"}},
    {METHOD_NOT_ALLOWED, {"Method Not Allowed", "The requested method is not allowed for this resource.\n"}},
    {NOT_ACCEPTABLE, {"Not Acceptable", "The requested resource is not available in a format acceptable to your browser.\n"}},
    {PROXY_AUTHENTICATION_REQUIRED, {"Proxy Authentication Required", nullptr}},
    {REQUEST_TIMEOUT, {"Request Timeout", "The server timed out waiting for the request.\n"}},
    {CONFLICT, {"Conflict", "The request could not be completed due to a conflict with the current state of the target resource.\n"}},
    {GONE, {"Gone", "The requested resource is no longer available and has been permanently removed.\n"}},
    {LENGTH_REQUIRED, {"Length Required", "A valid Content-Length header is required for this request.\n"}},
    {PRECONDITION_FAILED, {"Precondition Failed", "A precondition for this request failed.\n"}},
    {PAYLOAD_TOO_LARGE, {"Payload Too Large", "The request payload is too large for the server to process.\n"}},
    {URI_TOO_LONG, {"URI Too Long", "The request URI is too long for the server to handle.\n"}},
    {UNSUPPORTED_MEDIA_TYPE, {"Unsupported Media Type", "The request entity has a media type which the server does not support.\n"}},
    {RANGE_NOT_SATISFIABLE, {"Range Not Satisfiable", "The requested range cannot be satisfied.\n"}},
    {EXPECTATION_FAILED, {"Expectation Failed", "The server cannot meet the expectations specified in the request header.\n"}},

    // 5xx 服务器错误
    {INTERNAL_SERVER_ERROR, {"Internal Server Error", "The server encountered an unexpected error and could not complete your request.\n"}},
    {NOT_IMPLEMENTED, {"Not Implemented", "The server does not support the requested functionality.\n"}},
    {BAD_GATEWAY, {"Bad Gateway", "The server received an invalid response from the upstream server.\n"}},
    {SERVICE_UNAVAILABLE, {"Service Unavailable", "The server is currently unable to handle the request due to temporary overload or maintenance.\n"}},
    {GATEWAY_TIMEOUT, {"Gateway Timeout", "The server, acting as a gateway, did not receive a timely response from the upstream server.\n"}},
    {HTTP_VERSION_NOT_SUPPORTED, {"HTTP Version Not Supported", "The server does not support the HTTP version used in the request.\n"}}
};

//对文件描述符设置非阻塞
/*目的：将指定的文件描述符（fd）设置为非阻塞模式。
  非阻塞模式意味着当应用程序进行读取或写入时，如果没有数据可用，
  read 或 write 操作不会阻塞，而是立即返回一个错误（例如 EAGAIN）。*/
int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK; // 设置为非阻塞
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

//将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
/*目的：将文件描述符 fd 注册到 epoll 事件监听中，支持边缘触发（ET）或水平触发（LT）模式，以及选择是否启用 EPOLLONESHOT。
  EPOLLIN：表示该文件描述符可读。
  EPOLLET：启用边缘触发模式（ET）。
  EPOLLRDHUP：当远程主机关闭连接时，epoll 会触发该事件。
  EPOLLONESHOT：事件触发一次后会自动从 epoll 中移除，通常用来防止多次触发。*/
void addfd(int epollfd, int fd, bool one_shot, int TRIGMode)
{
    epoll_event event;
    event.data.fd = fd;

    if (TRIGMode == ET)
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else
        event.events = EPOLLIN | EPOLLRDHUP;

    if (one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);// 注册事件监听
    setnonblocking(fd);
}

//从内核时间表删除描述符
void removefd(int epollfd, int fd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

//将事件重置为EPOLLONESHOT
void modfd(int epollfd, int fd, int ev, int TRIGMode)
{
    epoll_event event;
    event.data.fd = fd;

    if (TRIGMode == ET)
        event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    else
        event.events = ev | EPOLLONESHOT | EPOLLRDHUP;

    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

HttpConnect::HttpConnect()
{

}

HttpConnect::~HttpConnect()
{

}

void HttpConnect::init(int sockfd, const sockaddr_in& addr, char* root, int TRIGMode, int close_log, string user, string passwd, string sqlname)
{
    m_sockfd = sockfd;
    m_address = addr;

    addfd(m_epollfd, sockfd, true, m_TRIGMode);

    //当浏览器出现连接重置时，可能是网站根目录出错或http响应格式出错或者访问的文件中内容完全为空
    doc_root = root;
    m_TRIGMode = TRIGMode;
    m_close_log = close_log;

    init();
}

void HttpConnect::process()
{
    HTTPStatusCode read_ret = processRead();
    if (read_ret == INCOMPLETE_REQUEST)
    {
        modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
        return;
    }
    bool write_ret = processWrite(read_ret);
    if (!write_ret)
    {
        cerr << "processWrite() in process() went wrong" << endl;
        close_conn();
    }
    modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);
}

// 使用状态机处理http请求报文
HTTPStatusCode HttpConnect::processRead()
{
    LINE_STATUS line_status = LINE_OK;
    HTTPStatusCode ret = INCOMPLETE_REQUEST;
    char* text = 0;
    /*在GET请求报文中，每一行都是\r\n作为结束，所以对报文进行拆解时，仅用从状态机的状态line_status=parse_line())==LINE_OK语句即可。
      但，在POST请求报文中，消息体的末尾没有任何字符，所以不能使用从状态机的状态，这里转而使用主状态机的状态作为循环入口条件。*/
    while ((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || ((line_status = parseLine()) == LINE_OK))
    {
        text = get_line();
        // 每check完一行，就更新新起始位置
        m_start_line = m_checked_idx;

        LOG_INFO("%s", text);
        switch (m_check_state)
        {
        case CHECK_STATE_REQUESTLINE:
        {
            ret = parseRequestLine(text);
            if (ret == BAD_REQUEST)
                return BAD_REQUEST;
            break;
        }
        case CHECK_STATE_HEADER:
        {
            ret = parseHeaders(text);
            if (ret == BAD_REQUEST)
                return BAD_REQUEST;
            else if (ret == GET_REQUEST)
            {
                //return do_request();
                return FILE_REQUEST; // 这里测试，直接返回文件请求成功状态
            }
            break;
        }
        case CHECK_STATE_CONTENT:
        {
            ret = parseContent(text);
            if (ret == GET_REQUEST)
                //return do_request();
                return FILE_REQUEST;
            line_status = LINE_OPEN;
            break;
        }
        default:
            return INTERNAL_SERVER_ERROR;
        }
    }
    return INCOMPLETE_REQUEST;
}


// 将响应内容放入m_write_buf写缓冲区，以及绑定iovec在write函数中聚集写入
bool HttpConnect::processWrite(HTTPStatusCode res)
{
    switch (res)
    {
    case INTERNAL_SERVER_ERROR:
    {
        if (!non_file_request_responce(INTERNAL_SERVER_ERROR))
            return false;
        break;
    }
    case BAD_REQUEST:
    {
        if (!non_file_request_responce(BAD_REQUEST))
            return false;
        break;
    }
    case FORBIDDEN:
    {
        if (!non_file_request_responce(FORBIDDEN))
            return false;
        break;
    }
    case FILE_REQUEST:
    {
        if (!file_request_responce(OK))
            return false;
        break;
    }
    default:
        return false;
    }
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    bytes_to_send = m_write_idx;
    return true;
}

// LT和ET两种方式读取数据
bool HttpConnect::readOnce()
{
    if (m_read_idx >= HTTP_READ_BUFFER_MAX_SIZE)
    {
        return false;
    }
    int bytes_read = 0;

    //LT读取数据
    if (0 == m_TRIGMode)
    {
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, HTTP_READ_BUFFER_MAX_SIZE - m_read_idx, 0);
        m_read_idx += bytes_read;

        if (bytes_read <= 0)
        {
            return false;
        }

        return true;
    }
    //ET读数据
    else
    {
        while (true)
        {
            bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, HTTP_READ_BUFFER_MAX_SIZE - m_read_idx, 0);
            if (bytes_read == -1)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;
                return false;
            }
            else if (bytes_read == 0)
            {
                return false;
            }
            m_read_idx += bytes_read;
        }
        return true;
    }
}

// 将http响应报文写入用户fd
bool HttpConnect::write()
{
    int temp = 0;

    if (bytes_to_send == 0)
    {
        modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
        cout << "nothing to send to " << m_sockfd << endl;
        init();
        return true;
    }

    while (1)
    {
        temp = writev(m_sockfd, m_iv, m_iv_count);

        if (temp < 0)
        {
            if (errno == EAGAIN)
            {
                modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);
                return true;
            }
            //unmap();
            return false;
        }

        bytes_have_send += temp;
        bytes_to_send -= temp;
        if (bytes_have_send >= m_iv[0].iov_len)
        {
            m_iv[0].iov_len = 0;
            m_iv[1].iov_base = m_file_address + (bytes_have_send - m_write_idx);
            m_iv[1].iov_len = bytes_to_send;
        }
        else
        {
            m_iv[0].iov_base = m_write_buf + bytes_have_send;
            m_iv[0].iov_len = m_iv[0].iov_len - bytes_have_send;
        }

        if (bytes_to_send <= 0)
        {
            //unmap();
            modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);

            // 如果保持连接，重置属性
            if (m_linger)
            {
                init();
                cout << "keeping connect" << endl;
                return true;
            }
            else
            {

                return false;
            }
        }
    }
}

//设置http类中的epoll实例fd
void HttpConnect::setHttpEpollfd(int epollfd)
{
    if (epollfd >= 0)
        HttpConnect::m_epollfd = epollfd;
    else
    {
        cerr << "bad epoll fd" << endl;
        exit(1);
    }
}

void HttpConnect::close_conn(bool real_close)
{
    if (real_close && (m_sockfd != -1))
    {
        printf("close %d\n", m_sockfd);
        removefd(m_epollfd, m_sockfd);
        m_sockfd = -1;
    }
    removefd(m_epollfd, m_sockfd);
}

//
void HttpConnect::init()
{
    //mysql = NULL;
    bytes_to_send = 0;
    bytes_have_send = 0;
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_linger = false;
    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_content_length = 0;
    m_host = 0;
    m_start_line = 0;
    m_checked_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    cgi = 0;
    //m_state = 0;
    //timer_flag = 0;
    //improv = 0;

    memset(m_read_buf, '\0', HTTP_READ_BUFFER_MAX_SIZE);
    memset(m_write_buf, '\0', HTTP_WRITE_BUFFER_MAX_SIZE);
    memset(m_real_file, '\0', FILENAME_LEN);
}

// 解析请求报文状态行
// requestLine --> <Method> \t<Request Target> \t<HTTP-Version> \r\n
HTTPStatusCode HttpConnect::parseRequestLine(char* text)
{
    // 找到第一个 "空格+\t"
    m_url = strpbrk(text, " \t");
    if (!m_url)
    {
        return BAD_REQUEST;
    }
    *m_url++ = '\0'; // 将空格替换成\0

    char* method = text;
    if (strcasecmp(method, "GET") == 0)
        m_method = GET;
    else if (strcasecmp(method, "POST") == 0)
    {
        m_method = POST;
        cgi = 1;
    }
    else
        return BAD_REQUEST;

    m_url += strspn(m_url, " \t"); // 跳过开头的空格或者制表符
    m_version = strpbrk(m_url, " \t");
    if (!m_version)
        return BAD_REQUEST;
    *m_version++ = '\0';
    m_version += strspn(m_version, " \t");
    if (strcasecmp(m_version, "HTTP/1.1") != 0)
        return BAD_REQUEST;
    if (strncasecmp(m_url, "http://", 7) == 0)
    {
        m_url += 7;
        m_url = strchr(m_url, '/');
    }

    if (strncasecmp(m_url, "https://", 8) == 0)
    {
        m_url += 8;
        m_url = strchr(m_url, '/');
    }

    if (!m_url || m_url[0] != '/')
        return BAD_REQUEST;
    //当url为/时，显示默认界面
    if (strlen(m_url) == 1)
        strcat(m_url, "judge.html");
    m_check_state = CHECK_STATE_HEADER;
    return INCOMPLETE_REQUEST;
}

//解析http请求的一个头部信息
HTTPStatusCode HttpConnect::parseHeaders(char* text)
{
    /*如果请求体body是空行，则先被parseLine将\r\n替换为\0\0, 
      调用getLine后将start指向被替换成\0\0的\r\n，
      由于空行会产生两个相连的\r\n即\r \n \r \n
                                       ^
                                     start
                                 \r \n \0 \0
                                             ^
                                           check*/
    if (text[0] == '\0') // 请求体空行
    {
        // 检查请求体长度是否为0，不为0表示请求接收不完整
        if (m_content_length != 0)
        {
            // 将状态机改至解析请求体body
            m_check_state = CHECK_STATE_CONTENT;

            // 请求报文不完整，后续处理错误
            return INCOMPLETE_REQUEST;
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
        LOG_INFO("oop!unknow header: %s", text);
    }
    return INCOMPLETE_REQUEST;
}

HTTPStatusCode HttpConnect::parseContent(char* text)
{
    if (m_read_idx >= (m_content_length + m_checked_idx))
    {
        text[m_content_length] = '\0';
        //POST请求中最后为输入的用户名和密码
        m_string = text;
        return GET_REQUEST;
    }
    return INCOMPLETE_REQUEST;
}

//将http请求报文按\r\n区分成三个部分
//\r\n成对出现
LINE_STATUS HttpConnect::parseLine()
{
    char temp;
    for (; m_checked_idx < m_read_idx; ++m_checked_idx)
    {
        temp = m_read_buf[m_checked_idx];
        if (temp == '\r')
        {
            // 已读到最后一个字符但是没有\n
            if ((m_checked_idx + 1) == m_read_idx)
                return LINE_OPEN;
            else if (m_read_buf[m_checked_idx + 1] == '\n')
            {
                m_read_buf[m_checked_idx++] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if (temp == '\n')
        {
            if (m_checked_idx > 1 && m_read_buf[m_checked_idx - 1] == '\r')
            {
                m_read_buf[m_checked_idx - 1] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

// 向写缓冲区 `m_write_buf` 中添加格式化的 HTTP 响应数据
// 返回值：如果写入成功返回 true，否则返回 false
bool HttpConnect::add_response(const char* format, ...)
{
    // 检查当前写入索引是否超出缓冲区最大限制，避免越界写入
    if (m_write_idx >= HTTP_WRITE_BUFFER_MAX_SIZE)
        return false;

    // 定义 `arg_list` 变量用于存储可变参数
    va_list arg_list;
    va_start(arg_list, format);

    // 使用 `vsnprintf` 格式化可变参数内容并写入 `m_write_buf`
    // - `m_write_buf + m_write_idx`：从当前写入索引开始写入
    // - `HTTP_WRITE_BUFFER_MAX_SIZE - 1 - m_write_idx`：确保不会超出缓冲区范围
    // - `format`：格式化字符串
    // - `arg_list`：可变参数列表
    int len = vsnprintf(m_write_buf + m_write_idx, HTTP_WRITE_BUFFER_MAX_SIZE - 1 - m_write_idx, format, arg_list);

    // 检查 `vsnprintf` 返回的 `len` 是否超出剩余可用缓冲区空间，防止溢出
    if (len >= (HTTP_WRITE_BUFFER_MAX_SIZE - 1 - m_write_idx))
    {
        va_end(arg_list);
        return false;
    }

    // 更新写入索引，确保后续内容能正确追加
    m_write_idx += len;

    // 结束可变参数处理
    va_end(arg_list);

    // 记录日志，打印当前写缓冲区的内容
    LOG_INFO("request:%s", m_write_buf);

    return true;
}

bool HttpConnect::add_content(const char* content)
{
	return add_response("%s", content);
}

bool HttpConnect::add_status_line(int status, const char* title)
{
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}

bool HttpConnect::add_headers(int content_length, ContentType type)
{
    return add_content_type(type) && add_content_length(content_length) && add_linger() && add_blank_line();
}

bool HttpConnect::add_content_type(ContentType type)
{
    if (type == UNKNOWN) // 不在响应头添加类型，让浏览器自行判断
        return true;
	return add_response("Content-Type:%s\r\n", getContentTypeString(type));
}

bool HttpConnect::add_content_length(int content_length)
{
    return add_response("Content-Length:%d\r\n", content_length);
}

bool HttpConnect::add_linger()
{
	return add_response("Connection:%s\r\n", (m_linger == true) ? "keep-alive" : "close");
}

bool HttpConnect::add_blank_line()
{
	return add_response("%s", "\r\n");
}

// 非文件响应，响应已确定
bool HttpConnect::non_file_request_responce(int status)
{
    const HttpStatusCorrespondText* text = getHttpStatusText(status);
    add_status_line(status, text->title);
    if (text->content == nullptr)
        return false;
    add_headers(strlen(text->content));
    if (!add_content(text->content))
        return false;
    return true;
}

bool HttpConnect::file_request_responce(int status)
{
    const HttpStatusCorrespondText* text = getHttpStatusText(status);
    add_status_line(status, text->title);
    if (text->content != nullptr)
        cerr << "file_request_responce() : content is not nullptr!" << endl;
    if (m_file_stat.st_size != 0)
    {
        add_headers(m_file_stat.st_size);
        m_iv[0].iov_base = m_write_buf;
        m_iv[0].iov_len = m_write_idx;
        m_iv[1].iov_base = m_file_address;
        m_iv[1].iov_len = m_file_stat.st_size;
        m_iv_count = 2;
        bytes_to_send = m_write_idx + m_file_stat.st_size;
        return true;
    }
    else
    {
        const char* ok_string = "<html><body>nihao wo shi tang min</body></html>";
        add_headers(strlen(ok_string));
        if (!add_content(ok_string))
            return false;
        return true;
    }
}

// 获取 Content-Type 字符串
const char* HttpConnect::getContentTypeString(ContentType type)
{
    switch (type) {
    case ContentType::TEXT_HTML: return "text/html";
    case ContentType::TEXT_CSS: return "text/css";
    case ContentType::TEXT_PLAIN: return "text/plain";
    case ContentType::TEXT_JAVASCRIPT: return "text/javascript";
    case ContentType::APPLICATION_JSON: return "application/json";
    case ContentType::APPLICATION_XML: return "application/xml";
    case ContentType::APPLICATION_PDF: return "application/pdf";
    case ContentType::APPLICATION_ZIP: return "application/zip";
    case ContentType::APPLICATION_OCTET: return "application/octet-stream";
    case ContentType::IMAGE_JPEG: return "image/jpeg";
    case ContentType::IMAGE_PNG: return "image/png";
    case ContentType::IMAGE_GIF: return "image/gif";
    case ContentType::IMAGE_SVG: return "image/svg+xml";
    case ContentType::IMAGE_WEBP: return "image/webp";
    case ContentType::AUDIO_MPEG: return "audio/mpeg";
    case ContentType::AUDIO_OGG: return "audio/ogg";
    case ContentType::VIDEO_MP4: return "video/mp4";
    case ContentType::VIDEO_WEBM: return "video/webm";
    case ContentType::VIDEO_OGG: return "video/ogg";
    case ContentType::MULTIPART_FORMDATA: return "multipart/form-data";
    case ContentType::APPLICATION_X_WWW_FORM_URLENCODED: return "application/x-www-form-urlencoded";
    default: return "application/octet-stream"; // 未知类型默认为二进制流
    }
}

// 查找函数
const HttpStatusCorrespondText* HttpConnect::getHttpStatusText(int status) {
    auto it = http_status_map.find(status);
    return (it != http_status_map.end()) ? &it->second : nullptr;
}

#define TEST_HTTP_H
#ifdef TEST_HTTP_H
int main()
{
#ifdef strspn_USE
    // strpn(str1, str2)的用法
    const char* str1 = "abc123xyz";
    const char* str2 = "bac"; // 只考虑字符 a, b, c
    size_t len = strspn(str1, str2);

    printf("Number of leading 'a', 'b', or 'c': %zu\n", len);
#endif

    HttpConnect user;

    //创建监听socket
    int m_listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(m_listenfd >= 0);

    //绑定服务器地址
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY); //绑定到 所有可用的 IP 地址（0.0.0.0）
    address.sin_port = htons(8080);

    int flag = 1;
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));

    int ret = bind(m_listenfd, (struct sockaddr*)&address, sizeof(address));
    assert(ret >= 0);

    //开启监听
    ret = listen(m_listenfd, 5);
    assert(ret >= 0);

    //创建epoll 内核事件表
    epoll_event events[1024];
    int m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);
    HttpConnect::setHttpEpollfd(m_epollfd);

    //添加监听socket到epoll
    addfd(m_epollfd, m_listenfd, false, ET);

    while (true)
    {
        int number = epoll_wait(m_epollfd, events, 1024, -1);
        if (number < 0 && errno != EINTR)
        {
            cout << "epoll failure" << endl;
            break;
        }

        for (int i = 0; i < number; i++)
        {
            int sockfd = events[i].data.fd;

            //处理新到的客户连接
            if (sockfd == m_listenfd)
            {
                struct sockaddr_in client_address;
                socklen_t client_addrlength = sizeof(client_address);
                while (1)
                {
                    int connfd = accept(m_listenfd, (struct sockaddr*)&client_address, &client_addrlength);
                    if (connfd < 0)
                    {
                        cout << ("%s:errno is:%d", "accept error", errno);
                        break;
                    }

                    user.init(connfd, client_address, nullptr, ET, 0);
                    cout << "client init successfully" << endl;
                }
            }
            //处理客户连接上接收到的数据
            else if (events[i].events & EPOLLIN)
            {
                if (user.readOnce())
                {
                    user.process();
                    cout << "enter process()" << endl;
                }
            }
            else if (events[i].events & EPOLLOUT)
            {
                cout << "enter server write" << endl;
                user.write();
            }
        }
    }
    return 0;
}
#endif