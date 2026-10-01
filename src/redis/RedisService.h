//
// Created by 박동빈 on 2026. 9. 28..
//

#ifndef REDISMESSAGESUBSCRIBER_H
#define REDISMESSAGESUBSCRIBER_H

#include <iostream>
/*
 * boost/redis 관련 두 가지 include 는 프로젝트 전체에서 오직 1 번씩만 정의돼야 한다.
 * 그렇지 않을 경우(ex : boost::redis::connection 객체를 다른 곳에서 만들고 생성자에 참조 전달 하는 경우)
 * 'duplicate symbol' 관련 에러가 뜨면서 빌드에 실패한다.
 * 따라서, RedisService 같은 레디스 전용 클래스를 만들고,
 * 그 안에서만 connection을 딱 하나만 만들어서 사용하는 구조가 권장된다.
 */
#include <boost/redis.hpp>
#include <boost/redis/src.hpp>

#include <boost/redis/connection.hpp>
#include <boost/redis/push_parser.hpp>

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/consign.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/signal_set.hpp>

#include "../constants/C.h"
#include <../include/Logger.h>
#include <boost/asio.hpp>

class RedisService : public std::enable_shared_from_this<RedisService>
{
public:
  explicit RedisService(
    boost::asio::io_context& input_io_context
    // TODO : 나중에 여기에는 각종 msg_tx 용 객체들이 정의돼야 한다.
    //  server socket 을 돌리는 Server 객체는 Session 을 만들 뿐, 응답을 전송하지는 않기 때문이다.
  ) : logger(Logger::getLogger(C::REDIS_MSG_SUBSCRIBER)),
      io_context(input_io_context),
      redis_conn(io_context),
      strand(boost::asio::make_strand(io_context))
  {
  }

  ~RedisService()
  {
    redis_conn.cancel();
  }

  void init()
  {
    auto self = shared_from_this();
    cfg.addr.host = C::REDIS_HOST_IP;
    cfg.addr.port = C::REDIS_PORT;

    // 이 부분은 건드리지 않는게 좋다. boost::asio::detached 대신
    // 다른 Completion Token 람다를 넣어 봤지만, 무슨 이유에서인지 람다가 실행되지 않아서 그렇다.
    // 번거롭지만 초기화를 확인하는건 아예 별도의 테스트 함수(void verifyRedisConnection())로 실행하고,
    // 연결 여부를 caller 쪽에서 확인하고 대응하게 만들었다.
    redis_conn.async_run(cfg, {}, boost::asio::detached);
  }

  bool get_is_ready()
  {
    return is_ready.load();
  }


  void verifyRedisConnection()
  {
    // 이걸로 RedisSession class 멤버필드에 접근 가능.
    // shared_from_this() 롤 사용하기 위해서는 RedisService.h 자체가 shared_ptr로 초기화 돼야 한다.
    auto self = shared_from_this();
    auto reqPtr = std::make_shared<boost::redis::request>();
    auto resPtr = std::make_shared<boost::redis::response<std::string>>();
    reqPtr->push("PING");

    // exec redis cmd in async mode
    std::cout << "Redis Ping Pong Test Start! \n";
    self->redis_conn.async_exec(
      *reqPtr,
      *resPtr,
      [self, resPtr](const boost::system::error_code& ec, std::size_t)
      {
        if (ec)
        {
          std::cerr << "Redis PING failed: " << ec.message() << "\n";
          return;
        }
        self->is_ready.store(true); // ping pong test 결과 기록
        std::cout << "PING: " << std::get<0>(*resPtr).value() << "\n";
      }
    );

    // 아래와 같은 '스레드 고의 정지' 같은 '멋지지 않은 방법' 대신 io_context 를 써서 좀 더 'fancy'하게
    // 처리해보려 했지만 결국 실패하고, 결국 '안 멋지지만 확실히 작동하는 코드'로 롤백했다.
    // io_context 를 써서 'fancy'하게 구현한 것은 boost 1.9x 버전의 Mac 에서는 잘 작동했지만,
    // boost 1.86 버전인 윈도우 11 에서는 알 수 없는 에러로 실패했던 것이다.

    // 아무튼 아래의 코드가 있어야 Windows 환경에서는 redis_conn.async_exec(...) 가 정확하게 작동한다.
    // 이 함수가 끝나는 것을 고의적으로 지연시켜서 async_exec()가 지역 변수들을 참조해서 일을 처리할 때
    // bad memory access 가 나지 않게 해주기 때문이다.
    // TODO : 때로는 '우아하고 멋져보이는 코드' 보다는 '지루하고 뻔하지만 작동하는게 보장되는 코드'가 가치 있다.
    // TODO : 이번 프로젝트 처럼 Boost Lib 같은 외부 라이브러리에 의존해야 하면서
    // TODO : 해당 라이브러리의 버전이 실행하는 운영체제 마다 다른 경우엔 특히 그렇다.
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
  }


