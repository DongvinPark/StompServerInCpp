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

#include "../constants/C.h"
#include <../include/Logger.h>
#include <boost/asio.hpp>

class RedisService {
public:
  explicit RedisService(
    boost::asio::io_context& input_io_context,
    std::vector<std::shared_ptr<boost::asio::io_context>>& input_worker_io_context_pool
    // TODO : 나중에 여기에는 각종 msg_tx 용 객체들이 정의돼야 한다.
    //  server socket 을 돌리는 Server 객체는 Session 을 만들 뿐, 응답을 전송하지는 않기 때문이다.
    ) : logger(Logger::getLogger(C::REDIS_MSG_SUBSCRIBER)),
      io_context(input_io_context),
      worker_io_context_pool(input_worker_io_context_pool),
      redis_conn(io_context)
  {
    boost::redis::config cfg;
    cfg.addr.host = C::REDIS_HOST_IP;
    cfg.addr.port = C::REDIS_PORT;
    redis_conn.async_run( cfg, boost::asio::detached);
  };
  ~RedisService(){
    redis_conn.cancel();
  }

  void redisPingPongTest(){
    // Create Redis req/res
    boost::redis::request redisReq;
    redisReq.push("PING");
    boost::redis::response<std::string> redisRes;

    // exec redis cmd in async mode
    redis_conn.async_exec(
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

    // 호출자의 스레드를 async_exec가 실행을 마칠 때까지 대가하게 만들지 않으면 SIGABRT 에러가 뜬다.
    // connTest() 메서드 호출 후 redisRes 가 함수 범위를 벗어나서 바로
    // 소멸돼 버리기 때문에, async_exec() 람다 내부에서 '잘못된 메모리 주소 접근'이 발생하는 것이다.
    std::this_thread::sleep_for(std::chrono::microseconds(3000));
  }

  void publishMsg(const std::string& msg){

  }

  void startPubSubListening(){

  }

  void shutdown(){
    redis_conn.cancel();
  }

private:
  std::shared_ptr<Logger> logger;
  boost::asio::io_context& io_context;
  std::vector<std::shared_ptr<boost::asio::io_context>>& worker_io_context_pool;
  boost::redis::connection redis_conn;

  // TODO : 나중에 여기에는 각종 msg_tx 용 객체들이 정의돼야 한다.
  //  server socket 을 돌리는 Server 객체는 Session 을 만들 뿐, 응답을 전송하지는 않기 때문이다.
};

#endif //REDISMESSAGESUBSCRIBER_H
