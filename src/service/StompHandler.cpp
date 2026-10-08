//
// Created by 박동빈 on 2026. 10. 6..
//
#include "../include/StompHandler.h"

#include "../../constants/Util.h"
#include "../../include/Session.h"
#include "../include/Logger.h"

StompHandler::StompHandler(
    std::weak_ptr<Session> input_session_ptr
):
    logger(Logger::getLogger(C::STOMP_HANDLER)),
    parent_session_ptr(std::move(input_session_ptr))
{
}

StompHandler::~StompHandler()
{
}

/**
 * 처리에 실패하면 빈 문자열을 리턴한다. nullptr은 리턴하지 않는다.
*/
std::string StompHandler::handleStompReq(
    const std::string& req, std::shared_ptr<bool> is_disconnected_ptr
)
{
    const std::vector<std::string> req_parts
        = Util::splitToVecBySingleChar(req, C::SINGLE_BACK_SLASH_CHAR);

    if (req_parts.empty())
    {
        logger->severe("invalid req!");
        return C::HEART_BEAT_STR;
    }

    if (req.back() != C::SINGLE_BACK_SLASH_CHAR)
    {
        // heart beat 요청은 출력하지 않는다.
        logger->info3(">>> req from client :");
        logger->info3("\n" + req);
    }

    // TODO : 헤더 Key & Value 들을 파싱하는 부분을 나중에 추가해야 한다.
    const auto& method = req_parts[0];

    if (const auto ptr_for_parent_session = parent_session_ptr.lock())
    {
        if (method == "CONNECT")
        {
            std::string frame =
                "CONNECTED\n"
                "version:1.2\n"
                "heart-beat:"
                + std::to_string(C::HEART_BEAT_MS)
                + "," + std::to_string(C::HEART_BEAT_MS) + "\n"
                "\n";
            frame.push_back(C::STOMP_FRAME_NUL_OCTET);
            return frame;
        }
        else if (method == "SUBSCRIBE")
        {
            // TODO : implement later
            return C::HEART_BEAT_STR;
        }
        else if (method == "UNSUBSCRIBE")
        {
            // TODO : implement later
            return C::HEART_BEAT_STR;
        }
        else if (method == "SEND")
        {
            // TODO : implement later
            return C::HEART_BEAT_STR;
        }
        /*  서버는 SEND로 받은 메시지를 타킷 클라이언트들한테 전송할 때, MESSAGE 메서드를 사용한다.
         *  클라이언트가 MESSAGE 메서드로 요청을 하는 경우는 없다.
         *else if (method == "MESSAGE") { return C::HEART_BEAT_STR; }*/
        else if (method == "DISCONNECT")
        {
            // receipt-id 로는 세션 아이디를 달아서 준다.
            long session_id = ptr_for_parent_session->getSessionId();
            std::string frame =
                "RECEIPT\n"
                "receipt-id:" + std::to_string(session_id) + "\n"
                "\n";
            frame.push_back(C::STOMP_FRAME_NUL_OCTET);
            *is_disconnected_ptr = true;
            return frame;
        }
        else
        {
            // 이때는 heart-beat 라고 봐야 한다.
            return C::HEART_BEAT_RESULT;
        }
    }
    else
    {
        logger->severe("Parent Session is Null!");
        return C::HEART_BEAT_STR;
    }
}
