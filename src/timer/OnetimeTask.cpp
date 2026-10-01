//
// Created by 박동빈 on 2026-10-01.
//
#include "../include/OnetimeTask.h"
#include "../include/Logger.h"
#include "../constants/C.h"

OnetimeTask::OnetimeTask(
    boost::asio::io_context& input_io_context,
    const int input_interval_ms,
    std::function<void()> input_task
) : logger(Logger::getLogger(C::ONETIME_TASK)),
    strand(boost::asio::make_strand(input_io_context)),
    timer(input_io_context),
    interval(std::chrono::milliseconds(input_interval_ms)),
    task(std::move(input_task))
{
}

OnetimeTask::~OnetimeTask()
{
}

void OnetimeTask::start()
{
    if (is_executed.load())
    {
        logger->warning("Already executed");
        return;
    }
    scheduleTask();
}

void OnetimeTask::scheduleTask()
{
    timer.expires_after(interval);
    timer.async_wait(
        boost::asio::bind_executor(
            strand,
            [this](boost::system::error_code ec)
            {
                if (!ec)
                {
                    if (task)
                    {
                        is_executed.store(true);
                        task();
                    }
                } else
                {
                    logger->severe("boost steady timer failed! error : " + ec.message());
                }
            }
            )
        );
}
