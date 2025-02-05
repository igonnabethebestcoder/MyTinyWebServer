#include "Log.h"
Log::Log()
{

}
Log::~Log()
{
	if (logfile.is_open()) {
		logfile.close();
	}
}

Log* Log::getInstance()
{
	static Log instance;
	return &instance;
}

bool Log::init()
{
    //生成的日志文件在bin/x64/Debug中，和.out文件一起
	filePath = get_current_date() + ".log";
	logfile.open(filePath, ios::app);
    time_t t = time(NULL);
    struct tm* sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;
    date = my_tm.tm_mday;
    m_close_log = 0;
	if (!logfile.is_open())
	{
		cerr << "Can't open log file : " << filePath << endl;
        return false;
	}
    return true;
}


void Log::writeLog(int level, const string format, ...)
{
    struct timeval now = { 0, 0 };
    gettimeofday(&now, NULL);
    time_t t = now.tv_sec;
    struct tm* sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;

    // 清理旧logstr
    logstr.clear();

    switch (level)
    {
    case DEBUG:
        logstr += "[DEBUG]:";
        break;
    case INFO:
        logstr += "[INFO]:";
        break;
    case WARN:
        logstr += "[WARN]:";
        break;
    case ERR:
        logstr += "[ERR]:";
        break;
    default:
        logstr += "[DEBUG]:";
        break;
    }

    // 当日期不同的时候，创建新文件，并且更行日期date
    if (date != my_tm.tm_mday) //everyday log
    {
        filePath = "../log/" + get_current_date() + ".log";

        if (logfile.is_open())
        {
            logfile.flush();
            logfile.close();
        }
        else return;

        date = my_tm.tm_mday;

        logfile.open(filePath, ios::app);
    }

    va_list valst;
    va_start(valst, format);

    // 计算最终的日志字符串（包括时间戳、日志级别、日志内容等）
    char buf[1024]; // 临时缓冲区存放格式化后的内容

    int len = snprintf(buf, sizeof(buf), "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
        my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
        my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec,
        logstr.c_str());

    // 使用 vsnprintf 来格式化剩余的日志内容
    vsnprintf(buf + len, sizeof(buf) - len, format.c_str(), valst);

    // 将格式化后的日志内容追加到 logstr
    logstr += buf;

    va_end(valst);

    // 将日志内容写入文件
    if (logfile.is_open())
    {
        logfile << logstr << std::endl; // 写入日志并换行
        logfile.flush(); // 确保日志即时写入磁盘
    }
}

string Log::get_current_date() {
	// 获取当前时间
	time_t now = time(nullptr);
	tm* local_time = localtime(&now);

	// 使用 stringstream 格式化日期
	stringstream date_stream;
	date_stream << put_time(local_time, "%Y-%m-%d");
	return date_stream.str();
}

//#define TEST_LOG_H
#ifdef TEST_LOG_H
int main()
{
    // 获取日志实例
    Log* log = Log::getInstance();

    int m_close_log = 0;

    // 初始化日志系统
    if (log->init()) {
        std::cout << "Log initialized successfully." << std::endl;
    }
    else {
        std::cerr << "Log initialization failed." << std::endl;
        return -1;
    }

    // 测试写不同级别的日志
    log->writeLog(INFO, "This is an info log message with value: %d", 100);
    log->writeLog(DEBUG, "This is a debug log message.");
    log->writeLog(WARN, "This is a warning message.");
    log->writeLog(ERR, "This is an error log message with error code: %d", -1);

    // 测试日志按日期分文件
    // 可以调用多次，验证日期变化时日志文件是否切换
    for (int i = 0; i < 5; ++i) {
        log->writeLog(INFO, "Log test iteration %d", i + 1);
    }

    std::cout << "Logs have been written successfully." << std::endl;


    char str[6] = "nihao";
    LOG_INFO("%s", str);
    return 0;
}

#endif