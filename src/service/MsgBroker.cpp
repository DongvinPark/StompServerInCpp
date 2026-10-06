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

}

void MsgBroker::shutdown()
{
  topic_session_map.clear();
}

void MsgBroker::subscribe(const std::string& topic, std::shared_ptr<Session> session_ptr)
{
  // TODO : thread safe 가 돼야 할 수도 있다.
}

void MsgBroker::unsubscribe(const std::string& topic, std::shared_ptr<Session> session_ptr)
{
  // TODO : thread safe 가 돼야 할 수도 있다.
}

uint32_t MsgBroker::sendMsgToAllSesisons(const std::string& topic, const std::string& message)
{
  return C::INVALID;
}