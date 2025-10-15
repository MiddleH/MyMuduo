#include "Logger.h"
#include "Timestamp.h"

#include <iostream>

// 获取唯一实例
Logger &Logger::instance()
{
    static Logger logger;
    return logger;
}
// 设置日志级别
void Logger::setLogLevel(int logLevel)
{
    logLevel_ = logLevel;
}
// 写日志 [级别信息]时间信息 : 具体信息
void Logger::log(std::string msg)
{
    switch (logLevel_)
    {
    case INFO:
        std::cout << "[INFO]";
        break;
    case ERROR:
        std::cout << "[ERROR]";
        break;
    case FATAL:
        std::cout << "[FATAL]";
        break;
    case DEBUG:
        std::cout << "[DEBUG]";
        break;
    default:
        break;
    }

    // 打印时间信息和具体信息
    std::cout <<Timestamp::now().toString()<< " : " << msg << std::endl;
}
