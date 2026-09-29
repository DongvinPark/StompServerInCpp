//
// Created by 박동빈 on 2026. 9. 28..
//

#ifndef STOMPHANDLER_H
#define STOMPHANDLER_H
#include <memory>

#include "../constants/C.h"
#include "../include/Logger.h"
#include "../include/Buffer.h"

class Session;

class StompHandler
{
public:
  explicit StompHandler(
    std::weak_ptr<Session> input_session_ptr
  );
  ~StompHandler();

  void run(Buffer& buf);
  void handleStompRequest(std::string reqStr, Buffer& buf);

private:
  std::shared_ptr<Logger> logger;
  std::weak_ptr<Session> parent_session;
};

#endif //STOMPHANDLER_H
