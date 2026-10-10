//
// Created by 박동빈 on 2026-10-10.
//

#ifndef STOMPFRAME_H
#define STOMPFRAME_H

#include <string>
#include <unordered_map>

class StompFrame
{
public:
    // Rule of 5 : 아무 것도 정의하지 않는다. 마치 int 처럼 작동한다.

    void parse(const std::string& req);

    [[nodiscard]] std::string getCommand();

    [[nodiscard]] bool isValid() const;

    [[nodiscard]] std::string getBody() const;

private:
    std::string command{};
    std::unordered_map<std::string, std::string> headers{};
    std::string body{};
    bool is_valid{false};
};

#endif //STOMPFRAME_H
