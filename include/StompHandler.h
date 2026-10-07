//
// Created by 박동빈 on 2026. 9. 28..
//

#ifndef STOMPHANDLER_H
#define STOMPHANDLER_H
#include <memory>

#include "../constants/C.h"
#include "Logger.h"
#include "Buffer.h"

class Session;

class StompHandler
{
public:
  explicit StompHandler(
    std::weak_ptr<Session> input_session_ptr
  );

  ~StompHandler();

  std::string handleStompReq(const std::string& req, bool& is_disconnected);

private:
  std::shared_ptr<Logger> logger;
  std::weak_ptr<Session> parent_session_ptr;
};

#endif //STOMPHANDLER_H
