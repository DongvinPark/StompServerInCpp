//
// Created by user on 2026-10-03.
//

#ifndef MSGBROKER_H
#define MSGBROKER_H
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Session.h"

class MsgBroker
{
public:
    explicit MsgBroker() :
        logger(Logger::getLogger(C::MSG_BROKER))
    {
    }

    // Rule of five. MsgBroker object is not allowed to copy or move.
    MsgBroker(const MsgBroker&) = delete;
    MsgBroker& operator=(const MsgBroker&) = delete;
    MsgBroker& operator=(MsgBroker&&) noexcept = delete;
    MsgBroker(MsgBroker&&) noexcept = delete;

    ~MsgBroker()
    {

    }

    void shutdown()
    {
        topic_session_map.clear();
    }

    void subscribe(const std::string& topic, std::shared_ptr<Session> session_ptr)
    {
        // TODO : thread safe 가 돼야 할 수도 있다.
    }

    void unsubscribe(const std::string& topic, std::shared_ptr<Session> session_ptr)
    {
        // TODO : thread safe 가 돼야 할 수도 있다.
    }

    uint32_t sendMsgToAllSesisons(const std::string& topic, const std::string& message)
    {
        return C::INVALID;
    }

private:
    std::shared_ptr<Logger> logger;
    std::unordered_map<
        std::string, std::vector<std::shared_ptr<Session>>
    > topic_session_map{};
};

#endif //MSGBROKER_H
