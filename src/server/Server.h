//
// Created by 박동빈 on 2026. 9. 28..
//

#ifndef SERVER_H
#define SERVER_H

#include <boost/asio.hpp>
#include <boost/beast.hpp>

#include <string>

#include "../include/Logger.h"
#include "../constants/C.h"
#include "../include/PeriodicTask.h"

using boost::asio::ip::tcp;
namespace websocket = boost::beast::websocket;

class Session;

class Server
{
public:
    explicit Server(
        boost::asio::io_context& input_io_context
    ) : logger(Logger::getLogger(C::SERVER)),
        io_context(input_io_context),
        remove_session_task(
            input_io_context,
            boost::asio::make_strand(input_io_context),
            std::chrono::milliseconds(C::CLOSED_SESSION_REMOVAL_INTERVAL_MS)
        )
    {
    }

    ~Server()
    {
        logger->warning("Shutting down server...");
        is_shutdown.store(true);
        shutdown_session_map.clear();
    }

    void start()
    {
        logger->info2("Server Starts!");

        // 연결 끊어진 세션 제거 타이머
        remove_session_task.setTask([&]()
        {
            shutdown_session_map.clear();
            logger->severe("Dongvin, completely removed sessions.");
        });
        remove_session_task.start();
        logger->info3("Dongvin, timer for closed session removal starts!");

        tcp::acceptor acceptor(
            io_context,
            tcp::endpoint(tcp::v4(), C::STOMP_PORT)
        );

        try
        {
            while (is_shutdown.load() == false)
            {
                auto websocket_ptr = std::make_shared<tcp::socket>(io_context);
                acceptor.accept(*websocket_ptr);

                session_id_counter += 1;
                auto session_id = session_id_counter.load();

                // TODO : 세션 만들고, 세션 매니저에다가 추가하는 코드 넣어야 한다.

                // TODO : 나중에 총 세션(1개 세션 == 1개 웹소켓) 개수를 출력하는 로그를 넣자.
                logger->warning(
                    "Dongvin, new client arrives, id: " + std::to_string(session_id)
                );
            } //wh
        }
        catch (const std::exception& e)
        {
            std::ostringstream oss;
            oss << "Server stops with exception : " << e.what();
            logger->severe(oss.str());
        }
    }

    void shutdown()
    {
        if (is_shutdown.load() == false) is_shutdown = true;
    }

    void afterTerminationSession(long session_id);

private:
    std::string getSessionId();

    std::shared_ptr<Logger> logger;
    boost::asio::io_context& io_context;

    // TODO : 만들어진(==live) session 들을 어떻게 저장하고 있을 것인가?

    // shutdown 된 세션들은 별도의 맵에 모아뒀다가
    // 별도의 periodic task로 주기적으로(ex : 30 sec) 제거한다. 그래야 SIGABRT 에러 피할 수 있다.
    std::unordered_map<std::string, std::shared_ptr<Session>> shutdown_session_map;
    PeriodicTask remove_session_task;

    std::atomic<bool> is_shutdown{false};
    std::atomic<long> session_id_counter{0L};
};

#endif //SERVER_H
