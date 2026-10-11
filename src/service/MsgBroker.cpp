//
// Created by 박동빈 on 2026. 10. 6..
//
#include "../include/MsgBroker.h"

#include <iostream>

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
    auto self = shared_from_this();
    boost::asio::post(
        self->strand,
        [self, topic, session_ptr]()
        {
            // 인덱스 계산.
            ++self->topic_sub_cnt;
            int idx = self->topic_sub_cnt % C::TOPIC_INDEX_SIZE;
            if (idx == 0) { self->topic_sub_cnt = 0; }

            // 이거 한 줄이면 아래의 복잡한 if-else를 전부 대체할 수 있다.
            // std::unordered_map 의 기본 생성 동작이 JAVA의 put-if-absent 처럼 작동하기 때문이다.
            auto& ptr_vec = self->topic_session_map[topic][idx];
            ptr_vec.push_back(session_ptr);
            // self->printMap(); // 개발 & 체크용.
            /*
            if (self->topic_session_map.contains(topic)) // 토픽이 이미 있나?
            {
                auto& idx_ptr_map = self->topic_session_map[topic];
                // 맵 안에 idx 벡터가 있나?
                if (idx_ptr_map.contains(idx))
                {
                    auto& ptr_vec = idx_ptr_map[idx];
                    ptr_vec.push_back(session_ptr);
                }
                else // 없으면 새로 할당후 추가.
                {
                    idx_ptr_map.emplace(idx, std::vector<std::shared_ptr<Session>>());
                    idx_ptr_map[idx].push_back(session_ptr);
                }
            }
            else // 없으면 새로 할당
            {
                self->topic_session_map.emplace(
                    topic,
                    std::unordered_map<int, std::vector<std::shared_ptr<Session>>>()
                );
                self->topic_session_map[topic].emplace(
                    idx, std::vector<std::shared_ptr<Session>>()
                );
                auto& ptr_vec = self->topic_session_map[topic][idx];
                ptr_vec.push_back(session_ptr);
            }*/
        }
    );
}

void MsgBroker::unsubscribe(const std::string& topic, std::shared_ptr<Session> session_ptr)
{
    // boost asio strand를 써서 data race를 방지한다.
}

/**
 * 메시지를 전송한 세션의 개수를 리턴한다.
 */
int MsgBroker::sendMsgToAllSesisons(const std::string& topic, const std::string& message)
{
    return C::INVALID;
}

void MsgBroker::deleteSession(long session_id)
{
    // TODO : 세션을 제거 했을 때의 동작을 여기에 정의해야 한다. 제거한 세션이 구독한거 전부 취소 한다던지.
    // boost asio strand를 써서 data race를 방지한다.
}

inline void MsgBroker::printMap()
{
    auto self = shared_from_this();

    boost::asio::post(
        self->strand,
        [self]()
        {
            std::cout << "\n========== Topic Session Map ==========\n";
            std::cout << "Total topics: "
                      << self->topic_session_map.size() << '\n';

            for (const auto& [topic, idx_ptr_map]
                 : self->topic_session_map)
            {
                std::cout << "\nTopic: " << topic << '\n';
                std::cout << "  Index buckets: "
                          << idx_ptr_map.size() << '\n';

                for (const auto& [idx, session_vec] : idx_ptr_map)
                {
                    std::cout << "  Index: " << idx
                              << " | Sessions: "
                              << session_vec.size() << '\n';

                    for (const auto& session_ptr : session_vec)
                    {
                        if (session_ptr)
                        {
                            std::cout << "    Session address: "
                                      << session_ptr.get() << '\n';
                        }
                        else
                        {
                            std::cout << "    Session: nullptr\n";
                        }
                    }
                }
            }

            std::cout << "=======================================\n\n";
        }
    );
}