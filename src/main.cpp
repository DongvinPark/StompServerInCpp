#include <iostream>
#include <boost/asio.hpp>

#include "../constants/C.h"
#include "../include/Logger.h"

int main() {
  boost::asio::io_context io_context;
  io_context.run();

  const std::shared_ptr<Logger> logger = Logger::getLogger(C::MAIN);
  logger->severe("Let's start new C++ STOMP performance test!");

  return 0;
}
