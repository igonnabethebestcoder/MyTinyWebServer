#include "HttpConnect.h"

HttpConnect::HttpConnect()
{
}

HttpConnect::~HttpConnect()
{
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
            }
            break;
        }
        case CHECK_STATE_CONTENT:
        {
            ret = parseContent(text);
            if (ret == GET_REQUEST)
                //return do_request();
            line_status = LINE_OPEN;
            break;
        }
        default:
            return INTERNAL_SERVER_ERROR;
        }
    }
    return INCOMPLETE_REQUEST;
}



HTTPStatusCode HttpConnect::processWrite()
{
	return HTTPStatusCode();
}

bool HttpConnect::readOnce()
{
	return false;
}

bool HttpConnect::write()
{
	return false;
}

//
void HttpConnect::init()
{
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

bool HttpConnect::add_response(const char* format, ...)
{
	return false;
}

bool HttpConnect::add_content(const char* content)
{
	return false;
}

bool HttpConnect::add_status_line(int status, const char* title)
{
	return false;
}

bool HttpConnect::add_headers(int content_length)
{
	return false;
}

bool HttpConnect::add_content_type()
{
	return false;
}

bool HttpConnect::add_content_length(int content_length)
{
	return false;
}

bool HttpConnect::add_linger()
{
	return false;
}

bool HttpConnect::add_blank_line()
{
	return false;
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

    return 0;
}
#endif