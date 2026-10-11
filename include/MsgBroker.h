//
// Created by 박동빈 on 2026-10-03.
//

#ifndef MSGBROKER_H
#define MSGBROKER_H
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Session.h"

class MsgBroker : public std::enable_shared_from_this<MsgBroker>
{
public:
    explicit MsgBroker(boost::asio::io_context& input_io_context);

    // Rule of five. MsgBroker object is not allowed to copy or move.
    MsgBroker(const MsgBroker&) = delete;
    MsgBroker& operator=(const MsgBroker&) = delete;
    MsgBroker& operator=(MsgBroker&&) noexcept = delete;
    MsgBroker(MsgBroker&&) noexcept = delete;

    ~MsgBroker();

    void subscribe(
        const std::string& topic, const std::string& id_str, std::shared_ptr<Session> session_ptr
    );

    void unsubscribe(
        std::shared_ptr<Session> session_ptr, TopicInfo topic_info
    );

    int sendMsgToAllSesisons(const std::string& topic, const std::string& message);

    void deleteSession(std::shared_ptr<Session> session_ptr);

private:
    void printMap();

    std::shared_ptr<Logger> logger;
    boost::asio::strand<boost::asio::io_context::executor_type> strand;

    /**
     * topic 스트링 : map<br/>
     * _________________|인덱스(0~9) : session 포인터 벡터<br/>
     * 하나의 토픽에 대해서 10 개의 스레드가 각가의 세션 포인터 벡터에 접근하게 만든다.<br/>
     * 이로써 동기화 필요성을 제거함과 동시에 응답 성능을 개선시킨다.
     */
    std::unordered_map<
        std::string,
        std::unordered_map<
            int, std::vector<std::shared_ptr<Session>>
        >
    > topic_session_map{};

    std::atomic_int topic_sub_cnt{0};
};

#endif //MSGBROKER_H
