//
// Created by 박동빈 on 2026. 9. 28..
//

#ifndef REDISMESSAGESUBSCRIBER_H
#define REDISMESSAGESUBSCRIBER_H

#include <iostream>
/*
 * boost/redis 관련 두 가지 include 는 프로젝트 전체에서 오직 1 번씩만 정의돼야 한다.
 * 그렇지 않을 경우(ex : boost::redis::connection 객체를 다른 곳에서 만들고 생성자에 참조 전달 하는 경우)
 * 'duplicate symbol' 관련 에러가 뜨면서 빌드에 실패한다.
 * 따라서, RedisService 같은 레디스 전용 클래스를 만들고,
 * 그 안에서만 connection을 딱 하나만 만들어서 사용하는 구조가 권장된다.
 */
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

/*
 * 별별 방법을 다 써 보며 boost::redis pub/sub 리스너를 만들고 테스트 해본 결과 내가 내린 결론은 아래와 같다.
 *
 * 1. 라이브러리 예제에서 하라는 대로 하자.
 *      C++20 코루틴을 활용한 redis pub/sub listener에댜가 multi-thread run을 돌리는 io_context
 *      를 넣었더니 mac에서는 알 수 없는 이유로 PONG timeout 에러가 뜨면서 리스너가 멈춰버렸던 것이다.
 *      원래 예제에서 하던대로 main_io_context와는 별개의 io_context를 넣어주니 정상 동작했다.
 *      그리고 boost redis 라이브러리에서는 매 연결마다 커넥션을 만들어서 쓴 다음 버리고 있었다.
 *      이것 또한 그대로 활용했다.
 *
 * 2. sync blocking으로 처리할 수 있으면 최대한 sync blocking 으로 처리하자.
 *      SIGABRT 등의 에러를 잡기 위해서 별 방법들을 다 동원해봤지만, 결국 sync blocking으로 실행하는게
 *      가장 쉽고 확실한 문제 해결 방법이었다.
 *      async 는 꼭 필요할 때만 써야하며, 남용할 경우 알 수 없는 에러들이 튀어나오며 디버깅 난이도도 높았다.
 *      대략적으로는 '1회성 실행 : sync/blocking' / '이벤트 대기 루프 : async' 의 느낌이다.
 */
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
        co_await
            conn->async_exec(req);

        // 채널(또는 채널들) 구독이 완료됐다. 채널에 푸시된 메시지들은 resp에 쌓인다.
        // 커넥션이 네트워크 에러 떠서 레디스에 다시 연결할 때, 채널들을 자동으로 다시 구독한다.
        // 그러기 위해서는 request::subscribe()를 호출해야 한다.
        while (conn->will_reconnect() && !is_shutdown.load())
        {
            // 메시지 도착을 기다린다.
            auto [ec] = co_await
                conn->async_receive2(boost::asio::as_tuple);

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
        auto ex = co_await boost::asio::this_coro::executor;
        auto conn = std::make_shared<boost::redis::connection>(ex);
        co_spawn(ex, pub_sub_listener(conn), boost::asio::detached);
        conn->async_run(
            cfg,
            boost::asio::consign(boost::asio::detached, conn)
        );

        boost::asio::signal_set sig_set(ex, SIGINT, SIGTERM);
        co_await
            sig_set.async_wait();

        conn->cancel();
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
