//
// Created by 박동빈 on 2026. 10. 6..
//
#include "../include/MsgBroker.h"

MsgBroker::MsgBroker(boost::asio::io_context& input_io_context) :
        logger(Logger::getLogger(C::MSG_BROKER)),
        strand(boost::asio::make_strand(input_io_context))
{
}

MsgBroker::~MsgBroker()
{
  topic_session_map.clear();
}

void MsgBroker::subscribe(const std::string& topic, std::shared_ptr<Session> session_ptr)
{
  // boost asio strand를 써서 data race를 방지한다.
}

void MsgBroker::unsubscribe(const std::string& topic, std::shared_ptr<Session> session_ptr)
{
  // boost asio strand를 써서 data race를 방지한다.
}

uint32_t MsgBroker::sendMsgToAllSesisons(const std::string& topic, const std::string& message)
{
  return C::INVALID;
}

void MsgBroker::deleteSession(long session_id)
{
  // TODO : 세션을 제거 했을 때의 동작을 여기에 정의해야 한다. 제거한 세션이 구독한거 전부 취소 한다던지.
  // boost asio strand를 써서 data race를 방지한다.
}