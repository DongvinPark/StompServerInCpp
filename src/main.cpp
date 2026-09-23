#include <iostream>
#include <boost/asio.hpp>

#include "../constants/C.h"
#include "../include/Logger.h"
#include "../include/PeriodicTask.h"

int main() {
  // 메인 io_context 와 work guard 정의
  boost::asio::io_context io_context;
  auto workGuard = boost::asio::make_work_guard(io_context);

  // 메인 io_context 는 CPU 코어 개수 만큼의 스레드에서 .run 병렬 처리.
  std::vector<std::thread> threadVec;
  int cpuCoreCnt = static_cast<int>(std::thread::hardware_concurrency());
  for(int i = 0; i < cpuCoreCnt; ++i){
    threadVec.emplace_back( [&io_context](){io_context.run();} );
  }

  // logger 테스트
  const std::shared_ptr<Logger> logger = Logger::getLogger(C::MAIN);
  logger->severe("Let's start new C++ STOMP performance test!");

  // PeriodicTask 실행 테스트.
  auto test_strand = boost::asio::make_strand(io_context);
  PeriodicTask periodic_task(io_context, test_strand, std::chrono::milliseconds(1000));
  periodic_task.setTask(
    [](){
      std::cout << "Run PeriodicTask!\n";
    });
  periodic_task.start();

  std::this_thread::sleep_for(std::chrono::milliseconds(5000));
  periodic_task.stop();


  // 프로그램 정상 종료 준비
  for (auto& thread : threadVec)
  {
    workGuard.reset();
    thread.join();
  }

  return 0;
}
