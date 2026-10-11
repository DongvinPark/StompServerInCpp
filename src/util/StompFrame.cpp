//
// Created by 박동빈 on 2026-10-10.
//
#include "../include/StompFrame.h"
#include "../constants/Util.h"

void StompFrame::parse(const std::string& req)
{
    if (req.empty())
        return;

    // Heartbeat case 1 : req == "\n".
    if (req.length() == 1 && req == C::HEART_BEAT_STR)
    {
        command = C::HEART_BEAT_CMD;
        is_valid = true;
        return;
    }

    // A complete STOMP frame must end with a NULL octet.
    if (req.back() != C::STOMP_FRAME_NUL_OCTET)
        return;

    const std::string_view frame(req.data(), req.size() - 1);

    // Heartbeat case 2 : LF or CRLF.
    if (frame == "\n" || frame == "\r\n")
    {
        command = C::HEART_BEAT_CMD;
        is_valid = true;
        return;
    }

    // Read one line, supporting LF and CRLF.
    auto read_line = [](std::string_view input,
                        std::size_t& pos,
                        std::string_view& line) -> bool
    {
        const auto end = input.find('\n', pos);

        if (end == std::string_view::npos)
            return false;

        line = input.substr(pos, end - pos);

        if (!line.empty() && line.back() == '\r')
            line.remove_suffix(1);

        pos = end + 1;
        return true;
    };

    std::size_t pos = 0;
    std::string_view line;

    // 1. Command line.
    if (!read_line(frame, pos, line) || line.empty())
        return;

    command.assign(line);

    // 2. Headers until the first blank line.
    bool found_separator = false;

    while (read_line(frame, pos, line))
    {
        if (line.empty())
        {
            found_separator = true;
            break;
        }

        const auto colon = line.find(':');

        if (colon == std::string_view::npos || colon == 0)
            return;

        std::string key(line.substr(0, colon));
        std::string value(line.substr(colon + 1));

        // STOMP 1.2: the first occurrence of a header wins.
        headers.try_emplace(std::move(key), std::move(value));
    } //wh

    if (!found_separator)
        return;

    // 3. Parse body.
    const bool body_allowed
        = command == "SEND" || command == "MESSAGE" || command == "ERROR";

    if (headers.contains("content-length"))
    {
        // content-length specifies a BYTE count, not a character count.
        std::size_t length = 0;
        const std::string& value = headers.at("content-length");

        if (value.empty())
            return;

        const auto [ptr, ec] = std::from_chars(
            value.data(),
            value.data() + value.size(),
            length
        );

        if (ec != std::errc{} ||
            ptr != value.data() + value.size())
            return;

        if (!body_allowed)
            return;

        if (length > frame.size() - pos)
            return;

        body.assign(frame.data() + pos, length);
        pos += length;

        // Exactly one NULL terminator was removed from req above.
        // No other bytes may follow the declared body.
        if (pos != frame.size())
            return;
    }
    else
    {
        // Without content-length, the first NULL terminates the body.
        // The trailing NULL was removed above, so an embedded NULL
        // is not valid in this branch.
        if (frame.find(C::STOMP_FRAME_NUL_OCTET) != std::string_view::npos)
            return;

        if (pos < frame.size())
        {
            if (!body_allowed)
                return;

            body.assign(frame.substr(pos));
        }
    }

    is_valid = true;
}

std::string StompFrame::getCommand()
{
    return command;
}

bool StompFrame::isValid() const
{
    return is_valid;
}

std::string StompFrame::getBody() const
{
    return body;
}

std::string StompFrame::getTopic(const char* header_key)
{
    if (headers.empty())
    {
        return C::EMPTY_STR;
    }

    if (headers.contains(header_key))
    {
        return headers.at(header_key);
    } else
    {
        return C::EMPTY_STR;
    }
}
