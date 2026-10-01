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
    constexpr int THREAD_CNT_PER_IO_CONTEXT = 2;

    // STOMP server urls

    // redis conn
    constexpr char REDIS_HOST_IP[] = "127.0.0.1";
    constexpr char REDIS_PORT[] = "6379";
    constexpr char REDIS_PUB_SUB_CHANNEL[] = "chat";
    constexpr int REDIS_CONN_WAIT_TIMEOUT_SECONDS = 3;

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
