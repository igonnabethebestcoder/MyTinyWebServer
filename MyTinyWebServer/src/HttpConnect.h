#pragma once
/// Readme
/// ����httpЭ�������
/// ��������ͻ��˺�ҵ�����߼�
/// �ͻ���Э���л��ɿͻ��˴���
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

/// ----begin HTTPЭ�鴦���໺������С����----
// ��ʼ��С2KB
#define HTTP_READ_BUFFER_INIT_SIZE 2048
// �������2MB
#define HTTP_READ_BUFFER_MAX_SIZE 2048
// ��ʼ��С2KB
#define HTTP_WRITE_BUFFER_INIT_SIZE 2048
// �������2MB
#define HTTP_WRITE_BUFFER_MAX_SIZE 2048
/// ----end HTTPЭ�鴦���໺������С����----


/// ----begin ����ģʽ----
#define LT 0
#define ET 1
/// ----end ����ģʽ----

// �����ļ�Ŀ¼����������
#define FILENAME_LEN 200

// HTTP���󷽷�
enum HTTP_METHOD
{
    GET = 0,  // GET����
    POST,     // POST����
    HEAD,     // HEAD����
    PUT,      // PUT����
    DELETE,   // DELETE����
    TRACE,    // TRACE����
    OPTIONS,  // OPTIONS����
    CONNECT,  // CONNECT����
    PATH      // PATH����
};

// HTTPЭ��״̬��
enum HTTPStatusCode
{
    // �Զ���
    INCOMPLETE_REQUEST,    // ������������Ҫ������ȡ�ͻ�������
    GET_REQUEST,           // ����������Ŀͻ�������
    FILE_REQUEST,         // �ļ����󣬻�ȡ�ļ��ɹ�

    // 1xx����Ϣ��Ӧ
    CONTINUE = 100,        // �������ͻ���Ӧ��������
    SWITCHING_PROTOCOLS,   // �л�Э�飺�������������󲢽�Э���л�����һ��Э��
    PROCESSING,            // �����������Ѿ��յ��������ڴ���

    // 2xx���ɹ���Ӧ
    OK = 200,              // OK������ɹ����������ݣ�����У�
    CREATED,               // �Ѵ���������ɹ���������������Դ
    ACCEPTED,              // �ѽ��ܣ������ѽ��ܣ���δ������
    NON_AUTHORITATIVE_INFORMATION,  // ��Ȩ����Ϣ������ɹ������ص���Ϣ���������Ի����
    NO_CONTENT,            // �����ݣ�����ɹ�����û�����ݷ���
    RESET_CONTENT,         // �������ݣ�����ɹ����ͻ���Ӧ�����õ�ǰ�ĵ���ͼ
    PARTIAL_CONTENT,       // �������ݣ�����ɹ��������ص��ǲ�������

    // 3xx���ض���
    MOVED_PERMANENTLY = 301,   // �����ƶ����������Դ�ѱ������ƶ�����λ��
    FOUND,                    // ��ʱ�ƶ�����Դ��ʱ�ƶ����ͻ�����Ҫ����ʹ��ԭ URI
    SEE_OTHER,                // �鿴�������ͻ���Ӧ�������� URI ����ȡ�������Դ
    NOT_MODIFIED,             // δ�޸ģ���Դδ���޸ģ��ͻ��˿�ʹ�û���ĸ���
    USE_PROXY,                // ʹ�ô����ͻ���Ӧ��ʹ�ô�����������Դ
    TEMPORARY_REDIRECT,       // ��ʱ�ض���������Դ��ʱ�ض�����һ�� URI

