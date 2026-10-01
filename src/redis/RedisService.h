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

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/consign.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/signal_set.hpp>

#include "../constants/C.h"
#include "../include/Logger.h"
#include "../include/PeriodicTask.h"
#include "../include/OnetimeTask.h"
#include <boost/asio.hpp>

class RedisService : public std::enable_shared_from_this<RedisService>
{
public:
    explicit RedisService(
        boost::asio::io_context& input_io_context
        // TODO : 나중에 여기에는 각종 msg_tx 용 객체들이 정의돼야 한다.
        //  server socket 을 돌리는 Server 객체는 Session 을 만들 뿐, 응답을 전송하지는 않기 때문이다.
    ) : logger(Logger::getLogger(C::REDIS_MSG_SUBSCRIBER)),
        io_context(input_io_context),
        redis_conn(io_context),
        strand(boost::asio::make_strand(io_context)),
        task_cleaner(
            input_io_context,
            boost::asio::make_strand(input_io_context),
            std::chrono::milliseconds(C::ONETIME_TASK_CLEAN_INTERVAL_MS)
        )
    {
    }

    ~RedisService()
    {
        task_cleaner.stop();
        redis_conn.cancel();
    }

    void init()
    {
        auto self = shared_from_this();
        cfg.addr.host = C::REDIS_HOST_IP;
        cfg.addr.port = C::REDIS_PORT;

        // 이 부분은 건드리지 않는게 좋다. boost::asio::detached 대신
        // 다른 Completion Token 람다를 넣어 봤지만, 무슨 이유에서인지 람다가 실행되지 않아서 그렇다.
        // 번거롭지만 초기화를 확인하는건 아예 별도의 테스트 함수(void verifyRedisConnection())로 실행하고,
        // 연결 여부를 caller 쪽에서 확인하고 대응하게 만들었다.
        redis_conn.async_run(cfg, {}, boost::asio::detached);

        task_cleaner.setTask(
            [&]()
            {
                onetime_task_vec.clear();
                logger->severe(
                    "Dongvin, completely removed onetime tasks in RedisService."
                );
            }
        );
        task_cleaner.start();
        logger->info3("Dongvin, timer for onetime task cleaner starts!");
    }

    bool get_is_ready()
    {
        return is_ready.load();
    }


    void verifyRedisConnection()
    {
        // 이걸로 RedisSession class 멤버필드에 접근 가능.
        // shared_from_this() 롤 사용하기 위해서는 RedisService.h 자체가 shared_ptr로 초기화 돼야 한다.
        auto self = shared_from_this();
        auto reqPtr = std::make_shared<boost::redis::request>();
        auto resPtr = std::make_shared<boost::redis::response<std::string>>();
        reqPtr->push("PING");

        auto onetime_task_ptr = std::make_shared<OnetimeTask>(
            io_context,
            C::REDIS_CONN_WAIT_TIMEOUT_MS,
            [self, reqPtr, resPtr]()
            {
                // exec redis cmd in async mode
                std::cout << "Redis Ping Pong Test Start! \n";
                self->redis_conn.async_exec(
                    *reqPtr,
                    *resPtr,
                    [self, resPtr, reqPtr](const boost::system::error_code& ec, std::size_t)
                    {
                        if (ec)
                        {
                            std::cerr << "Redis PING failed: " << ec.message() << "\n";
                            return;
                        }
                        self->is_ready.store(true); // ping pong test 결과 기록
                        std::cout << "PING: " << std::get<0>(*resPtr).value() << "\n";
                    }
                );
            }
        );
        onetime_task_ptr->start();
        onetime_task_vec.emplace_back(std::move(onetime_task_ptr));
        // 2026년 10월 1일 pm 09:41 현재, OnetimeTask를 정의해서 shared ptr로 만들었고,
        // 해당 포인터를 별도의 vector에 담아뒀다가 별도의 타이머로 주기적으로(30초 간격)
        // onetime task vertor를 비워주는 방식으로 task가 io_context를 통해서
        // async 하게 처리되게 만들었다.
        // 그랬더니 지긋지긋했던 SIGABRT 문제와 비일관적인 후속동작 문제가
        // Mac/Window/Linux 에서 모두 해결됐으며, 호출자의 스레드를 고의로 슬립시켰던
        // 과거의 방법도 쓸 필요 없게 됐다.
    }


    void publishMsg(const std::string& msg)
    {
        auto self = shared_from_this();
        auto reqPtr = std::make_shared<boost::redis::request>();
        reqPtr->push("PUBLISH", C::REDIS_PUB_SUB_CHANNEL, msg);
        auto resPtr = std::make_shared<boost::redis::response<std::string>>();

        auto onetime_task_ptr = std::make_shared<OnetimeTask>(
            io_context,
            C::REDIS_CONN_WAIT_TIMEOUT_MS,
            [self, reqPtr, resPtr, msg]()
            {
                // exec redis cmd in async mode
                self->redis_conn.async_exec(
                    *reqPtr,
                    *resPtr,
                    [self, resPtr, msg](const boost::system::error_code& ec, std::size_t)
                    {
                        if (ec)
                        {
                            std::cerr << "Redis PUBLISH failed: " << ec.message() << "\n";
                            return;
                        }
                        std::cout << "Msg published by this server : " << msg << "\n";
                    }
                );
            }
        );
        onetime_task_ptr->start();
        onetime_task_vec.emplace_back(std::move(onetime_task_ptr));
    }

    void startPubSubListening()
    {
        if (is_listening.load())
        {
            logger->warning("Already Listening Pub/Sub channel!");
            return;
        }
        is_listening.store(true);
        boost::asio::co_spawn(
            io_context,
            co_entry(cfg),
            boost::asio::detached
        );
    }

    void shutdown()
    {
        is_ready.store(false);
        is_listening.store(false);
        is_shutdown.store(true);
        redis_conn.cancel();
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
            if (ec)
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
    boost::asio::io_context& io_context;
    boost::redis::connection redis_conn;
    boost::redis::config cfg;

    // used strand to reduce cache miss
    boost::asio::strand<boost::asio::io_context::executor_type> strand;

    // Onetime task saving container
    std::vector<std::shared_ptr<OnetimeTask>> onetime_task_vec;
    // Onetime task cleaner
    PeriodicTask task_cleaner;

    std::atomic<bool> is_ready{false};
    std::atomic<bool> is_listening{false};
    std::atomic<bool> is_shutdown{false};

    // TODO : 나중에 여기에는 각종 msg_tx 용 객체들이 정의돼야 한다.
    //  server socket 을 돌리는 Server 객체는 Session 을 만들 뿐, 응답을 전송하지는 않기 때문이다.
};

#endif //REDISMESSAGESUBSCRIBER_H
