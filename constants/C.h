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

    // general constants
    constexpr char MY_NAME[] = "StompServerInCpp/1.1.1";
    constexpr char PERIODIC_TASK[] = "PeriodicTask";

}

#endif //C_H