    // 4xx���ͻ��˴���
    BAD_REQUEST = 400,       // �������󣺷������޷����ͻ�������
    UNAUTHORIZED,            // δ��Ȩ��������Ҫ�����֤
    FORBIDDEN = 403,               // ��ֹ��������������󣬵��ܾ�ִ��
    NOT_FOUND,               // δ�ҵ����������Դδ�ҵ�
    METHOD_NOT_ALLOWED,      // �������������󷽷�����ֹ
    NOT_ACCEPTABLE,          // ���ɽ��ܣ��������޷�������������ݷ�����Ӧ
    PROXY_AUTHENTICATION_REQUIRED, // ���������֤����Ҫ����������������֤
    REQUEST_TIMEOUT,         // ����ʱ���ͻ�������ʱ
    CONFLICT,                // ��ͻ���������Դ�뵱ǰ״̬������ͻ
    GONE,                    // ��ɾ�����������Դ���ٿ���
    LENGTH_REQUIRED,         // ��Ҫ���ݳ��ȣ�����ȱ�ٱ�Ҫ�����ݳ���ͷ
    PRECONDITION_FAILED,     // �Ⱦ�����ʧ�ܣ�������Ⱦ�����ʧ��
    PAYLOAD_TOO_LARGE,       // ��Ч���ع��������ʵ����󣬷������޷�����
    URI_TOO_LONG,            // URI����������� URI �����˷��������������
    UNSUPPORTED_MEDIA_TYPE, // ��֧�ֵ�ý�����ͣ���������֧�������ý���ʽ
    RANGE_NOT_SATISFIABLE,   // �޷�����ķ�Χ���ͻ�������ķ�Χ��������
    EXPECTATION_FAILED,      // ����ʧ�ܣ��������޷��������������

    // 5xx������������
    INTERNAL_SERVER_ERROR = 500,  // �������ڲ����󣺷��������������޷��������
    NOT_IMPLEMENTED,              // δʵ�֣���������֧������Ĺ���
    BAD_GATEWAY,                  // �������أ���������Ϊ���ػ����ʱ�յ���Ч��Ӧ
    SERVICE_UNAVAILABLE,          // ���񲻿��ã���������ʱ�����ã�ͨ���ǹ��ػ�ά��
    GATEWAY_TIMEOUT,              // ���س�ʱ����������Ϊ���ػ����ʱδ�ܼ�ʱ�յ���Ӧ
    HTTP_VERSION_NOT_SUPPORTED,   // HTTP �汾����֧�֣���������֧��������ʹ�õ� HTTP �汾
};

// ��״̬��״̬���еĶ�ȡ״̬
enum LINE_STATUS
{
    LINE_BAD = -1,  // �г���
    LINE_OK,        // ��ȡ���������� 
    LINE_OPEN       // �������в�����
};

// ��״̬����״̬
enum CHECK_STATE
{
    CHECK_STATE_REQUESTLINE = 0,  // ����������
    CHECK_STATE_HEADER,           // ��������ͷ
    CHECK_STATE_CONTENT           // ����������
};

// ��Ӧ���ĵ�content-type
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
    APPLICATION_OCTET,  // application/octet-stream (�������ļ�)
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
    UNKNOWN             // δ֪����
};

struct HttpStatusCorrespondText
{
    const char* title;
    const char* content;
};

// ����ȫ�� HTTP ״̬ӳ���
extern unordered_map<int, HttpStatusCorrespondText> http_status_map;

class HttpConnect
{
public:

    HttpConnect();
    ~HttpConnect();

    // �ⲿ��ʼ��
    void init(int sockfd, const sockaddr_in& addr, char* root, int TRIGMode = ET,
        int close_log = 0, string user = "", string passwd = "", string sqlname = "");

    // ��дfd������
    void process();

    // ������������,LT��ET����ģʽ��
    // Q: �������������ģ�
    bool readOnce();

    // ������д����
    bool write();

    // ����epollʵ��fd
    static void setHttpEpollfd(int epollfd);

    // �ر�����
    void close_conn(bool real_close = true);

public:
    static int m_epollfd;// epollʵ����fd��ȫ��httpʵ������

private:
    //��ʼ��HTTP������
    void init();

    //����HTTP���󣨲�����
    HTTPStatusCode processRead();
    //����HTTP��Ӧ����
    bool processWrite(HTTPStatusCode res);

