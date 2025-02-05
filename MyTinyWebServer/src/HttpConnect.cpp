#include "HttpConnect.h"

HttpConnect::HttpConnect()
{
}

HttpConnect::~HttpConnect()
{
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
    // strpn(str1, str2)���÷�
    const char* str1 = "abc123xyz";
    const char* str2 = "bac"; // ֻ�����ַ� a, b, c
    size_t len = strspn(str1, str2);

    printf("Number of leading 'a', 'b', or 'c': %zu\n", len);
#endif

    return 0;
}
#endif