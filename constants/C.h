//
// Created by 박동빈 on 2026. 9. 23..
//

#ifndef C_H
#define C_H

namespace C
{
    // version
    constexpr char VER[] = "1.0.0";

    // class names
    constexpr char MAIN[] = "main";

    // boost::asio::io_context thread pool cnt
    constexpr int THREAD_CNT_PER_WORKER_IO_CONTEXT = 3;

    // STOMP server urls

    // class names
    constexpr char MY_NAME[] = "StompServerInCpp";
    constexpr char PERIODIC_TASK[] = "PeriodicTask";
    constexpr char REDIS_MSG_SUBSCRIBER[] = "RedisMessageSubscriber";
    constexpr char SESSION[] = "Session";
    constexpr char STOMP_HANDLER[] = "StompHandler";

    // general constants
    constexpr char EMPTY_STR[] = "";
    constexpr int INVALID = -1;
    constexpr int UNSET = -1;

}

#endif //C_H
