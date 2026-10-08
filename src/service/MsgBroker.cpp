//
// Created by 박동빈 on 2026. 10. 6..
//
#include "../include/MsgBroker.h"

MsgBroker::MsgBroker() :
        logger(Logger::getLogger(C::MSG_BROKER))
{
}

MsgBroker::~MsgBroker()
{
  topic_session_map.clear();
}

void MsgBroker::subscribe(const std::string& topic, std::shared_ptr<Session> session_ptr)
{
  // TODO : thread safe 가 필수일 듯한 느낌이다.
}

void MsgBroker::unsubscribe(const std::string& topic, std::shared_ptr<Session> session_ptr)
{
  // TODO : thread safe 가 필수일 듯한 느낌이다.
}

uint32_t MsgBroker::sendMsgToAllSesisons(const std::string& topic, const std::string& message)
{
  return C::INVALID;
}

void MsgBroker::deleteSession(long session_id)
{
  // TODO : 세션을 제거 했을 때의 동작을 여기에 정의해야 한다. 제거한 세션이 구독한거 전부 취소 한다던지.
  // TODO : thread safe 가 필수일 듯한 느낌이다.
}