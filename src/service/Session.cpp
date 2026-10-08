//
// Created by 박동빈 on 2026. 10. 6..
//

#include "../include/Session.h"
#include "../../include/Server.h"

#include <iostream>


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
  session_id(input_session_id)
{
}

Session::~Session()
{
  logger->severe("Session shuts down : " + std::to_string(session_id));
  is_shutdown.store(true);
  if (web_socket_ptr != nullptr && web_socket_ptr->is_open())
  {
    web_socket_ptr->close(C::UNSET);
  }
  if (raw_tcp_socket_ptr != nullptr && raw_tcp_socket_ptr->is_open())
  {
    raw_tcp_socket_ptr->close();
  }
  if (msg_broker_ptr != nullptr)
  {
    msg_broker_ptr = nullptr;
  }
  if (stomp_handler_ptr != nullptr)
  {
    stomp_handler_ptr = nullptr;
  }
}

void Session::start()
{
  auto self = shared_from_this();

  // Boost Beast 라이브러리 공식 문서 내 예제를 활용했다.
  // https://www.boost.org/doc/libs/master/libs/beast/doc/html/beast/using_websocket/handshaking.html
  auto select_protocol = [](boost::beast::string_view offered_tokens) -> std::string
  {
    // tokenize the Sec-Websocket-Protocol header offered by the client
    boost::beast::http::token_list offered(offered_tokens);

    // an array of protocols supported by this server
    // in descending order of preference
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

    if (!boost::beast::websocket::is_upgrade(req))
    {
      // web socket 으로 업그레이드 해달라는 요청이 아님
      logger->severe("Not a web socket upgrade req!");
      parent_server.afterTerminationSession(session_id);
      return;
    }
    if (req.method() != boost::beast::http::verb::get)
    {
      logger->severe("Not a HTTP GET req!");
      parent_server.afterTerminationSession(session_id);
      return;
    }
    if (req.target() != C::STOMP_SERVER_END_POINT)
    {
      logger->severe("STOMP server endpoint not matches!");
      parent_server.afterTerminationSession(session_id);
      return;
    }

    std::string protocol =
      select_protocol(req[boost::beast::http::field::sec_websocket_protocol]);

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
      read();
    }
  } // try
  catch (const std::exception& e)
  {
    logger->severe("Failed to set up STOMP connection ! e.what() : " + std::string(e.what()));
    parent_server.afterTerminationSession(session_id);
  } catch (...)
  {
    logger->severe("Failed to set up STOMP connection with unknown exception!");
    parent_server.afterTerminationSession(session_id);
  }
}

long Session::getSessionId()
{
  return session_id;
}

bool Session::isShutDown()
{
  return is_shutdown.load();
}

void Session::setMsgBroker(const std::shared_ptr<MsgBroker>& msg_broker_ptr)
{
  this->msg_broker_ptr = msg_broker_ptr;
}

void Session::setStompHandler(const std::shared_ptr<StompHandler>& stomp_handler_ptr)
{
  this->stomp_handler_ptr = stomp_handler_ptr;
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
        //self->logger->severe("error category: " + std::string(ec.category().name()));
        //self->logger->severe("error value: " + std::to_string(ec.value()));
        self->logger->severe("error message: " + ec.message());

        // 여기서 오류 나면 더 이상 socker read를 지속해서는 안 된다.
        // 현재의 세션을 '삭제 예정 세션' 으로 이동시킨다.
        self->is_shutdown.store(true);
        self->parent_server.afterTerminationSession(self->getSessionId());
        return;
      }

      const auto req =
        boost::beast::buffers_to_string(
          self->read_buffer.data()
        );

      bool is_disconnected = false;
      std::string res_frame = self->stomp_handler_ptr->handleStompReq(req, is_disconnected);
      if (res_frame != C::EMPTY_STR)
      {
        self->logger->info2(">>> res for client :");
        self->logger->info2(res_frame);

        auto connected_frame = std::make_shared<std::string>(res_frame);
        self->web_socket_ptr->async_write(
          boost::asio::buffer(*connected_frame),
          [self, connected_frame, &is_disconnected](
          const boost::system::error_code& ec, std::size_t)
          {
            if (ec)
            {
              self->logger->severe(
                "WebSocket write failed: " + ec.message()
              );
              return;
            }
            // 만약 DISCONNECT 요청이었다면, 현재 세션은 '삭제 예정 세션 맵'으로 이동해야 한다.
            if (is_disconnected)
            {
              self->is_shutdown.store(true);
              if (self->web_socket_ptr != nullptr && self->web_socket_ptr->is_open())
              {
                self->web_socket_ptr->close(C::UNSET);
              }
              if (self->raw_tcp_socket_ptr != nullptr && self->raw_tcp_socket_ptr->is_open())
              {
                self->raw_tcp_socket_ptr->close();
              }

              // TODO : implement later - 현재 세션이 구독했던 모든 토픽들에서 unscribe 해야 한다.
              self->parent_server.afterTerminationSession(self->getSessionId());
              return;
            }
          }
        ); // async_write
      } //if (res_frame != C::EMPTY_STR)
      // 어쨌건 버퍼는 다음 요청을 위해서 비운다.
      self->read_buffer.consume(self->read_buffer.size());
      boost::asio::post(self->strand, [self]() { self->read(); });
    }
  );
}

