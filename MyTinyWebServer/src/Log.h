#pragma once
/// ----��־ϵͳ�汾�ƻ�----
/// ------V1.0------
/// 1.��־����[INFO...]
/// 2.ͬ����־
/// 3.��־�ļ�������Ŀ�ļ����е�log�ļ�����
/// 4.�����ڣ��죩����־�ļ����
/// 5.���õ���ģʽ
/// 
/// 
#ifndef LOG_H
#define LOG_H

#include <iostream>
#include <string.h>
#include <fstream>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>
#include <sys/time.h>
using namespace std;


// ----��־����----
#define DEBUG 1
#define INFO 2
#define WARN 3
#define ERR 4
// --------


class Log
{
public:
	Log();
	~Log();

	// ����ģʽ������c++���Ա�֤�̰߳�ȫ��������Ҫ˫�������
	static Log* getInstance();

	// ��ʼ������
	bool init();

	// д��Ϣ����log�ļ�����
	void writeLog(int level, const string format, ...);

	// ��ȡ��������
	string get_current_date();


private:
	string filePath; // ��ǰ������־�ļ����ļ���
	fstream logfile; // ��log���ļ�ָ��
	int date;        //��Ϊ�������,��¼��ǰʱ������һ��
	string logstr;
	// ����������־��server�����������Զ���
	int m_close_log; // �ر���־��ǣ�0�رգ�1����
};

// ÿһ�����ú꺯�������ж�Ӧ����m_close_log����
#define LOG_DEBUG(format, ...) if(0 == m_close_log) {Log::getInstance()->writeLog(1, format, ##__VA_ARGS__);}
#define LOG_INFO(format, ...) if(0 == m_close_log) {Log::getInstance()->writeLog(2, format, ##__VA_ARGS__);}
#define LOG_WARN(format, ...) if(0 == m_close_log) {Log::getInstance()->writeLog(3, format, ##__VA_ARGS__);}
#define LOG_ERROR(format, ...) if(0 == m_close_log) {Log::getInstance()->writeLog(4, format, ##__VA_ARGS__);}

#endif