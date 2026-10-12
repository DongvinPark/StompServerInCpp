//
// Created by 박동빈 on 2026. 9. 28..
//

#ifndef REDISMESSAGESUBSCRIBER_H
#define REDISMESSAGESUBSCRIBER_H

#include <iostream>

#include <boost/redis.hpp>
#include <boost/redis/src.hpp>

#include <boost/redis/connection.hpp>
#include <boost/redis/push_parser.hpp>

#include "../include/sync_connection.h"

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/consign.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/signal_set.hpp>

#include "../constants/C.h"
#include "../include/Logger.h"
#include <boost/asio.hpp>


class RedisService
{
public:
    explicit RedisService(
        // TODO : 나중에 여기에는 각종 msg_tx 용 객체들이 정의돼야 한다.
        //  server socket 을 돌리는 Server 객체는 Session 을 만들 뿐, 응답을 전송하지는 않기 때문이다.
    ) : logger(Logger::getLogger(C::REDIS_MSG_SUBSCRIBER))
    {
    }

    ~RedisService()
    {
    }

    void init()
    {
        cfg.addr.host = C::REDIS_HOST_IP;
        cfg.addr.port = C::REDIS_PORT;
        is_ready.store(true);
    }

    bool get_is_ready()
    {
        return is_ready.load();
    }


    void verifyRedisConnection()
    {
        // 그냥 sync 로 처리한다. 어차피 1 번만 정확하게 하면 되기 때문이다.
        try
        {
            boost::redis::sync_connection conn;
            conn.run(cfg);

            boost::redis::request req;
            req.push("PING");

            boost::redis::response<std::string> resp;

            conn.exec(req, resp);
            conn.stop();

            std::cout << "Response: " << std::get<0>(resp).value() << std::endl;
            is_ready.store(true);
        } catch (std::exception& e)
        {
            logger->severe("Redis Ping failed! : " + std::string(e.what()));
            is_ready.store(false);
        } catch (...)
        {
            logger->severe("Redis Ping failed via unknown exception!");
            is_ready.store(false);
        }
    }


    void publishMsg(const std::string& msg)
    {
        // 여기도 마친가지로 그냥 sync 로만 처리한다.
        try
        {
            boost::redis::sync_connection conn;
            conn.run(cfg);

            boost::redis::request req;
            std::string cmd = "PUBLISH";
            req.push(cmd, C::REDIS_PUB_SUB_CHANNEL, msg);

            boost::redis::response<std::string> resp;

            conn.exec(req, resp);
            conn.stop();

            std::cout << "Push Result: " << std::get<0>(resp).value() << std::endl;
            is_ready.store(true);
        } catch (std::exception& e)
        {
            logger->severe("Redis Push failed! : " + std::string(e.what()));
            is_ready.store(false);
        } catch (...)
        {
            logger->severe("Redis Push failed via unknown exception!\n");
            is_ready.store(false);
        }
    }

    void startPubSubListening()
    {
        if (is_listening.load())
        {
            logger->warning("Already Listening Pub/Sub channel!");
            return;
        }
        boost::asio::io_context io_context_internal;
        is_listening.store(true);
        // co_spawn() 부분에 마우스를 올려보면 라이브러리 문서가 뜰텐데, 이 문서를 꼭 읽어보길 바란다.
        boost::asio::co_spawn(
            io_context_internal,
            co_entry(cfg),
            boost::asio::detached
        );
        io_context_internal.run(); // 이 부분이 blocking 이다. 프로그램을 종료해야 아래의 로그가 줄력된다.
        // std::cout << "startPubSubListening() 종료 !!!\n";
    }

    void shutdown()
    {
        is_ready.store(false);
        is_listening.store(false);
        is_shutdown.store(true);
    }

private:
    // 레디스에 푸시된 메시지를 리스닝한다.
    auto pub_sub_listener(
        std::shared_ptr<boost::redis::connection> conn) -> boost::asio::awaitable<void>
    {
        boost::redis::generic_flat_response resp;
        conn->set_receive_response(resp);

        // 채널을 구독한다. 여러개 구독할 수도 있다.
        boost::redis::request req;
        req.subscribe({C::REDIS_PUB_SUB_CHANNEL});
        co_await conn->async_exec(req);

        // 채널(또는 채널들) 구독이 완료됐다. 채널에 푸시된 메시지들은 resp에 쌓인다.
        // 커넥션이 네트워크 에러 떠서 레디스에 다시 연결할 때, 채널들을 자동으로 다시 구독한다.
        // 그러기 위해서는 request::subscribe()를 호출해야 한다.
        while (conn->will_reconnect() && !is_shutdown.load())
        {
            // 메시지 도착을 기다린다.
            auto [ec] = co_await conn->async_receive2(boost::asio::as_tuple);

            // 에러 체크.
            if (ec)
            {
                std::cerr << "Error during receive: " << ec << "\n";
                break;
            }

            // 권한 부족 등의 이유로 아래의 에러체크문이 실행될 수도 있다.
            if (resp.has_error())
            {
                std::cerr << "The receive response contains an error: "
                    << resp.error().diagnostic << "\n";
                break;
            }

            // 받은 응답은 코루틴을 suspend 하지 않으면서 즉각 소비돼야 한다. 즉, async operation 으로 소비하면 안 된다.
            for (boost::redis::push_view elem : boost::redis::push_parser(resp.value()))
            {
                std::cout << "Pub/Sub channel Listening success! : " << elem.channel
                    << ": " << elem.payload << "\n";
            }

            resp.value().clear();
        } //wh
    } //end of


    auto co_entry(boost::redis::config cfg) -> boost::asio::awaitable<void>
    {
        auto executor = co_await boost::asio::this_coro::executor;
        auto conn_ptr = std::make_shared<boost::redis::connection>(executor);

        // co_spawn 위에 마우스를 올려보면 boost::redis 라이브러리 문서가 보일텐데, 해당 문서를 읽어보길 바란다.
        // 간단하게는 전달 받은 awaitable_executor
        //  (여기서는 pub_sub_listener()함수의 결과물인 boost::asio::awaitable<void>) 를
        // 별도의 비동기 실행 흐름으로 분리하라는 것으로 이해하면 된다.
        co_spawn(executor, pub_sub_listener(conn_ptr), boost::asio::detached);

        conn_ptr->async_run(
            cfg,
            boost::asio::consign(boost::asio::detached, conn_ptr)
        );

        boost::asio::signal_set sig_set(executor, SIGINT, SIGTERM);

        // co_await '...'; 는 이런 뜻이다.
        // '...'; 이 부분의 작업이 끝나면 그때 나를 다시 resume 해서 실행하면 된다는 것.
        // 즉, 바로 아래의 코드는 결국 프로그램 종료를 기다리는 놈이다.
        co_await sig_set.async_wait();

        conn_ptr->cancel();
    }

    std::shared_ptr<Logger> logger;
    boost::redis::config cfg;

    std::atomic<bool> is_ready{false};
    std::atomic<bool> is_listening{false};
    std::atomic<bool> is_shutdown{false};

    // TODO : 나중에 여기에는 각종 msg_tx 용 객체들이 정의돼야 한다.
    //  server socket 을 돌리는 Server 객체는 Session 을 만들 뿐, 응답을 전송하지는 않기 때문이다.
};

#endif //REDISMESSAGESUBSCRIBER_H
