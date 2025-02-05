#pragma once
/// ----日志系统版本计划----
/// ------V1.0------
/// 1.日志分类[INFO...]
/// 2.同步日志
/// 3.日志文件放在项目文件夹中的log文件夹中
/// 4.按日期（天）对日志文件编号
/// 5.采用单例模式
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


// ----日志级别----
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

	// 懒汉模式，利用c++特性保证线程安全，否则需要双检测上锁
	static Log* getInstance();

	// 初始化单例
	bool init();

	// 写消息进入log文件调用
	void writeLog(int level, const string format, ...);

	// 获取当天日期
	string get_current_date();


private:
	string filePath; // 当前操作日志文件的文件名
	fstream logfile; // 打开log的文件指针
	int date;        //因为按天分类,记录当前时间是那一天
	string logstr;
	// 开不开启日志由server决定，该属性多余
	int m_close_log; // 关闭日志标记，0关闭，1开启
};

// 每一个调用宏函数的类中都应该由m_close_log属性
#define LOG_DEBUG(format, ...) if(0 == m_close_log) {Log::getInstance()->writeLog(1, format, ##__VA_ARGS__);}
#define LOG_INFO(format, ...) if(0 == m_close_log) {Log::getInstance()->writeLog(2, format, ##__VA_ARGS__);}
#define LOG_WARN(format, ...) if(0 == m_close_log) {Log::getInstance()->writeLog(3, format, ##__VA_ARGS__);}
#define LOG_ERROR(format, ...) if(0 == m_close_log) {Log::getInstance()->writeLog(4, format, ##__VA_ARGS__);}

#endif