//
// Created by 박동빈 on 2026. 10. 6..
//

#include "../include/Session.h"
#include "../../include/Server.h"
#include "../../constants/Util.h"

#include <iostream>
#include <algorithm>


Session::Session(
  long input_session_id,
  std::shared_ptr<
    boost::asio::ip::tcp::socket
  > input_web_socket_ptr,
  boost::asio::io_context& input_io_context,
  Server& input_server
):
  logger(Logger::getLogger(C::SESSION)),
  raw_tcp_socket_ptr(std::move(input_web_socket_ptr)),
  io_context(input_io_context),
  strand(boost::asio::make_strand(input_io_context)),
  parent_server(input_server),
  session_id(input_session_id),
  heart_beat_time_millis(Util::getCurrentTimeMillis())
{
}

Session::~Session()
{
  logger->severe("Session shuts down : " + std::to_string(session_id));
  setShutdownTrue();
}

void Session::start()
{
  if (is_started.load())
  {
    // already started
    return;
  }
  is_started.store(true);

  auto self = shared_from_this();

  // Boost Beast 라이브러리 공식 문서 내 예제를 활용했다.
  // https://www.boost.org/doc/libs/master/libs/beast/doc/html/beast/using_websocket/handshaking.html
  auto select_protocol = [](boost::beast::string_view offered_tokens) -> std::string
  {
    // tokenize the Sec-Websocket-Protocol header offered by the client
    boost::beast::http::token_list offered(offered_tokens);

    // an array of protocols supported by this server
    static constexpr std::array<boost::beast::string_view, 1>
      supported = {{"v12.stomp"}};

    std::string result;

    for (auto proto : supported)
    {
      if (
        auto iter = std::ranges::find(offered, proto);
        iter != offered.end()
      )
      {
        // we found a supported protocol in the list offered by the client
        result.assign(proto.begin(), proto.end());
        break;
      }
    }
    return result;
  };

  try
  {
    boost::beast::flat_buffer buffer;
    boost::beast::http::request<boost::beast::http::string_body> req;
    boost::beast::http::read(*raw_tcp_socket_ptr, buffer, req);

    // validate incoming req
    if (!boost::beast::websocket::is_upgrade(req))
    {
      // web socket 으로 업그레이드 해달라는 요청이 아닌 경우.
      logger->severe("Not a web socket upgrade req!");
      parent_server.afterTerminationSession(session_id);
      return;
    }
    if (req.method() != boost::beast::http::verb::get)
    {
      // 최초 웹소켓 연결 요청은 http get 요청이어야 한다.
      logger->severe("Not a HTTP GET req!");
      parent_server.afterTerminationSession(session_id);
      return;
    }
    if (req.target() != C::STOMP_SERVER_END_POINT)
    {
      // 현재 서버에서 지원하는 접속 URL이 아닌 경우.
      logger->severe("STOMP server endpoint not matches!");
      parent_server.afterTerminationSession(session_id);
      return;
    }

    std::string protocol = select_protocol(
      req[boost::beast::http::field::sec_websocket_protocol]
    );

    if (protocol.empty())
    {
      // none of our supported protocols were offered
      boost::beast::http::response<boost::beast::http::string_body> res;
      res.result(boost::beast::http::status::bad_request);
      res.body() = "No valid sub-protocol was offered."
        " This server implements"
        " v12.stomp only!";
      boost::beast::http::write(*raw_tcp_socket_ptr, res);
    }
    else
    {
      // Construct the stream, transferring ownership of the socket
      web_socket_ptr = std::make_shared<
        boost::beast::websocket::stream<boost::beast::tcp_stream>
      >(std::move(*raw_tcp_socket_ptr));

      web_socket_ptr->set_option(
        boost::beast::websocket::stream_base::decorator(
          [protocol](boost::beast::http::response_header<>& hdr)
          {
            hdr.set(
              boost::beast::http::field::sec_websocket_protocol,
              protocol
            );
          }
        ) //decorator
      ); //set option

      // Accept the upgrade request and start sync reading on STOMP web-socket
      web_socket_ptr->accept(req);
      heart_beat_time_millis = Util::getCurrentTimeMillis();
      read();
    }
  } // try
  catch (const std::exception& e)
  {
    setShutdownTrue();
    logger->severe("Failed to set up STOMP connection ! e.what() : " + std::string(e.what()));
    parent_server.afterTerminationSession(session_id);
  } catch (...)
  {
    setShutdownTrue();
    logger->severe("Failed to set up STOMP connection with unknown exception!");
    parent_server.afterTerminationSession(session_id);
  }
}

