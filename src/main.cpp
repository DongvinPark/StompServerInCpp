#include <iostream>
#include <boost/asio.hpp>

#include "../constants/C.h"
#include "../include/Logger.h"
#include "../include/PeriodicTask.h"
#include "../constants/Util.h"

int main() {
  const std::shared_ptr<Logger> logger = Logger::getLogger(C::MAIN);
  logger->warning("=================================================================");
  logger->warning("Dongvin, C++ STOMP Server STARTS. ver: " + std::string{C::VER});
  logger->warning("=================================================================");

    // make worker thread pool for main boost.asio io_context
    boost::asio::io_context io_context;
    auto workGuard = boost::asio::make_work_guard(io_context);
    std::vector<std::thread> threadVec;
    int cpuCoreCnt = static_cast<int>(std::thread::hardware_concurrency());
    for (auto i = 0; i < cpuCoreCnt; ++i) {
        threadVec.emplace_back(
            [&io_context]() {
                Util::set_thread_priority();
                io_context.run();
            }
        );
    }

    // make threads pool for worker io_context pool
    std::vector<std::shared_ptr<boost::asio::io_context>> ioContextPool;
    std::vector<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> workGuardVec;
    for (int i = 0; i < cpuCoreCnt; ++i) {
        auto workerIoContextPtr = std::make_shared<boost::asio::io_context>();
        ioContextPool.emplace_back(workerIoContextPtr);

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
  auto test_strand = boost::asio::make_strand(io_context);
  PeriodicTask periodic_task(*ioContextPool[0], test_strand, std::chrono::milliseconds(1000));
  periodic_task.setTask(
    [](){
      std::cout << "Run PeriodicTask!\n";
    });
  periodic_task.start();

  std::this_thread::sleep_for(std::chrono::milliseconds(5000));
  periodic_task.stop();



  // 프로그램 정상 종료 준비

  // handle exit signal using boost::asio::signal_set
  boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
  signals.async_wait([&](const boost::system::error_code& ec, int signal) {
      try {
          if (!ec) {
              std::cout << "\n\t>>> Received signal: " << signal << ". Stopping server...\n";
              workGuard.reset();

              for (int i = 0; i < cpuCoreCnt; ++i){
                  workGuardVec[i].reset();
              }

              if (!io_context.stopped()) {
                  io_context.stop();
              }

              for (int i = 0; i < cpuCoreCnt; ++i){
                  if (!ioContextPool[i]->stopped()){
                      ioContextPool[i]->stop();
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
