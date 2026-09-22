#include <iostream>
#include <boost/asio.hpp>

int main() {
  boost::asio::io_context io_context;
  io_context.run();

  std::cout << "Let's start new C++ STOMP performance test!\n";
  return 0;
}