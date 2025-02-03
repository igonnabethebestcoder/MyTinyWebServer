#include "HttpConnect.h"

HTTPStatusCode HttpConnect::processRead()
{
    /*LINE_STATUS line_status = LINE_OK;
    HTTPStatusCode ret = NO_REQUEST;
    char* text = 0;

    while ((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || ((line_status = parseLine()) == LINE_OK))
    {
        text = get_line();
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
                return do_request();
            }
            break;
        }
        case CHECK_STATE_CONTENT:
        {
            ret = parse_content(text);
            if (ret == GET_REQUEST)
                return do_request();
            line_status = LINE_OPEN;
            break;
        }
        default:
            return INTERNAL_ERROR;
        }
    }*/
    return NO_REQUEST;
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

HTTPStatusCode HttpConnect::parseRequestLine(char* text)
{
	return HTTPStatusCode();
}

HTTPStatusCode HttpConnect::parseHeaders(char* text)
{
	return HTTPStatusCode();
}

HTTPStatusCode HttpConnect::parseContent(char* text)
{
	return HTTPStatusCode();
}

LINE_STATUS HttpConnect::parseLine()
{
	return LINE_STATUS();
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