long Session::getSessionId() const
{
  return session_id;
}

bool Session::isShutDown() const
{
  return is_shutdown.load();
}

void Session::setShutdownTrue()
{
  if (is_shutdown.load())
  {
    return;
  }
  const auto self = shared_from_this();
  // 제거 예정인 세션은 최대한 빨리 msg broker에서 삭제 해주는게 좋다.
  msg_broker_ptr->deleteSession(self);
  is_shutdown.store(true);
}

int64_t Session::getLatestHeartBeatTimeMillis() const
{
  return heart_beat_time_millis;
}

void Session::pubMsgToRedisViaMsgBroker(const std::string& msg)
{
  msg_broker_ptr->txMsgToRedisPubSubChannel(msg);
}

void Session::sendMsgToClientViaMsgBroker(const std::string& msg)
{
  if (!isShutDown())
  {
    auto self = shared_from_this();
    auto res_frame_ptr = std::make_shared<std::string>(msg);
    boost::asio::post(
      self->strand,
      [self, res_frame_ptr]()
      {
        self->web_socket_ptr->async_write(
          boost::asio::buffer(*res_frame_ptr),
          [self, res_frame_ptr](const boost::system::error_code& ec, std::size_t)
          {
            if (ec)
            {
              self->logger->severe(
                "Msg TX to client failed: " + ec.message());
            }
          }
        ); // async_write
      }
    );
  }
  else
  {
    logger->warning(
      "MsgBroker TX failed! Session id : "
      + std::to_string(getSessionId()) + " shutdown!"
    );
  }
}

void Session::sendHeartBeatToClient()
{
  /*
   * STOMP heart-beat 는 두 가지가 있다.
   * 1. server -> client : 이거를 안 보내면 클라이언트가 연결을 끊는다.
   * 2. server <- client : 서버가 이거를 못 받으면 클라이언트 커넥션와 관련 세션을 제거한다.
   */
  if (!isShutDown())
  {
    auto self = shared_from_this();
    auto res_frame_ptr = std::make_shared<std::string>(&C::SINGLE_BACK_SLASH_CHAR);
    boost::asio::post(
      self->strand,
      [self, res_frame_ptr]()
      {
        self->web_socket_ptr->async_write(
          boost::asio::buffer(*res_frame_ptr),
          [self, res_frame_ptr](const boost::system::error_code& ec, std::size_t)
          {
            if (ec)
            {
              self->logger->severe(
                "WebSocket heart beat write failed: " + ec.message());
            }
          }
        ); // async_write
      }
    );
  }
  else
  {
    logger->warning(
      "Heart-beat TX failed! Session id : "
      + std::to_string(getSessionId()) + " shutdown!"
    );
  }
}

void Session::sendErrorFrameToClient()
{
  if (!isShutDown())
  {
    auto self = shared_from_this();
    std::string frame =
      "ERROR\n"
      "receipt-id:" + std::to_string(session_id) + "\n"
      "message:client sent malformed frame or internal server error\n"
      "\n";
    frame.push_back(C::STOMP_FRAME_NUL_OCTET);
    auto res_frame_ptr = std::make_shared<std::string>(frame);
    boost::asio::post(
      self->strand,
      [self, res_frame_ptr]()
      {
        self->web_socket_ptr->async_write(
          boost::asio::buffer(*res_frame_ptr),
          [self, res_frame_ptr](const boost::system::error_code& ec, std::size_t)
          {
            if (ec)
            {
              self->logger->severe(
                "Sending ERROR frame failed: " + ec.message());
            }
          }
        ); // async_write
      }
    );
  }
  else
  {
    logger->warning(
      "ERROR frame TX failed! Session id : "
      + std::to_string(getSessionId()) + " shutdown!"
    );
  }
}

void Session::setMsgBroker(const std::shared_ptr<MsgBroker>& msg_broker_ptr)
{
  this->msg_broker_ptr = msg_broker_ptr;
}

