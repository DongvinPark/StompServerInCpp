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

// 아래는 코루틴 복습용 주석이다.
/*
 * ============================================================================
 * C++20 Coroutine 학습 정리
 * ============================================================================
 * [1. 코루틴이란?]
 * 코루틴은
 *   "실행 중인 함수의 상태를 저장한 채 실행을 잠시 중단(suspend)하고,
 *    나중에 그 상태에서 다시 실행(resume)할 수 있는 실행 단위"
 * 라고 우선 이해하면 된다.
 *
 * 일반적인 함수는 아래의 flow로 실행된다 :
 *   함수 실행 -> return -> 함수 종료
 *
 * 하지만 코루틴의 flow는 다음과 같다 :
 *   실행 -> suspend -> 상태 저장 -> resume -> 다시 실행 -> 다시 suspend ...
 *   필요하면 co_return으로 최종 종료
 *
 * ============================================================================
 * [2. 처음에는 "코루틴 실행 엔진"을 가정하자]
 * 코루틴 내부 구현을 처음부터 모두 이해하려고 하면 너무 복잡하다.
 * 따라서 처음에는 다음과 같은 "코루틴 실행 엔진"이 있다고 가정한다.
 *                  Coroutine Engine
 *                        │
 *              ┌─────────┴─────────┐
 *              │                   │
 *         Coroutine A         Coroutine B
 *              │                   │
 *            실행                 실행
 *              ↓                   ↓
 *           suspend             suspend
 *              ↓                   ↓
 *           resume              resume
 *              ↓                   ↓
 *            실행                 실행
 *
 * 이 엔진이 내부적으로 어떻게 구현되어 있는지는 일단 신경 쓰지 않는다.
 * 실제 C++에서는:
 *   - coroutine frame
 *   - promise_type
 *   - coroutine_handle
 *   - suspend_always / suspend_never
 *   - awaiter / awaitable
 *   - scheduler
 * 등의 개념이 등장하지만,
 * "코루틴이 어떻게 동작하는가?"를 처음 이해하는 단계에서는
 * 이것들을 모두 알아야 할 필요는 없다. 이것들은 나중에 코루틴의 내부 동작이 궁금해졌을 때 공부한다.
 *
 * ============================================================================
 * [3. 가장 먼저 알아야 할 키워드]
 * C++20 coroutine에서 우선 다음 키워드의 의미를 익힌다.
 *   co_await
 *   └─ 현재 코루틴의 실행을 잠시 중단하고,
 *      기다리던 작업이 완료되면 다시 resume한다.
 *
 *   co_return
 *   └─ 코루틴의 실행을 종료한다.
 *
 *   co_yield
 *   └─ 값을 하나 외부로 전달하고 코루틴을 잠시 중단한다.
 *      다음 resume 때 그 다음 지점부터 실행할 수 있다.
 *
 * 그리고 Boost.Asio에서는:
 *   co_spawn()
 *   └─ 코루틴을 Asio의 실행 시스템에 등록하여 실행시키는 역할을 한다.
 *
 * ============================================================================
 * [4. co_await를 이해하는 것이 가장 중요하다]
 * 예:
 *   boost::asio::awaitable<void> foo()
 *   {
 *       std::cout << "A\n";
 *       auto [ec] = co_await socket.async_read_some(...);
 *       std::cout << "B\n";
 *   }
 *
 * 처음에는 다음과 같이 생각한다.
 *
 *   foo()
 *     │
 *     ▼
 *   "A" 출력
 *     │
 *     ▼
 *   co_await
 *     │
 *     ├─ 현재 coroutine의 상태를 저장
 *     ├─ coroutine suspend
 *     │
 *     │   다른 작업 수행 가능
 *     │
 *     ▼
 *   I/O 완료
 *     │
 *     ▼
 *   coroutine resume
 *     │
 *     ▼
 *   "B" 출력
 *     │
 *     ▼
 *   co_return
 *
 * 따라서 co_await는 단순히
 *   "기다린다"
 * 라고만 생각하면 부족하다.
 *
 * 더 정확하게는:
 *   "현재 coroutine의 실행을 잠시 중단하고,
 *    기다리던 작업이 완료되면 다시 이어서 실행한다."
 *
 * ============================================================================
 * [5. Coroutine과 Thread는 서로 다른 개념이다]
 * 이것은 반드시 구분해야 한다.
 * 일반적인 blocking:
 *   Thread
 *     │
 *     ├── read()
 *     │
 *     │   ─────────────── 대기 ───────────────
 *     │
 *     ▼
 *   다음 코드 실행
 * 이 경우 thread 자체가 blocking된다.
 *
 *
 * 반면 coroutine + asynchronous operation:
 *   Thread
 *     │
 *     ├── Coroutine 실행
 *     │
 *     ├── co_await
 *     │
 *     └── Coroutine suspend
 *             │
 *             │
 *             │  Thread는 다른 작업 수행 가능
 *             │
 *             ▼
 *         I/O 완료
 *             │
 *             ▼
 *         Coroutine resume
 *             │
 *             ▼
 *         다음 코드 실행
 * 즉,
 *   coroutine suspend != thread sleep
 *   coroutine suspend != thread blocking
 *
 * Coroutine은 함수의 실행 상태를 잠시 멈추는 것이고,
 * OS thread가 반드시 멈추는 것은 아니다.
 *
 * ============================================================================
 * [6. Boost.Asio에서 coroutine은 어떻게 실행되는가?]
 * Boost.Asio에서는 다음과 같은 구조로 생각하면 편하다.
 *
 *
 *             C++ Coroutine
 *                   │
 *                   │ co_spawn()
 *                   ▼
 *             Asio 실행 시스템
 *                   │
 *                   ▼
 *              io_context
 *                   │
 *          ┌────────┼────────┐
 *          │        │        │
 *        thread   thread   thread
 *          │        │        │
 *          └────────┼────────┘
 *                   │
 *                   ▼
 *              coroutine 실행
 * 중요한 점:
 *   co_spawn()
 * 은 일반적으로
 *   "새로운 OS thread를 생성한다"
 * 는 의미가 아니다.
 * 이미 존재하는 io_context와 executor/scheduler를 통해
 * coroutine의 실행이 이루어진다고 이해하는 것이 좋다.
 *
 * ============================================================================
 * [7. Coroutine을 이해하는 가장 중요한 반복 패턴]
 *             ┌──────────────┐
 *             │              │
 *             ▼              │
 *          coroutine         │
 *            실행             │
 *             │              │
 *             ▼              │
 *          co_await          │
 *             │              │
 *             ▼              │
 *          suspend           │
 *             │              │
 *             │ 기다리던       │
 *             │ 작업 완료      │
 *             ▼              │
 *           resume ──────────┘
 * 이 "실행 → suspend → resume → 실행"이라는 반복 구조를
 * 먼저 이해하는 것이 중요하다.
 *
 * ============================================================================
 * [8. Boost.Redis에서의 실제 예]
 * 예:
 *   auto [ec] =
 *       co_await conn->async_receive2(boost::asio::as_tuple);
 * 이것을 처음에는 다음과 같이 읽으면 된다.
 *
 *   Redis 메시지를 기다린다.
 *           │
 *           ▼
 *   현재 coroutine suspend
 *           │ 다른 coroutine / I/O 작업 가능
 *           ▼
 *   Redis 메시지 도착
 *           │
 *           ▼
 *   coroutine resume
 *           │
 *           ▼
 *   다음 코드 실행
 *
 * 따라서:
 *   "Redis 메시지가 올 때까지 thread가 멈춘다."
 * 가 아니라,
 *   "Redis 메시지가 올 때까지 현재 coroutine의 실행을 중단하고,
 *    메시지가 도착하면 해당 coroutine을 다시 실행한다."
 * 라고 이해해야 한다.
 *
 * ============================================================================
 * [9. Coroutine의 내부 구현은 나중에 공부한다]
 * 처음부터 다음 개념들을 전부 이해하려고 하지 않는다.
 *   coroutine frame
 *   promise_type
 *   coroutine_handle
 *   awaiter
 *   awaitable
 *   await_transform
 *   completion token
 *   executor
 *   scheduler
 * 처음에는:
 *   "이런 것들이 coroutine을 실제로 동작시키는 내부 장치다."
 * 정도만 알고 넘어간다.
 * 먼저 coroutine의 사용 방법과 실행 흐름을 이해한 뒤,
 * 필요할 때 하나씩 내부 구현으로 내려간다.
 *
 * ============================================================================
 * [10. Coroutine 학습 순서]
 * 추천 순서:
 *   1.
 *   Coroutine은 suspend/resume 가능한 실행 단위라는 것을 이해한다.
 *        ↓
 *   2.
 *   co_await / co_yield / co_return의 의미를 익힌다.
 *        ↓
 *   3.
 *   "실행 → suspend → resume → 실행" 패턴을 이해한다.
 *        ↓
 *   4.
 *   co_spawn이 coroutine을 실행 시스템에 등록하는 역할임을 이해한다.
 *        ↓
 *   5.
 *   Coroutine과 Thread가 서로 다른 개념이라는 것을 확실하게 이해한다.
 *        ↓
 *   6.
 *   coroutine frame / promise / coroutine_handle 등을 공부한다.
 *        ↓
 *   7.
 *   Boost.Asio가 coroutine과 io_context/scheduler를
 *   어떻게 연결하는지 공부한다.
 *
 * ============================================================================
 * [11. 최종적으로 기억할 한 문장]
 * Coroutine은
 *   "실행 상태를 저장한 채 잠시 멈췄다가,
 *    나중에 그 지점에서 다시 실행할 수 있는 실행 단위"
 * 이다.
 * 그리고 co_await는 그 coroutine을 suspend시키는 대표적인 지점이다.
 *
 * Coroutine 자체와 Thread는 다른 개념이며,
 * Boost.Asio에서는 io_context/executor/scheduler가
 * coroutine을 실제 실행 가능한 형태로 관리한다.
 *
 * 처음에는 이 실행 엔진의 모든 내부 구현을 이해하려 하지 말고,
 *   "실행 → co_await → suspend → resume → 실행"
 * 이 흐름을 머릿속에 확실히 만드는 것이 우선이다.
 * ============================================================================
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
