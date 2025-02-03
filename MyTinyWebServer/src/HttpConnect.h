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
    NO_REQUEST,          // ������������Ҫ������ȡ�ͻ�������
    GET_REQUEST,         // ����������Ŀͻ�������

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
    FORBIDDEN,               // ��ֹ��������������󣬵��ܾ�ִ��
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
    LINE_OK = 0,  // ��ȡ����������
    LINE_BAD,     // �г���
    LINE_OPEN     // �������в�����
};

// ��״̬����״̬
enum CHECK_STATE
{
    CHECK_STATE_REQUESTLINE = 0,  // ����������
    CHECK_STATE_HEADER,           // ��������ͷ
    CHECK_STATE_CONTENT           // ����������
};

class HttpConnect
{
public:
    HttpConnect();
    ~HttpConnect();
    //����HTTP���󣨲�����
    HTTPStatusCode processRead();
    //����HTTP��Ӧ����
    HTTPStatusCode processWrite();

    // ������������,LT��ET����ģʽ��
    // Q: �������������ģ�
    bool readOnce();
    // ������д����
    bool write();

private:
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
    bool add_headers(int content_length);
    // �����������
    bool add_content_type();
    // ������ݳ���
    bool add_content_length(int content_length);
    // ��ӱ�������
    bool add_linger();
    // ��ӿ���
    bool add_blank_line();


private:
    int m_sockfd;  // �׽����ļ�������
    sockaddr_in m_address;  // �ͻ��˵�ַ

    CHECK_STATE m_check_state;  // ��״̬����״̬
    HTTP_METHOD m_method;  // HTTP���󷽷�

    //��д������
    char* m_read_buf;  // ��������
    long m_read_idx;  // �Ѿ���ȡ���ֽ���
    long m_checked_idx;  // ��ǰ���ڷ������ֽ�λ��
    int m_start_line;  // ��ǰ���ڽ������е���ʼλ��
    char* m_write_buf;  // д������
    int m_write_idx;  // д�������д����͵��ֽ���

    int m_TRIGMode;  // ����ģʽ,LT or ET

    char m_real_file[FILENAME_LEN];  // �ͻ��������Ŀ���ļ�������·��
    char* m_url;  // �����Ŀ���ļ����ļ���
    char* m_version;  // HTTPЭ��汾
    char* m_host;  // ������
    long m_content_length;  // HTTP�������Ϣ�峤��


    bool m_linger;  // �Ƿ񱣳�����,opt ���Ƿ���Client��


    char* m_file_address;  // �ͻ���������ļ���mmap���ڴ��е���ʼλ��
    struct stat m_file_stat;  // Ŀ���ļ���״̬��Ϣ
    struct iovec m_iv[2];  // ����д������iovec
    int m_iv_count;  // ��д�ڴ�������
    int cgi;  // �Ƿ�����POST����
    char* m_string;  // �洢����ͷ����
    int bytes_to_send;  // ��Ҫ���͵������ֽ���
    int bytes_have_send;  // �Ѿ����͵������ֽ���
    char* doc_root;  // ��վ�ĸ�Ŀ¼

};
#endif // !HTTP_CONNECT_H


