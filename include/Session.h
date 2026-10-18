//
// Created by 박동빈 on 2026. 9. 28..
//

#ifndef SESSION_H
#define SESSION_H

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <memory>

#include "../include/Logger.h"
#include "../include/StompHandler.h"
#include "../include/TopicInfo.h"

using boost::asio::ip::tcp;

class Server;
class MsgBroker;
class StompHandler;

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

  bool start();

  long getSessionId() const;

  bool isShutDown() const;

  void setShutdownTrue();

  int64_t getLatestHeartBeatTimeMillis() const;

  void pubMsgToRedisViaMsgBroker(const std::string& msg);

  void sendMsgToClientViaMsgBroker(const std::string& topic, const std::string& msg);

  void sendHeartBeatToClient();

  void sendErrorFrameToClient();

  void setMsgBroker(const std::shared_ptr<MsgBroker>& msg_broker_ptr);

  void setStompHandler(const std::shared_ptr<StompHandler>& stomp_handler_ptr);

  int incrementReceiptIdAndGet();

  int getReceiptId() const;

  void subscribeTopic(const std::string& topic, const std::string& id_str);

  void unsubscribeTopic(const std::string& id_str);

  void addTopicInfo(const std::string& topic, const std::string& id_str, int idx);

  const std::vector<TopicInfo>& getTopicInfoList();

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
  std::vector<TopicInfo> topic_info_vec{};
};

#endif //SESSION_H