/*
 * ============================================================================
 * C++ STOMP Server - WebSocket / STOMP handshake 복습 노트
 * ============================================================================
 *
 * ============================================================================
 * STOMP 커넥션 생성 과정 요약
 * ============================================================================
 *   TCP
 *    │
 *    ▼
 *   HTTP Upgrade Request
 *    │
 *    │  GET /gs-guide-websocket
 *    │  Upgrade: websocket
 *    │  Sec-WebSocket-Protocol: v12.stomp, v11.stomp, v10.stomp
 *    │
 *    ▼
 *   WebSocket Handshake
 *    │
 *    │  HTTP 101 Switching Protocols
 *    │  Sec-WebSocket-Protocol: v12.stomp
 *    │
 *    ▼
 *   WebSocket OPEN
 *    │
 *    │  STOMP CONNECT
 *    ▼
 *   C++ STOMP Server
 *    │
 *    │  STOMP CONNECTED
 *    ▼
 *   app.js onConnect()
 *
 *
 * ============================================================================
 * 1. WebSocket과 STOMP는 서로 다른 계층이다.
 * ============================================================================
 *
 * WebSocket:
 *   HTTP Upgrade
 *        ↓
 *   101 Switching Protocols
 *        ↓
 *   WebSocket connection
 *
 *
 * STOMP:
 *   WebSocket connection이 만들어진 이후
 *        ↓
 *   CONNECT
 *        ↓
 *   CONNECTED
 *        ↓
 *   SUBSCRIBE / SEND / MESSAGE ...
 *
 *
 * 따라서 HTTP 101이 성공했다고 해서
 * app.js의 onConnect()가 호출되는 것은 아니다.
 *
 * ============================================================================
 * 2. 처음에는 WebSocket handshake까지만 성공했다.
 * ============================================================================
 *
 * 처음 C++ 서버에서 확인한 것:
 *
 *   HTTP/1.1 101 Switching Protocols
 *   Upgrade: websocket
 *   Connection: Upgrade
 *   Sec-WebSocket-Accept: ...
 *   Sec-WebSocket-Protocol: v12.stomp
 *
 *
 * 여기까지 성공했다는 것은:
 *
 *   - TCP 연결 성공
 *   - HTTP Upgrade request 수신 성공
 *   - WebSocket handshake 성공
 *   - STOMP WebSocket sub-protocol 협상 성공
 * 을 의미한다.
 *
 *
 * 하지만 이 시점에서는 아직 STOMP CONNECT가 처리되지 않았다.
 *
 * ============================================================================
 * 3. async_accept(req)가 왜 실패했는가?
 * ============================================================================
 *
 * 처음에는 다음과 비슷하게 생각했다.
 *
 *   websocket.async_accept(req, handler);
 *
 * "서버가 HTTP Upgrade request를 받아서 req에 넣어주겠지?"
 *
 *
 * 하지만 Beast의 해당 overload에서 req는
 * "이미 읽어놓은 HTTP request"이다.
 *
 * 즉:
 *   async_accept(req)
 * 는
 *
 *   HTTP request를 읽는다
 * 가 아니라,
 *   이미 읽은 HTTP request를 가지고
 *   WebSocket handshake를 수행한다.
 *
 * 그래서 빈 request를 넘기면:
 *   The WebSocket handshake method was not GET
 * 같은 에러가 발생했다.
 *
 * ============================================================================
 * 4. 직접 HTTP request를 읽는 방식으로 변경
 * ============================================================================
 *
 * 먼저 raw TCP socket에서 HTTP request를 읽는다.
 *   boost::beast::http::read(
 *       *raw_tcp_socket_ptr,
 *       buffer,
 *       req
 *   );
 *
 * 이제 req에서:
 *   - HTTP method
 *   - URI
 *   - Upgrade
 *   - Sec-WebSocket-Protocol
 *   - Sec-WebSocket-Key
 *   - 기타 header
 *
 * 등을 확인할 수 있다.
 * 그 다음:
 *   websocket::stream
 * 을 생성하고:
 *
 *   web_socket_ptr->accept(req);
 * 로 WebSocket handshake를 완료한다.
 *
 *
 * 즉 수동 handshake의 개념은:
 *
 *   TCP socket
 *      ↓
 *   HTTP read
 *      ↓
 *   request 검사
 *      ↓
 *   WebSocket stream 생성
 *      ↓
 *   accept(req)
 *      ↓
 *   WebSocket connection
 *
 * ============================================================================
 * 5. Sec-WebSocket-Protocol은 WebSocket handshake에서 결정한다.
 * ============================================================================
 *
 * app.js가 보낸 request:
 *   Sec-WebSocket-Protocol:
 *       v12.stomp, v11.stomp, v10.stomp
 *
 * C++ 서버는:
 *   v12.stomp
 *
 * 를 지원하도록 했다.
 * 따라서:
 *
 *   Client: v12.stomp, v11.stomp, v10.stomp
 *   Server: v12.stomp
 *
 * 가 되고,
 *
 * HTTP 101 response에는:
 *   Sec-WebSocket-Protocol: v12.stomp
 * 가 들어간다.
 *
 * 이것은 "STOMP CONNECTED"가 아니다.
 *
 * 단지 WebSocket handshake 단계에서
 * "이 WebSocket connection에서 사용할 sub-protocol은
 *  v12.stomp로 하자."
 *
 * 라고 협상한 것이다.
 *
 * ============================================================================
 * 6. WebSocket handshake가 끝난 뒤 STOMP CONNECT가 들어온다.
 * ============================================================================
 *
 * handshake 이후 async_read()를 시작하자
 * 실제로 다음 데이터가 들어왔다.
 *
 *   CONNECT
 *   accept-version:1.2,1.1,1.0
 *   heart-beat:10000,10000
 *
 *   NUL
 *
 *
 * 여기서 중요한 점:
 *   WebSocket frame 안에 STOMP frame이 들어있다.
 *
 * 즉:
 *   WebSocket
 *       ↓
 *   STOMP
 * 관계이다.
 *
 *
 * ============================================================================
 * 7. 왜 처음에는 app.js onConnect()가 호출되지 않았는가?
 * ============================================================================
 *
 * Client:
 *   CONNECT
 *       ↓
 *   Server
 *
 * 까지는 됐다.
 * 하지만 서버가 아무 응답도 하지 않았다.
 *
 * app.js의:
 *   stompClient.onConnect = ...
 *
 * 는 WebSocket connection이 열렸을 때 실행되는 callback이 아니다.
 *
 * STOMP CONNECT를 보낸 후 서버로부터:
 *   CONNECTED
 * frame을 받아야 실행된다.
 *
 * 따라서:
 *   WebSocket OPEN
 *       ↓
 *   STOMP CONNECT
 *       ↓
 *   C++ Server
 *       ↓
 *   STOMP CONNECTED
 *       ↓
 *   app.js onConnect()
 *
 * 순서가 된다.
 *
 *
 * ============================================================================
 * 8. 야매 CONNECTED 응답으로 실제 동작을 확인했다.
 * ============================================================================
 *
 * 일단 STOMP parser를 만들기 전에
 * Session에서 직접 CONNECTED frame을 만들어서 테스트했다.
 *
 *   CONNECTED
 *   version:1.2
 *   heart-beat:10000,10000
 *
 *   NUL
 *
 * 이것을 WebSocket async_write()로 전송하자
 * app.js의 onConnect()가 실제로 호출되었다.
 *
 *
 * 브라우저 콘솔:
 *   Connected: CONNECTED
 *   heart-beat:10000,10000
 *   version:1.2
 *   content-length:0
 *
 * 따라서 다음 사실을 실제로 검증했다.
 *
 *   "C++ Beast WebSocket → STOMP frame → app.js"
 * 전체 경로가 정상적으로 연결되어 있다.
 *
 *
 * ============================================================================
 * 9. STOMP frame의 끝에는 NUL byte가 필요하다.
 * ============================================================================
 * STOMP frame은 대략 다음 구조다.
 *
 *   COMMAND
 *   header:value
 *   header:value
 *
 *   body
 *   NUL
 * 즉 frame 마지막의 '\0'이 STOMP frame terminator이다.
 *
 *
 * 브라우저 네트워크 메시지에서:
 *   ^@
 * 로 보인 것은 NUL byte를 의미한다.
 *
 *
 * ============================================================================
 * 10. C++ std::string과 NUL byte에서 주의할 점
 * ============================================================================
 *
 * 다음 코드는 주의해야 한다.
 *
 *   std::string(
 *       "CONNECTED\n"
 *       "version:1.2\n"
 *       "\0"
 *   );
 *
 * const char*를 받는 std::string 생성자는
 * C-style string의 첫 번째 NUL을 문자열 끝으로 판단한다.
 *
 *
 * 따라서 STOMP frame의 NUL을 확실하게 포함시키려면:
 *
 *   auto frame = std::make_shared<std::string>(
 *       "CONNECTED\n"
 *       "version:1.2\n"
 *   );
 *   frame->push_back('\0');
 * 처럼 명시적으로 넣는 것이 이해하기 쉽다.
 *
 *
 * ============================================================================
 * 11. async_write에서 buffer의 lifetime도 중요하다.
 * ============================================================================
 *
 * async_write()는 호출하는 순간 데이터를 전부 복사하는 것이 아니다.
 * 비동기 operation이 완료될 때까지
 * buffer가 가리키는 메모리가 살아 있어야 한다.
 *
 * 그래서 테스트 코드에서는:
 *   auto frame = std::make_shared<std::string>(...);
 *
 *   websocket.async_write(
 *       asio::buffer(*frame),
 *       [frame](...)
 *       {
 *           ...
 *       }
 *   );
 * 처럼 callback에서 frame을 capture하여
 * write가 끝날 때까지 lifetime을 유지했다.
 *
 * ============================================================================
 * 12. 현재 Session의 역할
 * ============================================================================
 *
 * 현재 Session은 우선 WebSocket transport 역할을 한다.
 *
 *   Session
 *      │
 *      ├── TCP socket
 *      │
 *      ├── HTTP Upgrade
 *      │
 *      ├── WebSocket handshake
 *      │
 *      ├── WebSocket async_read
 *      │
 *      └── WebSocket async_write
 *             │
 *             ▼
 *        StompHandler
 *
 * 이후에는 STOMP protocol 처리를 StompHandler로 넘기는 것이 좋다.
 *
 * ============================================================================
 * 13. 앞으로 구현할 구조
 * ============================================================================
 *
 *   Session
 *      │
 *      │ WebSocket message
 *      ▼
 *   StompHandler
 *      │
 *      ├── CONNECT
 *      │      └── CONNECTED
 *      │
 *      ├── SUBSCRIBE
 *      │      └── subscription 등록
 *      │
 *      ├── UNSUBSCRIBE
 *      │
 *      ├── SEND
 *      │      └── MsgBroker / Redis
 *      │
 *      └── DISCONNECT
 *      ...
 *
 * ============================================================================
 * 14. WebSocket read/write 동시성
 * ============================================================================
 *
 * Beast WebSocket에서는 일반적으로:
 *
 *   async_read  1개
 *   async_write 1개
 *
 * 를 동시에 수행할 수 있다.
 * 하지만 여러 곳에서 동시에 async_write()를 호출하면 안 된다.
 * 예:
 *
 *   Redis message
 *       ↓
 *   Session A → write
 *
 *   다른 이벤트
 *       ↓
 *   Session A → 또 다른 write
 *
 * 이런 식으로 write가 동시에 발생할 수 있기 때문에
 * 실제 서버에서는 write queue를 두고 직렬화하는 구조가 필요하다.
 *
 * 현재 Session에 있는 strand는
 * 이런 session-level serialization을 구현할 때 활용할 수 있다.
 *
 * ============================================================================
 * 15. 현재 구현의 한계
 * ============================================================================
 *
 * 현재 start()에서는:
 *
 *   http::read()
 *   websocket.accept()
 *
 * 를 synchronous 방식으로 수행한다.
 *
 * 따라서 클라이언트가 TCP 연결만 맺고
 * HTTP request를 보내지 않는다면
 * 해당 작업이 blocking될 수 있다.
 *
 * 또한 Server의 accept loop도 synchronous accept 방식이다.
 *
 * 현재는 학습 / 테스트용으로 충분하지만
 * 실제 서버 구조에서는 HTTP handshake까지
 * asynchronous하게 처리하는 방향을 고려할 수 있다.
 *
 * ============================================================================
 * 최종적으로 기억할 것
 * ============================================================================
 *
 *   HTTP 101
 *       =
 *   WebSocket handshake 성공
 *
 *   CONNECT
 *       =
 *   STOMP client → server 요청
 *
 *   CONNECTED
 *       =
 *   STOMP server → client 응답
 *
 *   onConnect()
 *       =
 *   app.js가 CONNECTED를 정상적으로 받은 뒤 실행
 *
 *
 * 즉:
 *   "WebSocket 연결 성공" != "STOMP 연결 성공"
 * 이 차이를 이해하는 것이 오늘 구현에서 가장 중요한 내용이었다.
 *
 * ============================================================================
 */
