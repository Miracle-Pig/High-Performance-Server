#include <iostream>
#include "../src/log.h"

int main(int argc, char** argv) {
    HPS::Logger::ptr logger(new HPS::Logger);
    //! 添加输出到控制台的日志输出器
    logger->addAppender(HPS::LogAppender::ptr(new HPS::StdoutLogAppender));
    //! 添加输出到文件的日志输出器
    HPS::FileLogAppender::ptr file_appender(new HPS::FileLogAppender("./log.txt"));
    logger->addAppender(file_appender);
    //! 重新设置日志格式
    HPS::LogFormatter::ptr fmt(new HPS::LogFormatter("%d%T%p%T%m%n"));
    file_appender->setFormatter(fmt);
    file_appender->setLevel(HPS::LogLevel::ERROR);

    

    //HPS::LogEvent::ptr event(new HPS::LogEvent(__FILE__, __LINE__, 0, HPS::GetThreadId(), HPS::GetFiberId(), time(0)));
    //event->getSS() << "hello HPS log";
    //logger->log(HPS::LogLevel::DEBUG, event);

    std::cout << "hello HPS log" << std::endl;
    LOG_DEBUG(logger) << "test macro";
    
    return 0;
}
