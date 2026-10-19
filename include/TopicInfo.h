//
// Created by 박동빈 on 2026-10-11.
//

#ifndef TOPICINFO_H
#define TOPICINFO_H

#include <string>
#include <utility>

class TopicInfo
{
public:
  explicit TopicInfo(
    std::string  input_topic,
    std::string  input_id_str,
    const int input_idx,
    const long input_session_id
  ) :
    topic(std::move(input_topic)), // to avoid unnecessary copies
    id_str(std::move(input_id_str)),
    idx(input_idx),
    session_id(input_session_id)
  {}

  std::string topic;
  std::string id_str;
  int idx;
  long session_id;
};

#endif //TOPICINFO_H
