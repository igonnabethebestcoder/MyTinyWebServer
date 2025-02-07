#include "HttpConnect.h"

// ��ʼ����̬����m_epollfd
int HttpConnect::m_epollfd = -1;

// ���� HTTP ״̬��Ͷ�Ӧ��Ϣ�Ĺ�ϣ��
unordered_map<int, HttpStatusCorrespondText> http_status_map = {
    // �Զ���״̬
    {INCOMPLETE_REQUEST, {"Incomplete Request", "The request is incomplete and needs more data from the client.\n"}},
    {GET_REQUEST, {"Get Request", nullptr}},

    // 1xx ��Ϣ��Ӧ
    {CONTINUE, {"Continue", nullptr}},
    {SWITCHING_PROTOCOLS, {"Switching Protocols", nullptr}},
    {PROCESSING, {"Processing", nullptr}},

    // 2xx �ɹ���Ӧ
    {OK, {"OK", nullptr}},
    {CREATED, {"Created", nullptr}},
    {ACCEPTED, {"Accepted", nullptr}},
    {NON_AUTHORITATIVE_INFORMATION, {"Non-Authoritative Information", nullptr}},
    {NO_CONTENT, {"No Content", nullptr}},
    {RESET_CONTENT, {"Reset Content", nullptr}},
    {PARTIAL_CONTENT, {"Partial Content", nullptr}},

    // 3xx �ض���
    {MOVED_PERMANENTLY, {"Moved Permanently", nullptr}},
    {FOUND, {"Found", nullptr}},
    {SEE_OTHER, {"See Other", nullptr}},
    {NOT_MODIFIED, {"Not Modified", nullptr}},
    {USE_PROXY, {"Use Proxy", nullptr}},
    {TEMPORARY_REDIRECT, {"Temporary Redirect", nullptr}},

    // 4xx �ͻ��˴���
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

    // 5xx ����������
    {INTERNAL_SERVER_ERROR, {"Internal Server Error", "The server encountered an unexpected error and could not complete your request.\n"}},
    {NOT_IMPLEMENTED, {"Not Implemented", "The server does not support the requested functionality.\n"}},
    {BAD_GATEWAY, {"Bad Gateway", "The server received an invalid response from the upstream server.\n"}},
    {SERVICE_UNAVAILABLE, {"Service Unavailable", "The server is currently unable to handle the request due to temporary overload or maintenance.\n"}},
    {GATEWAY_TIMEOUT, {"Gateway Timeout", "The server, acting as a gateway, did not receive a timely response from the upstream server.\n"}},
    {HTTP_VERSION_NOT_SUPPORTED, {"HTTP Version Not Supported", "The server does not support the HTTP version used in the request.\n"}}
};

//���ļ����������÷�����
/*Ŀ�ģ���ָ�����ļ���������fd������Ϊ������ģʽ��
  ������ģʽ��ζ�ŵ�Ӧ�ó�����ж�ȡ��д��ʱ�����û�����ݿ��ã�
  read �� write ��������������������������һ���������� EAGAIN����*/
int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK; // ����Ϊ������
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

//���ں��¼���ע����¼���ETģʽ��ѡ����EPOLLONESHOT
/*Ŀ�ģ����ļ������� fd ע�ᵽ epoll �¼������У�֧�ֱ�Ե������ET����ˮƽ������LT��ģʽ���Լ�ѡ���Ƿ����� EPOLLONESHOT��
  EPOLLIN����ʾ���ļ��������ɶ���
  EPOLLET�����ñ�Ե����ģʽ��ET����
  EPOLLRDHUP����Զ�������ر�����ʱ��epoll �ᴥ�����¼���
  EPOLLONESHOT���¼�����һ�κ���Զ��� epoll ���Ƴ���ͨ��������ֹ��δ�����*/
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
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);// ע���¼�����
    setnonblocking(fd);
}

