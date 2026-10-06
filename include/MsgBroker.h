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
  explicit MsgBroker();

  // Rule of five. MsgBroker object is not allowed to copy or move.
  MsgBroker(const MsgBroker&) = delete;
  MsgBroker& operator=(const MsgBroker&) = delete;
  MsgBroker& operator=(MsgBroker&&) noexcept = delete;
  MsgBroker(MsgBroker&&) noexcept = delete;

  ~MsgBroker();

  void shutdown();

  void subscribe(const std::string& topic, std::shared_ptr<Session> session_ptr);

  void unsubscribe(const std::string& topic, std::shared_ptr<Session> session_ptr);

  uint32_t sendMsgToAllSesisons(const std::string& topic, const std::string& message);

private:
  std::shared_ptr<Logger> logger;
  std::unordered_map<
    std::string, std::vector<std::shared_ptr<Session>>
  > topic_session_map{};
};

#endif //MSGBROKER_H