  void publishMsg(const std::string& msg)
  {
    auto self = shared_from_this();
    auto reqPtr = std::make_shared<boost::redis::request>();
    auto resPtr = std::make_shared<boost::redis::response<std::string>>();
    const std::string cmd = "PUBLISH " + std::string(C::REDIS_PUB_SUB_CHANNEL) + "\"" + msg + "\"";
    reqPtr->push(cmd);

    // exec redis cmd in async mode
    self->redis_conn.async_exec(
      *reqPtr,
      *resPtr,
      [self, resPtr, msg](const boost::system::error_code& ec, std::size_t)
      {
        if (ec)
        {
          std::cerr << "Redis PUBLISH failed: " << ec.message() << "\n";
          return;
        }
        std::cout << "Pub to Redis : " << msg << "\n";
      }
    );
  }

  void startPubSubListening()
  {
    if (is_listening.load())
    {
      logger->warning("Already Listening Pub/Sub channel!");
      return;
    }
    is_listening.store(true);
    boost::asio::co_spawn(
      io_context,
      co_entry(cfg),
      boost::asio::detached
    );
  }

  void shutdown()
  {
    is_ready.store(false);
    is_listening.store(false);
    is_shutdown.store(true);
    redis_conn.cancel();
  }

private:
  // 레디스에 푸시된 메시지를 리스닝한다.
  auto pub_sub_listener(
    std::shared_ptr<boost::redis::connection> conn) -> boost::asio::awaitable<void>
  {
    boost::redis::generic_flat_response resp;
    conn->set_receive_response(resp);

    // 채널을 구독한다. 여러개 구독할 수도 있다.
    boost::redis::request req;
    req.subscribe({C::REDIS_PUB_SUB_CHANNEL});
    co_await
      conn->async_exec(req);

    // 채널(또는 채널들) 구독이 완료됐다. 채널에 푸시된 메시지들은 resp에 쌓인다.
    // 커넥션이 네트워크 에러 떠서 레디스에 다시 연결할 때, 채널들을 자동으로 다시 구독한다.
    // 그러기 위해서는 request::subscribe()를 호출해야 한다.
    while (conn->will_reconnect())
    {
      // 메시지 도착을 기다린다.
      auto [ec] = co_await
        conn->async_receive2(boost::asio::as_tuple);

      // 에러 체크.
      if (ec)
      {
        std::cerr << "Error during receive: " << ec << "\n";
        break;
      }

      // 권한 부족 등의 이유로 아래의 에러체크문이 실행될 수도 있다.
      if (ec)
      {
        std::cerr << "The receive response contains an error: "
          << resp.error().diagnostic << "\n";
        break;
      }

      // 받은 응답은 코루틴을 suspend 하지 않으면서 즉각 소비돼야 한다. 즉, async operation 으로 소비하면 안 된다.
      for (boost::redis::push_view elem : boost::redis::push_parser(resp.value()))
      {
        std::cout << "Pub/Sub channel Listening success! : " << elem.channel
          << ": " << elem.payload << "\n";
      }

      resp.value().clear();
    } //wh
  } //end of


  auto co_entry(boost::redis::config cfg) -> boost::asio::awaitable<void>
  {
    auto ex = co_await boost::asio::this_coro::executor;
    auto conn = std::make_shared<boost::redis::connection>(ex);
    co_spawn(ex, pub_sub_listener(conn), boost::asio::detached);
    conn->async_run(
      cfg,
      boost::asio::consign(boost::asio::detached, conn)
    );

    boost::asio::signal_set sig_set(ex, SIGINT, SIGTERM);
    co_await
      sig_set.async_wait();

    conn->cancel();
  }

  std::shared_ptr<Logger> logger;
  boost::asio::io_context& io_context;
  boost::redis::connection redis_conn;
  boost::redis::config cfg;

  // used strand to reduce cache miss
  boost::asio::strand<boost::asio::io_context::executor_type> strand;

  std::atomic<bool> is_ready{false};
  std::atomic<bool> is_listening{false};
  std::atomic<bool> is_shutdown{false};

  // TODO : 나중에 여기에는 각종 msg_tx 용 객체들이 정의돼야 한다.
  //  server socket 을 돌리는 Server 객체는 Session 을 만들 뿐, 응답을 전송하지는 않기 때문이다.
};

#endif //REDISMESSAGESUBSCRIBER_H