//���ں�ʱ���ɾ��������
void removefd(int epollfd, int fd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

//���¼�����ΪEPOLLONESHOT
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

    //�������������������ʱ����������վ��Ŀ¼�����http��Ӧ��ʽ������߷��ʵ��ļ���������ȫΪ��
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

// ʹ��״̬������http������
HTTPStatusCode HttpConnect::processRead()
{
    LINE_STATUS line_status = LINE_OK;
    HTTPStatusCode ret = INCOMPLETE_REQUEST;
    char* text = 0;
    /*��GET�������У�ÿһ�ж���\r\n��Ϊ���������ԶԱ��Ľ��в��ʱ�����ô�״̬����״̬line_status=parse_line())==LINE_OK��伴�ɡ�
      ������POST�������У���Ϣ���ĩβû���κ��ַ������Բ���ʹ�ô�״̬����״̬������ת��ʹ����״̬����״̬��Ϊѭ�����������*/
    while ((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || ((line_status = parseLine()) == LINE_OK))
    {
        text = get_line();
        // ÿcheck��һ�У��͸�������ʼλ��
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
                return FILE_REQUEST; // ������ԣ�ֱ�ӷ����ļ�����ɹ�״̬
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


// ����Ӧ���ݷ���m_write_bufд���������Լ���iovec��write�����оۼ�д��
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

// LT��ET���ַ�ʽ��ȡ����
bool HttpConnect::readOnce()
{
    if (m_read_idx >= HTTP_READ_BUFFER_MAX_SIZE)
    {
        return false;
    }
    int bytes_read = 0;

    //LT��ȡ����
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
    //ET������
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

// ��http��Ӧ����д���û�fd
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

            // ����������ӣ���������
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

//����http���е�epollʵ��fd
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

// ����������״̬��
// requestLine --> <Method> \t<Request Target> \t<HTTP-Version> \r\n
HTTPStatusCode HttpConnect::parseRequestLine(char* text)
{
    // �ҵ���һ�� "�ո�+\t"
    m_url = strpbrk(text, " \t");
    if (!m_url)
    {
        return BAD_REQUEST;
    }
    *m_url++ = '\0'; // ���ո��滻��\0

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

    m_url += strspn(m_url, " \t"); // ������ͷ�Ŀո�����Ʊ��
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
    //��urlΪ/ʱ����ʾĬ�Ͻ���
    if (strlen(m_url) == 1)
        strcat(m_url, "judge.html");
    m_check_state = CHECK_STATE_HEADER;
    return INCOMPLETE_REQUEST;
}

//����http�����һ��ͷ����Ϣ
HTTPStatusCode HttpConnect::parseHeaders(char* text)
{
    /*���������body�ǿ��У����ȱ�parseLine��\r\n�滻Ϊ\0\0, 
      ����getLine��startָ���滻��\0\0��\r\n��
      ���ڿ��л��������������\r\n��\r \n \r \n
                                       ^
                                     start
                                 \r \n \0 \0
                                             ^
                                           check*/
    if (text[0] == '\0') // ���������
    {
        // ��������峤���Ƿ�Ϊ0����Ϊ0��ʾ������ղ�����
        if (m_content_length != 0)
        {
            // ��״̬����������������body
            m_check_state = CHECK_STATE_CONTENT;

            // �����Ĳ������������������
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
        //POST���������Ϊ������û���������
        m_string = text;
        return GET_REQUEST;
    }
    return INCOMPLETE_REQUEST;
}

//��http�����İ�\r\n���ֳ���������
//\r\n�ɶԳ���
LINE_STATUS HttpConnect::parseLine()
{
    char temp;
    for (; m_checked_idx < m_read_idx; ++m_checked_idx)
    {
        temp = m_read_buf[m_checked_idx];
        if (temp == '\r')
        {
            // �Ѷ������һ���ַ�����û��\n
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

// ��д������ `m_write_buf` ����Ӹ�ʽ���� HTTP ��Ӧ����
// ����ֵ�����д��ɹ����� true�����򷵻� false
bool HttpConnect::add_response(const char* format, ...)
{
    // ��鵱ǰд�������Ƿ񳬳�������������ƣ�����Խ��д��
    if (m_write_idx >= HTTP_WRITE_BUFFER_MAX_SIZE)
        return false;

    // ���� `arg_list` �������ڴ洢�ɱ����
    va_list arg_list;
    va_start(arg_list, format);

    // ʹ�� `vsnprintf` ��ʽ���ɱ�������ݲ�д�� `m_write_buf`
    // - `m_write_buf + m_write_idx`���ӵ�ǰд��������ʼд��
    // - `HTTP_WRITE_BUFFER_MAX_SIZE - 1 - m_write_idx`��ȷ�����ᳬ����������Χ
    // - `format`����ʽ���ַ���
    // - `arg_list`���ɱ�����б�
    int len = vsnprintf(m_write_buf + m_write_idx, HTTP_WRITE_BUFFER_MAX_SIZE - 1 - m_write_idx, format, arg_list);

    // ��� `vsnprintf` ���ص� `len` �Ƿ񳬳�ʣ����û������ռ䣬��ֹ���
    if (len >= (HTTP_WRITE_BUFFER_MAX_SIZE - 1 - m_write_idx))
    {
        va_end(arg_list);
        return false;
    }

    // ����д��������ȷ��������������ȷ׷��
    m_write_idx += len;

    // �����ɱ��������
    va_end(arg_list);

    // ��¼��־����ӡ��ǰд������������
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
    if (type == UNKNOWN) // ������Ӧͷ������ͣ�������������ж�
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

// ���ļ���Ӧ����Ӧ��ȷ��
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

// ��ȡ Content-Type �ַ���
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
    default: return "application/octet-stream"; // δ֪����Ĭ��Ϊ��������
    }
}

// ���Һ���
const HttpStatusCorrespondText* HttpConnect::getHttpStatusText(int status) {
    auto it = http_status_map.find(status);
    return (it != http_status_map.end()) ? &it->second : nullptr;
}

#define TEST_HTTP_H
#ifdef TEST_HTTP_H
int main()
{
#ifdef strspn_USE
    // strpn(str1, str2)���÷�
    const char* str1 = "abc123xyz";
    const char* str2 = "bac"; // ֻ�����ַ� a, b, c
    size_t len = strspn(str1, str2);

    printf("Number of leading 'a', 'b', or 'c': %zu\n", len);
#endif

    HttpConnect user;

    //��������socket
    int m_listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(m_listenfd >= 0);

    //�󶨷�������ַ
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY); //�󶨵� ���п��õ� IP ��ַ��0.0.0.0��
    address.sin_port = htons(8080);

    int flag = 1;
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));

    int ret = bind(m_listenfd, (struct sockaddr*)&address, sizeof(address));
    assert(ret >= 0);

    //��������
    ret = listen(m_listenfd, 5);
    assert(ret >= 0);

    //����epoll �ں��¼���
    epoll_event events[1024];
    int m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);
    HttpConnect::setHttpEpollfd(m_epollfd);

    //��Ӽ���socket��epoll
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

            //�����µ��Ŀͻ�����
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
            //����ͻ������Ͻ��յ�������
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