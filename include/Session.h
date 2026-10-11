//
// Created by 박동빈 on 2026. 9. 28..
//

#ifndef SESSION_H
#define SESSION_H

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <memory>

#include "Logger.h"
#include "StompHandler.h"

using boost::asio::ip::tcp;

class Server;
class MsgBroker;
class StompHandler;

// RedisService.h 같은 특수한 경우가 아니라면, 클래스 구현시  .h 와 .cpp 를 분리하는 것이 정석이다.
// 그렇지 않고 Session.h 에다가 모든 구현을 전부 집어넣어 버리면 순환 #include 문제가 발생했을 때
// forward declaration 만으로는 문제를 해결할 수가 없기 때문이다.
// Server 내의 member function 을 Session.h 내부에서 호출하려 하면 자꾸
// incomplete ... 라는 에러가 떠서 빌드가 실패했다.

// 이 문제를 해결하려면 .h / .cpp를 분리한 후 forward declaration 을 하던가,
// 멤버 클래스들 전부 포인터로 선언해서 Session 생성 후 일일이 set...(){...} 를 해줘야 한다.

// 후자의 포인터 취급 방법은 프로젝트 크기가 클 수록 '까먹은 set 과정'이 발생할 위험이 크고,
// 객체 생성 후 '이 객체는 올바르게 초기화 된 객체다'라는 보장을 할 수가 없게 된다.
// 상황이 너무 복잡할 때는 두 가지 방법을 전부 사용해야 할 수도 있다.

class Session : public std::enable_shared_from_this<Session>
{
public:
  explicit Session(
    long input_session_id,
    std::shared_ptr<
      boost::asio::ip::tcp::socket
    > input_web_socket_ptr,
    boost::asio::io_context& input_io_context,
    Server& input_server
  );

  ~Session();

  // Rule of five. Session object is not allowed to copy or move.
  Session(const Session&) = delete;
  Session& operator=(const Session&) = delete;
  Session& operator=(Session&&) noexcept = delete;
  Session(Session&&) noexcept = delete;

  void start();

  long getSessionId() const;

  bool isShutDown() const;

  void setShutdownTrue();

  int64_t getLatestHeartBeatTimeMillis() const;

  void sendMsgToClientViaMsgBroker(const std::string& msg);

  void sendHeartBeatToClient();

  void sendErrorFrameToClient();

  void setMsgBroker(const std::shared_ptr<MsgBroker>& msg_broker_ptr);

  void setStompHandler(const std::shared_ptr<StompHandler>& stomp_handler_ptr);

  int incrementReceiptIdAndGet();

  int getReceiptId() const;

  void subscribeTopic(const std::string& topic);

private:
  void read();

  std::shared_ptr<Logger> logger;

  std::shared_ptr<
    boost::asio::ip::tcp::socket
  > raw_tcp_socket_ptr;

  std::shared_ptr<
    boost::beast::websocket::stream<boost::beast::tcp_stream>
  > web_socket_ptr = nullptr;

  boost::beast::flat_buffer read_buffer;
  boost::asio::io_context& io_context;
  boost::asio::strand<boost::asio::io_context::executor_type> strand;
  Server& parent_server;

  std::shared_ptr<MsgBroker> msg_broker_ptr = nullptr;
  std::shared_ptr<StompHandler> stomp_handler_ptr = nullptr;

  long session_id;
  int receipt_id{0};
  std::atomic<bool> is_started{false};
  std::atomic<bool> is_shutdown{false};
  int64_t heart_beat_time_millis{0L};
};

#endif //SESSION_H