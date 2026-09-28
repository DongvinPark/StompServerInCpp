#include <iostream>
#include <boost/asio.hpp>
#include <boost/redis.hpp>
#include <boost/redis/src.hpp>

#include "../constants/C.h"
#include "../include/Logger.h"
#include "../include/PeriodicTask.h"
#include "../constants/Util.h"


/*
구현 순서.

1. 레디스 펍/섭 리스너 만들어서 '리스닝' 해보고 출력하기
2. 1 명의 클라이언트에게 웹소켓 연결 및 STOMP 프로토콜 제공 테스트(테스트용 클라이언트들과 호환 되게끔)
3. Stomp server 내에서 세션 관리 방법 정하기
3. 다수의 클라이언트에게 서버가 응답 전송하는 방법들 테스트
    single threaded serial : 싱글 스레드로 응답 전송
    multi threaded serial : 멀티 스레드로 전송 but, 개별 스레드는 serial
    io_context base async : 모든 네트워킹 Tx/Rx 를 boost asio io_context에게 위임
 */

int main() {

    const std::shared_ptr<Logger> logger = Logger::getLogger(C::MAIN);
    logger->warning("=================================================================");
    logger->warning("Dongvin, C++ STOMP Server STARTS. ver: " + std::string{C::VER});
    logger->warning("=================================================================");

    // make worker thread pool for main boost.asio io_context
    boost::asio::io_context main_io_context;
    auto workGuard = boost::asio::make_work_guard(main_io_context);
    std::vector<std::thread> threadVec;
    int cpuCoreCnt = static_cast<int>(std::thread::hardware_concurrency());
    for (auto i = 0; i < cpuCoreCnt; ++i) {
        threadVec.emplace_back(
            [&main_io_context]() {
                Util::set_thread_priority();
                main_io_context.run();
            }
        );
    }

    // make threads pool for worker io_context pool
    std::vector<std::shared_ptr<boost::asio::io_context>> workerIoContextPool;
    std::vector<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> workGuardVec;
    for (int i = 0; i < cpuCoreCnt; ++i) {
        auto workerIoContextPtr = std::make_shared<boost::asio::io_context>();
        workerIoContextPool.emplace_back(workerIoContextPtr);

        // create a work guard to prevent io_context from stopping
        workGuardVec.emplace_back(boost::asio::make_work_guard(*workerIoContextPtr));

        // create worker threads for this io_context
        for (int j = 0; j < C::THREAD_CNT_PER_WORKER_IO_CONTEXT; ++j) {
            threadVec.emplace_back([workerIoContextPtr]() {
                Util::set_thread_priority();
                workerIoContextPtr->run();
            });
        }
    }

    logger->warning(
        "Made io_cotext.run() worker thread pool with thread cnt: "
        + std::to_string(cpuCoreCnt + (cpuCoreCnt*C::THREAD_CNT_PER_WORKER_IO_CONTEXT))
    );

    // used std::promise to synchronize the shutdown process
    std::promise<void> shutdownPromise;
    auto shutdownFuture = shutdownPromise.get_future();

    // PeriodicTask 실행 테스트.
    auto test_strand = boost::asio::make_strand(main_io_context);
    PeriodicTask periodic_task(*workerIoContextPool[0], test_strand, std::chrono::milliseconds(1000));
    periodic_task.setTask(
      [](){
        std::cout << "Run PeriodicTask!\n";
      });
    periodic_task.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(5000));
    periodic_task.stop();

    // redis conn 테스트.
    boost::redis::connection conn(main_io_context);
    boost::redis::config cfg;
    cfg.addr.host = "127.0.0.1";
    cfg.addr.port = "6379";

    // Start Redis connection(in detach mode).
    conn.async_run( cfg, boost::asio::detached);

    // Create Redis req/res
    boost::redis::request redisReq;
    redisReq.push("PING");
    boost::redis::response<std::string> redisRes;

    // exec redis cmd in sync mode
    conn.async_exec(
        redisReq,
        redisRes,
        [&redisRes](const boost::system::error_code& ec, std::size_t) {
            if (ec) {
                std::cerr << "Redis PING failed: " << ec.message() << "\n";
                return;
            }
            std::cout << "PING: " << std::get<0>(redisRes).value() << "\n";
        }
    );


    // 프로그램 정상 종료 준비
    // handle exit signal using boost::asio::signal_set
    boost::asio::signal_set signals(main_io_context, SIGINT, SIGTERM);
    signals.async_wait([&](const boost::system::error_code& ec, int signal) {
      try {
          if (!ec) {
              std::cout << "\n\t>>> Received signal: " << signal << ". Stopping server...\n";
              workGuard.reset();

              for (int i = 0; i < cpuCoreCnt; ++i){
                  workGuardVec[i].reset();
              }

              if (!main_io_context.stopped()) {
                  main_io_context.stop();
              }

              for (int i = 0; i < cpuCoreCnt; ++i){
                  if (!workerIoContextPool[i]->stopped()){
                      workerIoContextPool[i]->stop();
                  }
              }

              std::cout << "\t>>> all io_context stopped.\n";
              shutdownPromise.set_value();
          }
      } catch (const std::exception& e) {
          std::cerr << "Exception during signal handling: " << e.what() << "\n";
          shutdownPromise.set_exception(std::make_exception_ptr(e));
      }
    });

    // Wait for shutdown to complete
    shutdownFuture.wait();

    // do cleaning before shutting down.
    for (auto& thread : threadVec) {
        if (thread.joinable()) {
            std::cout << "Joining io_context.run() worker thread " << thread.get_id() << "...\n";
            thread.join();
        } else {
            std::cout << "Cannot join thread : " << thread.get_id() << "\n";
        }
    }

    logger->warning("=================================================================");
    logger->warning("Dongvin, C++ STOMP Server SHUTS DOWN gracefully.");
    logger->warning("=================================================================");
    return 0;
}
