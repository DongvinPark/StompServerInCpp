//
// Created by 박동빈 on 2026. 10. 6..
//
#include "../include/StompHandler.h"

#include "../../constants/Util.h"
#include "../../include/Session.h"
#include "../include/Logger.h"

StompHandler::StompHandler(
  std::weak_ptr<Session> input_session_ptr
):
  logger(Logger::getLogger(C::STOMP_HANDLER)),
  parent_session_ptr(std::move(input_session_ptr))
{
}

StompHandler::~StompHandler()
{
}

/**
 * 처리에 실패하면 빈 문자열을 리턴한다. nullptr은 리턴하지 않는다.
*/
std::string StompHandler::handleStompReq(const std::string& req, bool& is_disconnected)
{
  if (
    const char last = req.back();
    last != C::STOMP_FRAME_NUL_OCTET
  )
  {
    return C::EMPTY_STR;
  }

  const std::vector<std::string> req_parts
    = Util::splitToVecBySingleChar(req, C::SINGLE_BACK_SLASH);

  if (req_parts.size() <= 0)
  {
    logger->severe("invalid req!");
    return C::EMPTY_STR;
  }

  logger->info3(">>> req from client :");
  logger->info3(req);

  // TODO : 헤더 Key & Value 들을 파싱하는 부분을 나중에 추가해야 한다.
  const auto& method = req_parts[0];

  if (const auto ptr_for_parent_session = parent_session_ptr.lock())
  {
    if (method == "CONNECT")
    {
      std::string frame =
        "CONNECTED\n"
        "version:1.2\n"
        "heart-beat:10000,10000\n"
        "\n";
      frame.push_back(C::STOMP_FRAME_NUL_OCTET);
      return frame;
    }
    else if (method == "SUBSCRIBE")
    {
      // TODO : implement later
      return C::EMPTY_STR;
    }
    else if (method == "UNSUBSCRIBE")
    {
      // TODO : implement later
      return C::EMPTY_STR;
    }
    else if (method == "SEND")
    {
      // TODO : implement later
      return C::EMPTY_STR;
    }
    /*  서버는 SEND로 받은 메시지를 타킷 클라이언트들한테 전송할 때, MESSAGE 메서드를 사용한다.
     *  클라이언트가 MESSAGE 메서드로 요청을 하는 경우는 없다.
     *else if (method == "MESSAGE") { return C::EMPTY_STR; }*/
    else if (method == "DISCONNECT")
    {
      // receipt-id 로는 세션 아이디를 달아서 준다.
      long session_id = ptr_for_parent_session->getSessionId();
      std::string frame =
        "RECEIPT\n"
        "receipt-id:" + std::to_string(session_id) + "\n"
        "\n";
      frame.push_back(C::STOMP_FRAME_NUL_OCTET);
      is_disconnected = true;
      return frame;
    }
    else
    {
      logger->severe("Not supporting STOMP method! : " + method);
      return C::EMPTY_STR;
    }
  }
  else
  {
    logger->severe("Parent Session is Null!");
    return C::EMPTY_STR;
  }
}