void Session::setStompHandler(const std::shared_ptr<StompHandler>& stomp_handler_ptr)
{
  this->stomp_handler_ptr = stomp_handler_ptr;
}

int Session::incrementReceiptIdAndGet()
{
  this->receipt_id++;
  return receipt_id;
}

int Session::getReceiptId() const
{
  return this->receipt_id;
}

void Session::subscribeTopic(const std::string& topic, const std::string& id_str)
{
  auto self = shared_from_this();
  msg_broker_ptr->subscribe(topic, id_str, self);
}

void Session::unsubscribeTopic(const std::string& id_str)
{
  // 제거 대상 요소를 가리키는 반복자를 찾아낸다.
  const auto it = std::ranges::find_if(
    topic_info_vec,
    [&id_str](const TopicInfo& info)
    {
      return info.id_str == id_str;
    }
  );

  if (it == topic_info_vec.end()) { return; }

  // 반복자로 얻어낸 제거 대상 요소를 std::erase_if()를 써서 실제로 삭제한다.
  const TopicInfo& topic_info = *it;
  std::erase_if(
    topic_info_vec,
    [&id_str](const TopicInfo& info)
    {
      return info.id_str == id_str;
    }
  );

  // MsgBroker에서도 삭제한다.
  auto self = shared_from_this();
  msg_broker_ptr->unsubscribe(self, topic_info);
}

void Session::addTopicInfo(const std::string& topic, const std::string& id_str, const int idx)
{
  TopicInfo info(topic, id_str, idx, this->getSessionId());
  this->topic_info_vec.emplace_back(info);
}

const std::vector<TopicInfo>& Session::getTopicInfoList()
{
  return this->topic_info_vec;
}

void Session::read()
{
  if (is_shutdown.load())
  {
    logger->warning("Session was closed! Stopped socket reading");
    return;
  }
  auto self = shared_from_this();
  web_socket_ptr->async_read(
    read_buffer,
    [self](const boost::system::error_code& ec, std::size_t)
    {
      if (ec)
      {
        self->logger->severe("WebSocket read failed!");
        self->logger->severe("error message: " + ec.message());

        // 여기서 오류 나면 더 이상 STOMP 세션을 지속해서는 안 된다.
        // ERROR 프레임을 전송한 후, 현재의 세션을 '삭제 예정 세션' 으로 이동시킨다.
        self->sendErrorFrameToClient();
        self->setShutdownTrue();
        self->parent_server.afterTerminationSession(self->getSessionId());
        return;
      }

      const auto req = boost::beast::buffers_to_string(self->read_buffer.data());
      std::shared_ptr<bool> is_disconnected_ptr = std::make_shared<bool>(false);
      std::string res_frame = self->stomp_handler_ptr->handleStompReq(
        req, is_disconnected_ptr
      );

      if (res_frame == C::HEART_BEAT_RESULT)
      {
        self->heart_beat_time_millis = Util::getCurrentTimeMillis();
        res_frame = C::HEART_BEAT_STR;
      }
      // 하트비트 아닌 응답들만 출력한다.
      if (res_frame != C::HEART_BEAT_STR && res_frame != C::HEART_BEAT_RESULT)
      {
        self->logger->info2(">>> res to client :");
        self->logger->info2("\n" + res_frame);
      }

      auto res_frame_ptr = std::make_shared<std::string>(res_frame);
      self->web_socket_ptr->async_write(
        boost::asio::buffer(*res_frame_ptr),
        [self, res_frame_ptr, is_disconnected_ptr]
      (const boost::system::error_code& ec_internal, std::size_t)
        {
          if (ec_internal)
          {
            self->logger->severe(
              "WebSocket write failed: " + ec_internal.message()
            );
            return;
          }
          // 만약 DISCONNECT 요청이었다면, 현재 세션은 '삭제 예정 세션 맵'으로 이동해야 한다.
          if (*is_disconnected_ptr == true)
          {
            self->setShutdownTrue();
            self->parent_server.afterTerminationSession(self->getSessionId());
            return;
          }
        }
      ); // async_write
      // 어쨌건 버퍼는 다음 요청을 위해서 비운다.
      self->read_buffer.consume(self->read_buffer.size());
      boost::asio::post(self->strand, [self]() { self->read(); });
    }
  );
}