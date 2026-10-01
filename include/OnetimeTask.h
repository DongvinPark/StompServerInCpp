//
// Created by 박동빈 on 2026-10-01.
//

#ifndef ONETIMETASK_H
#define ONETIMETASK_H

#include <boost/asio.hpp>
#include <functional>

#include "../include/Logger.h"

class OnetimeTask
{
public:

    explicit OnetimeTask(
      boost::asio::io_context& input_io_context,
      int input_interval_ms,
      std::function<void()> input_task
    );

    ~OnetimeTask();
    
    void start();

private:
    void scheduleTask();

    std::shared_ptr<Logger> logger;
    boost::asio::strand<boost::asio::io_context::executor_type> strand;
    boost::asio::steady_timer timer;
    std::chrono::milliseconds interval;
    std::function<void()> task;
    std::atomic<bool> is_executed{false};
};

#endif //ONETIMETASK_H
