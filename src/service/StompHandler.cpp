//
// Created by 박동빈 on 2026. 10. 6..
//
#include "../include/StompHandler.h"

#include "../../constants/Util.h"
#include "../../include/Session.h"
#include "../../include/StompFrame.h"
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
    const std::string& req, const std::shared_ptr<bool>& is_disconnected_ptr
)
{
    // STOMP 프레임을 만든다.
    StompFrame stomp_frame;
    stomp_frame.parse(req);

    if (!stomp_frame.isValid())
    {
        logger->warning("Invalid STOMP frame!");
        return C::HEART_BEAT_RESULT;
    }

    if (req.back() != C::SINGLE_BACK_SLASH_CHAR)
    {
        // heart beat 요청이 아닌 경우 출력한다.
        logger->info3(">>> req from client :");
        logger->info3("\n" + req);
    }

    std::string command = stomp_frame.getCommand();
    if (const auto ptr_for_parent_session = parent_session_ptr.lock())
    {
        if (command == "CONNECT")
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
        else if (command == "SUBSCRIBE")
        {
            // TODO : implement later
            return C::HEART_BEAT_STR;
        }
        else if (command == "UNSUBSCRIBE")
        {
            // TODO : implement later
            return C::HEART_BEAT_STR;
        }
        else if (command == "SEND")
        {
            // TODO : implement later
            return C::HEART_BEAT_STR;
        }
        /*  서버는 SEND로 받은 메시지를 타킷 클라이언트들한테 전송할 때, MESSAGE 메서드를 사용한다.
         *  클라이언트가 MESSAGE 메서드로 요청을 하는 경우는 없다.
         *else if (method == "MESSAGE") { return C::HEART_BEAT_STR; }*/
        else if (command == "DISCONNECT")
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
        else if (command == C::HEART_BEAT_CMD)
        {
            return C::HEART_BEAT_RESULT;
        }
        else
        {
            logger->warning("Not supporing command! : " + command);
            return C::HEART_BEAT_RESULT;
        }
    }
    else
    {
        logger->severe("Parent Session is Null!");
        return C::HEART_BEAT_STR;
    }
}
