#include <iostream>
#include <boost/asio.hpp>

#include "../constants/C.h"
#include "../include/Logger.h"
#include "../include/PeriodicTask.h"
#include "../constants/Util.h"
#include "../src/redis/RedisService.h"
#include "../include/Server.h"


/*
구현 순서.
1. (완료) 레디스 펍/섭 리스너 만들어서 '리스닝' 해보고 출력하기
2. 1 명의 클라이언트에게 웹소켓 연결 및 STOMP 프로토콜 제공 테스트(테스트용 클라이언트들과 호환 되게끔)
        요청 검증, heart-beat 체크 및 전송, 토픽 구독/구독취소 등등
3. Stomp server 내에서 세션 관리 방법 정하기
4. 다수의 클라이언트에게 서버가 응답 전송하는 방법들 각각에 대해서 성능 테스트
    single threaded serial : 싱글 스레드로 응답 전송
    multi threaded serial : 멀티 스레드로 전송 but, 개별 스레드는 serial
    io_context base async : 모든 네트워킹 Tx/Rx 를 boost asio io_context에게 위임
 */

int main()
{
    const std::shared_ptr<Logger> logger = Logger::getLogger(C::MAIN);
    logger->warning("=================================================================");
    logger->warning("Dongvin, C++ STOMP Server STARTS. ver: " + std::string{C::VER});
    logger->warning("=================================================================");

    // make worker thread pool for main boost.asio io_context
    boost::asio::io_context main_io_context;
    auto workGuard = boost::asio::make_work_guard(main_io_context);
    std::vector<std::thread> threadVec;
    int cpuCoreCnt = static_cast<int>(std::thread::hardware_concurrency());
    logger->warning("CPU Core Cnt : " + std::to_string(cpuCoreCnt));
    for (auto i = 0; i < cpuCoreCnt; ++i)
    {
        for (auto j = 0; j < C::THREAD_CNT_PER_IO_CONTEXT; ++j)
        {
            threadVec.emplace_back(
                [&main_io_context]()
                {
                    main_io_context.run();
                }
            );
        }
    }

    logger->warning(
        "Made io_cotext.run() worker thread pool with thread cnt: "
        + std::to_string(cpuCoreCnt * C::THREAD_CNT_PER_IO_CONTEXT)
    );

    // used std::promise to synchronize the shutdown process
    std::promise<void> shutdownPromise;
    auto shutdownFuture = shutdownPromise.get_future();

    // redis conn 테스트.
    std::shared_ptr<RedisService> redis_service_ptr = std::make_shared<RedisService>();
    redis_service_ptr->init();

    // 레디스 Ping 테스트
    redis_service_ptr->verifyRedisConnection();

    // Redis Pub/Sub channel 리스닝 시작
    std::thread([redis_service_ptr]()
    {
        // startPubSubListening 함수가 blocking이기 때문에 별개 스레드에서 실행시켜야 다음 로직을 실행할 수 있다.
        redis_service_ptr->startPubSubListening();
    }).detach();

    // Redis Pub/Sub channel 에 메시지 퍼블리시 테스트
    /*std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    redis_service_ptr->publishMsg("Test Msg Pub to Redis by main!");*/

    Server server(main_io_context);
    Util::delayedExecutorAsyncByThread(
        0, [&server] { server.start(); }
    );


    // 프로그램 정상 종료 준비
    // handle exit signal using boost::asio::signal_set
    boost::asio::signal_set signals(main_io_context, SIGINT, SIGTERM);
    signals.async_wait([&](const boost::system::error_code& ec, int signal)
    {
        try
        {
            if (!ec)
            {
                std::cout << "\n\t>>> Received signal: " << signal << ". Stopping server...\n";
                workGuard.reset();

                if (!main_io_context.stopped())
                {
                    main_io_context.stop();
                }

                std::cout << "\t>>> all io_context stopped.\n";
                shutdownPromise.set_value();
            }
        }
        catch (const std::exception& e)
        {
            std::cerr << "Exception during signal handling: " << e.what() << "\n";
            shutdownPromise.set_exception(std::make_exception_ptr(e));
        }
    });

    // Wait for shutdown to complete
    shutdownFuture.wait();

    // Shutdown RedisService
    redis_service_ptr->shutdown();

    // Server 는 굳이 또 shutdown 시킬 필요가 없다. Destructor 가 알아서 처리해준다.

    // do cleaning before shutting down.
    for (auto& thread : threadVec)
    {
        if (thread.joinable())
        {
            std::cout << "Joining io_context.run() worker thread " << thread.get_id() << "...\n";
            thread.join();
        }
        else
        {
            std::cout << "Cannot join thread : " << thread.get_id() << "\n";
        }
    }

    logger->warning("=================================================================");
    logger->warning("Dongvin, C++ STOMP Server SHUTS DOWN gracefully.");
    logger->warning("=================================================================");
    return 0;
} //main
