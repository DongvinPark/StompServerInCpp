//
// Created by 박동빈 on 2026. 9. 23..
//

#ifndef C_H
#define C_H

namespace C
{
    // version
    constexpr char VER[] = "1.0.0";

    // STOMP server urls & port
    constexpr int STOMP_PORT = 8080;
    constexpr char STOMP_SERVER_END_POINT[] = "/gs-guide-websocket";
    constexpr int HEART_BEAT_MS = 33'000; // 33 초.
    constexpr int HEART_BEAT_THRESHOLD_MS = 10 * 60 * 1000; // 10 분.
    constexpr char HEART_BEAT_RESULT[] = "heartbeat";

    // class names
    constexpr char MAIN[] = "main";
    constexpr char MY_NAME[] = "StompServerInCpp";
    constexpr char REDIS_SERVICE[] = "RedisService";
    constexpr char PERIODIC_TASK[] = "PeriodicTask";
    constexpr char ONETIME_TASK[] = "OnetimeTask";
    constexpr char REDIS_MSG_SUBSCRIBER[] = "RedisMessageSubscriber";
    constexpr char SERVER[] = "Server";
    constexpr char SESSION[] = "Session";
    constexpr char STOMP_HANDLER[] = "StompHandler";
    constexpr char MSG_BROKER[] = "MsgBroker";

    // boost::asio::io_context thread pool cnt
    constexpr int THREAD_CNT_PER_IO_CONTEXT = 2;

    // redis conn
    constexpr char REDIS_HOST_IP[] = "127.0.0.1";
    constexpr char REDIS_PORT[] = "6379";
    constexpr char REDIS_PUB_SUB_CHANNEL[] = "chat";
    constexpr int REDIS_CONN_WAIT_TIMEOUT_SECONDS = 3;
    constexpr int REDIS_CONN_WAIT_TIMEOUT_MS = 3000;

    // general constants
    constexpr char EMPTY_STR[] = "";
    constexpr char HEART_BEAT_STR[] = "\n";
    constexpr char HEART_BEAT_CMD[] = "HEART_BEAT";
    constexpr int INVALID = -1;
    constexpr int UNSET = -1;
    constexpr int CLOSED_SESSION_REMOVAL_INTERVAL_MS = 30'000;
    constexpr int ONETIME_TASK_CLEAN_INTERVAL_MS = 30'000;
    constexpr char STOMP_FRAME_NUL_OCTET = '\0';
    constexpr char SINGLE_BACK_SLASH_CHAR = '\n';

}

#endif //C_H
