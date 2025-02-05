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
    //���ɵ���־�ļ���bin/x64/Debug�У���.out�ļ�һ��
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

    // �����logstr
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

    // �����ڲ�ͬ��ʱ�򣬴������ļ������Ҹ�������date
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

    // �������յ���־�ַ���������ʱ�������־������־���ݵȣ�
    char buf[1024]; // ��ʱ��������Ÿ�ʽ���������

    int len = snprintf(buf, sizeof(buf), "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
        my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
        my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec,
        logstr.c_str());

    // ʹ�� vsnprintf ����ʽ��ʣ�����־����
    vsnprintf(buf + len, sizeof(buf) - len, format.c_str(), valst);

    // ����ʽ�������־����׷�ӵ� logstr
    logstr += buf;

    va_end(valst);

    // ����־����д���ļ�
    if (logfile.is_open())
    {
        logfile << logstr << std::endl; // д����־������
        logfile.flush(); // ȷ����־��ʱд�����
    }
}

string Log::get_current_date() {
	// ��ȡ��ǰʱ��
	time_t now = time(nullptr);
	tm* local_time = localtime(&now);

	// ʹ�� stringstream ��ʽ������
	stringstream date_stream;
	date_stream << put_time(local_time, "%Y-%m-%d");
	return date_stream.str();
}

//#define TEST_LOG_H
#ifdef TEST_LOG_H
int main()
{
    // ��ȡ��־ʵ��
    Log* log = Log::getInstance();

    int m_close_log = 0;

    // ��ʼ����־ϵͳ
    if (log->init()) {
        std::cout << "Log initialized successfully." << std::endl;
    }
    else {
        std::cerr << "Log initialization failed." << std::endl;
        return -1;
    }

    // ����д��ͬ�������־
    log->writeLog(INFO, "This is an info log message with value: %d", 100);
    log->writeLog(DEBUG, "This is a debug log message.");
    log->writeLog(WARN, "This is a warning message.");
    log->writeLog(ERR, "This is an error log message with error code: %d", -1);

    // ������־�����ڷ��ļ�
    // ���Ե��ö�Σ���֤���ڱ仯ʱ��־�ļ��Ƿ��л�
    for (int i = 0; i < 5; ++i) {
        log->writeLog(INFO, "Log test iteration %d", i + 1);
    }

    std::cout << "Logs have been written successfully." << std::endl;


    char str[6] = "nihao";
    LOG_INFO("%s", str);
    return 0;
}

#endif