    ///--------- ����������
    // ����������
    HTTPStatusCode parseRequestLine(char* text);
    // ��������ͷ
    HTTPStatusCode parseHeaders(char* text);
    // ����������
    HTTPStatusCode parseContent(char* text);
    // ����һ������
    LINE_STATUS parseLine();


    ///--------- ����HTTP��Ӧ����
    // �����Ӧ����
    bool add_response(const char* format, ...);
    // �����Ӧ��
    bool add_content(const char* content);
    // ���״̬��
    bool add_status_line(int status, const char* title);
    // �����Ӧͷ
    bool add_headers(int content_length, ContentType type = UNKNOWN);
    // �����������
    bool add_content_type(ContentType type = UNKNOWN);
    // ������ݳ���
    bool add_content_length(int content_length);
    // ��ӱ�������
    bool add_linger();
    // ��ӿ���
    bool add_blank_line();

    // ���ļ�������Ӧ����
    bool non_file_request_responce(int status);
    // �ļ�������Ӧ
    bool file_request_responce(int status);

    // ��ȡ��Ӧ�������͵��ַ���
    const char* getContentTypeString(ContentType type);

    // ���Һ���
    const HttpStatusCorrespondText* getHttpStatusText(int status);

    // ��ȡ��ǰ��������
    char* get_line() { return m_read_buf + m_start_line; };

private:
    int m_sockfd;  // �׽����ļ�������
    sockaddr_in m_address;  // �ͻ��˵�ַ

    CHECK_STATE m_check_state;  // ��״̬����״̬
    HTTP_METHOD m_method;  // HTTP���󷽷�

    // ��д������
    char m_read_buf[HTTP_READ_BUFFER_MAX_SIZE];  // ��������,��̬���󻺳�����С
    long m_read_idx;  // �Ѿ���ȡ���ֽ���
    long m_checked_idx;  // ��ǰ���ڷ������ֽ�λ��
    int m_start_line;  // ��ǰ���ڽ������е���ʼλ��
    char m_write_buf[HTTP_WRITE_BUFFER_MAX_SIZE];  // д������
    int m_write_idx;  // д�������д����͵��ֽ���

    int m_TRIGMode;  // ����ģʽ,LT or ET

    char m_real_file[FILENAME_LEN];  // �ͻ��������Ŀ���ļ�������·��
    char* m_url;  // �����Ŀ���ļ����ļ���
    char* m_version;  // HTTPЭ��汾
    char* m_host;  // ������
    long m_content_length;  // HTTP�������Ϣ�峤��


    bool m_linger;  // �Ƿ񱣳�����, ������������������

    // ��Ӧ��������
    char* m_file_address;  // �ͻ���������ļ���mmap���ڴ��е���ʼλ��
    struct stat m_file_stat;  // Ŀ���ļ���״̬��Ϣ
    struct iovec m_iv[2];  // ����д������iovec
    int m_iv_count;  // ��д�ڴ�������
    int cgi;  // �Ƿ�����POST����
    char* m_string;  // �洢����ͷ����
    int bytes_to_send;  // ��Ҫ���͵������ֽ���
    int bytes_have_send;  // �Ѿ����͵������ֽ���
    char* doc_root;  // ��վ�ĸ�Ŀ¼

    int m_close_log; // �Ƿ�رյ�ǰ����־��¼
};

/// ----begin HTTPЭ����ʹ�õ�epoll����������װ----

//���ļ����������÷�����
int setnonblocking(int fd);
//���ں��¼���ע����¼���ETģʽ��ѡ����EPOLLONESHOT
void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);
//���ں�ʱ���ɾ��������
void removefd(int epollfd, int fd);
//���¼�����ΪEPOLLONESHOT
void modfd(int epollfd, int fd, int ev, int TRIGMode);

/// ----end HTTPЭ����ʹ�õ�epoll����������װ----

#endif // !HTTP_CONNECT_H


