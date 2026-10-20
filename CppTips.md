# C++ Tips
<br><br/>
1. 특수한 경우가 아니라면, C/C++ 프로젝트에서는 .h 선언과 .cpp(또는 .c) 구현을 분리하자.
- 특수한 경우란, 본 프로젝트의 RedisService.h 를 뜻한다. 이 경우는 다음 2. 번 팁에서 설명한다.
- 선언과 구현을 분리하지 않고, .h 에다가 모든 구현을 다 집어넣을 경우 아래와 같은 문제가 발생한다.
  - 순환 #include 문제가 발생했을 때 보통 forward declaration 으로 문제를 해결하는데, .h 에 모든 구현이 들어 있을 경우 이 문제를 해결할 수 없다.
  - Server.h 내의 멤버 함수를 Session.h 내에서 호출하려고 하면 자꾸 'incomplete...' 에러가 떴던 것이다.
```text
'순환 #include 문제'란 다음과 같다.
- Server 안에는 Session 객체가 저장돼 있다고 하자.
- Session이 Server의 멤버 함수를 호출해야 할 수도 있다.
- 이때, Server 가 Session을 인클루드하고, Session은 Server를 인클루드하는 '순환'이 발생한다.
- 순환을 깨기 위해서는 Session.h 안에서 class Server; 라고 forward declaration 을 해야 한다.
- 그런데, Server 가 .cpp 없이 .h 만 있을 경우 참조를 이용한 생성자 + 초기화로는 Server.h 의 멤버함수를 정상적으로 인식하지 못하는 에러가 발생한다.
- Session 입장에서 전방 선언된 Server와 Server.h 를 헷갈려하는 것이다. #include "../include/Server.h"를 명시해줘도 인식하지 못한다.

- 이 문제를 해결하기 위해서는 결국 두 가지 중 하나를 선택해야 한다.
1. .h 에 모든 구현을 넣고, 멤버 객체는 무조건 std::shared_ptr 또는 std::weak_ptr 로 정의하는 것.
2. .h 와 .cpp 를 분리하고, forward declaration을 사용하는 것.

- 1번 방법은 객체의 생성 후 별도의 set 함수롤 통해 멤버 포인터를 정의해야 하며, 객체가 커질 경우 이 과정을 까먹을 우려가 있다.
- 2번 방법은 번거롭지만, C/C++ 의 역사상 가장 오랜 기간 사용 & 테스트 됐으며 객체를 명확하게 초기화 할 수 있다는 장점이 있다.
- 상항이 복잡할 때는 이 두 가지 방법을 모두 사용해야 할 수도 있다. 
```
<br><br/>
2. boost::redis 의 특수성 - 특정 헤더는 중복 선언하면 안 된다.
- boost/redis 관련 두 가지 include 는 프로젝트 전체에서 오직 1 번씩만 정의돼야 한다.
- 그렇지 않을 경우(ex : boost::redis::connection 객체를 다른 곳에서 만들고 생성자에 참조 전달 하는 경우) 'duplicate symbol' 관련 에러가 뜨면서 빌드에 실패한다.
- 따라서, RedisService 같은 레디스 전용 클래스를 만들고, 그 안에서만 redis connection을 만들어서 사용하는 구조가 권장된다.
```text
'duplicated symbol' 관련 에러를 유발한 boost::redis 관련 헤더는 다음과 같다.
이 헤더들은 프로젝트 전체에 걸쳐서 단 1 번만 선언 돼야 한다.

#include <boost/redis.hpp>
#include <boost/redis/src.hpp>
```
<br><br/>
3. boost::redis 라이브러리를 대하는 관점
```markdown
- 별별 방법을 다 써 보며 boost::redis pub/sub 리스너를 만들고 테스트 해본 결과 내가 내린 결론은 아래와 같다.

 I. 라이브러리 예제에서 하라는 대로 하자.
       C++20 코루틴을 활용한 redis pub/sub listener에댜가 multi-thread run을 돌리는 io_context
       를 넣었더니 mac에서는 알 수 없는 이유로 PONG timeout 에러가 뜨면서 리스너가 멈춰버렸던 것이다.
       원래 예제에서 하던대로 main_io_context 와는 별개의 io_context를 넣어주니 정상 동작했다.
       그리고 boost redis 라이브러리에서는 매 연결마다 커넥션을 만들어서 쓴 다음 버리고 있었다.
       이것 또한 그대로 적용했다.
 
 II. sync blocking으로 처리할 수 있으면 최대한 sync blocking 으로 처리하자.
       SIGABRT 등의 에러를 잡기 위해서 별 방법들을 다 동원해봤지만, 결국 sync blocking으로 실행하는게
       가장 쉽고 확실한 문제 해결 방법이었다.
       async 는 꼭 필요할 때만 써야한다.
       남용할 경우 알 수 없는 에러들이 튀어 나왔으며 디버깅 난이도도 높았다.
       대략적으로는 '1회성 실행 : sync/blocking' / '이벤트 대기 루프 : async' 의 느낌이다.
 ```
<br><br/>
4. C++ 코루틴을 대하는 관점
- 본 프로젝트의 RedisService.h 에서는 Redis Pub/Sub 리스너의 구현에 C++ 코루틴이 사용되었다.
- 정확히는, boost::redis에서 제공한 예제를 거의 그대로 복사/붙여넣기 한 것이다.
  - https://github.com/boostorg/redis/blob/master/example/cpp20_subscriber.cpp
- C++ 코루틴은 매우 편리하지만, 내부 작동 원리를 전부 이해하고 정확하게 코딩하는 것은 매우 어려우므로 웬만하면 직접 구현하기보다는 코루틴 구현 라이브러리(boost 또는 C++ 23 이상의 std lib 같은)를 적극 활용하는 것이 좋다고 본다.
- 아래는 이번 프로젝트에서 코루틴을 처음 접한 후 배운 점을 정리한 주석이다.
```c++
/*
 * ============================================================================
 * C++20 Coroutine 학습 정리
 * ============================================================================
 * [1. 코루틴이란?]
 * 코루틴은
 *   "실행 중인 함수의 상태를 저장한 채 실행을 잠시 중단(suspend)하고,
 *    나중에 그 상태에서 다시 실행(resume)할 수 있는 실행 단위"
 * 라고 우선 이해하면 된다.
 *
 * 일반적인 함수는 아래의 flow로 실행된다 :
 *   함수 실행 -> return -> 함수 종료
 *
 * 하지만 코루틴의 flow는 다음과 같다 :
 *   실행 -> suspend -> 상태 저장 -> resume -> 다시 실행 -> 다시 suspend ...
 *   필요하면 co_return으로 최종 종료
 *
 * ============================================================================
 * [2. 처음에는 "코루틴 실행 엔진"을 가정하자]
 * 코루틴 내부 구현을 처음부터 모두 이해하려고 하면 너무 복잡하다.
 * 따라서 처음에는 다음과 같은 "코루틴 실행 엔진"이 있다고 가정한다.
 *                  Coroutine Engine
 *                        │
 *              ┌─────────┴─────────┐
 *              │                   │
 *         Coroutine A         Coroutine B
 *              │                   │
 *            실행                 실행
 *              ↓                   ↓
 *           suspend             suspend
 *              ↓                   ↓
 *           resume              resume
 *              ↓                   ↓
 *            실행                 실행
 *
 * 이 엔진이 내부적으로 어떻게 구현되어 있는지는 일단 신경 쓰지 않는다.
 * 실제 C++에서는:
 *   - coroutine frame
 *   - promise_type
 *   - coroutine_handle
 *   - suspend_always / suspend_never
 *   - awaiter / awaitable
 *   - scheduler
 * 등의 개념이 등장하지만,
 * "코루틴이 어떻게 동작하는가?"를 처음 이해하는 단계에서는
 * 이것들을 모두 알아야 할 필요는 없다. 이것들은 나중에 코루틴의 내부 동작이 궁금해졌을 때 공부한다.
 *
 * ============================================================================
 * [3. 가장 먼저 알아야 할 키워드]
 * C++20 coroutine에서 우선 다음 키워드의 의미를 익힌다.
 *   co_await
 *   └─ 현재 코루틴의 실행을 잠시 중단하고,
 *      기다리던 작업이 완료되면 다시 resume한다.
 *
 *   co_return
 *   └─ 코루틴의 실행을 종료한다.
 *
 *   co_yield
 *   └─ 값을 하나 외부로 전달하고 코루틴을 잠시 중단한다.
 *      다음 resume 때 그 다음 지점부터 실행할 수 있다.
 *
 * 그리고 Boost.Asio에서는:
 *   co_spawn()
 *   └─ 코루틴을 Asio의 실행 시스템에 등록하여 실행시키는 역할을 한다.
 *
 * ============================================================================
 * [4. co_await를 이해하는 것이 가장 중요하다]
 * 예:
 *   boost::asio::awaitable<void> foo()
 *   {
 *       std::cout << "A\n";
 *       auto [ec] = co_await socket.async_read_some(...);
 *       std::cout << "B\n";
 *   }
 *
 * 처음에는 다음과 같이 생각한다.
 *
 *   foo()
 *     │
 *     ▼
 *   "A" 출력
 *     │
 *     ▼
 *   co_await
 *     │
 *     ├─ 현재 coroutine의 상태를 저장
 *     ├─ coroutine suspend
 *     │
 *     │   다른 작업 수행 가능
 *     │
 *     ▼
 *   I/O 완료
 *     │
 *     ▼
 *   coroutine resume
 *     │
 *     ▼
 *   "B" 출력
 *     │
 *     ▼
 *   co_return
 *
 * 따라서 co_await는 단순히
 *   "기다린다"
 * 라고만 생각하면 부족하다.
 *
 * 더 정확하게는:
 *   "현재 coroutine의 실행을 잠시 중단하고,
 *    기다리던 작업이 완료되면 다시 이어서 실행한다."
 *
 * ============================================================================
 * [5. Coroutine과 Thread는 서로 다른 개념이다]
 * 이것은 반드시 구분해야 한다.
 * 일반적인 blocking:
 *   Thread
 *     │
 *     ├── read()
 *     │
 *     │   ─────────────── 대기 ───────────────
 *     │
 *     ▼
 *   다음 코드 실행
 * 이 경우 thread 자체가 blocking된다.
 *
 *
 * 반면 coroutine + asynchronous operation:
 *   Thread
 *     │
 *     ├── Coroutine 실행
 *     │
 *     ├── co_await
 *     │
 *     └── Coroutine suspend
 *             │
 *             │
 *             │  Thread는 다른 작업 수행 가능
 *             │
 *             ▼
 *         I/O 완료
 *             │
 *             ▼
 *         Coroutine resume
 *             │
 *             ▼
 *         다음 코드 실행
 * 즉,
 *   coroutine suspend != thread sleep
 *   coroutine suspend != thread blocking
 *
 * Coroutine은 함수의 실행 상태를 잠시 멈추는 것이고,
 * OS thread가 반드시 멈추는 것은 아니다.
 *
 * ============================================================================
 * [6. Boost.Asio에서 coroutine은 어떻게 실행되는가?]
 * Boost.Asio에서는 다음과 같은 구조로 생각하면 편하다.
 *
 *
 *             C++ Coroutine
 *                   │
 *                   │ co_spawn()
 *                   ▼
 *             Asio 실행 시스템
 *                   │
 *                   ▼
 *              io_context
 *                   │
 *          ┌────────┼────────┐
 *          │        │        │
 *        thread   thread   thread
 *          │        │        │
 *          └────────┼────────┘
 *                   │
 *                   ▼
 *              coroutine 실행
 * 중요한 점:
 *   co_spawn()
 * 은 일반적으로
 *   "새로운 OS thread를 생성한다"
 * 는 의미가 아니다.
 * 이미 존재하는 io_context와 executor/scheduler를 통해
 * coroutine의 실행이 이루어진다고 이해하는 것이 좋다.
 *
 * ============================================================================
 * [7. Coroutine을 이해하는 가장 중요한 반복 패턴]
 *             ┌──────────────┐
 *             │              │
 *             ▼              │
 *          coroutine         │
 *            실행             │
 *             │              │
 *             ▼              │
 *          co_await          │
 *             │              │
 *             ▼              │
 *          suspend           │
 *             │              │
 *             │ 기다리던       │
 *             │ 작업 완료      │
 *             ▼              │
 *           resume ──────────┘
 * 이 "실행 → suspend → resume → 실행"이라는 반복 구조를
 * 먼저 이해하는 것이 중요하다.
 *
 * ============================================================================
 * [8. Boost.Redis에서의 실제 예]
 * 예:
 *   auto [ec] =
 *       co_await conn->async_receive2(boost::asio::as_tuple);
 * 이것을 처음에는 다음과 같이 읽으면 된다.
 *
 *   Redis 메시지를 기다린다.
 *           │
 *           ▼
 *   현재 coroutine suspend
 *           │ 다른 coroutine / I/O 작업 가능
 *           ▼
 *   Redis 메시지 도착
 *           │
 *           ▼
 *   coroutine resume
 *           │
 *           ▼
 *   다음 코드 실행
 *
 * 따라서:
 *   "Redis 메시지가 올 때까지 thread가 멈춘다."
 * 가 아니라,
 *   "Redis 메시지가 올 때까지 현재 coroutine의 실행을 중단하고,
 *    메시지가 도착하면 해당 coroutine을 다시 실행한다."
 * 라고 이해해야 한다.
 *
 * ============================================================================
 * [9. Coroutine의 내부 구현은 나중에 공부한다]
 * 처음부터 다음 개념들을 전부 이해하려고 하지 않는다.
 *   coroutine frame
 *   promise_type
 *   coroutine_handle
 *   awaiter
 *   awaitable
 *   await_transform
 *   completion token
 *   executor
 *   scheduler
 * 처음에는:
 *   "이런 것들이 coroutine을 실제로 동작시키는 내부 장치다."
 * 정도만 알고 넘어간다.
 * 먼저 coroutine의 사용 방법과 실행 흐름을 이해한 뒤,
 * 필요할 때 하나씩 내부 구현으로 내려간다.
 *
 * ============================================================================
 * [10. Coroutine 학습 순서]
 * 추천 순서:
 *   1.
 *   Coroutine은 suspend/resume 가능한 실행 단위라는 것을 이해한다.
 *        ↓
 *   2.
 *   co_await / co_yield / co_return의 의미를 익힌다.
 *        ↓
 *   3.
 *   "실행 → suspend → resume → 실행" 패턴을 이해한다.
 *        ↓
 *   4.
 *   co_spawn이 coroutine을 실행 시스템에 등록하는 역할임을 이해한다.
 *        ↓
 *   5.
 *   Coroutine과 Thread가 서로 다른 개념이라는 것을 확실하게 이해한다.
 *        ↓
 *   6.
 *   coroutine frame / promise / coroutine_handle 등을 공부한다.
 *        ↓
 *   7.
 *   Boost.Asio가 coroutine과 io_context/scheduler를
 *   어떻게 연결하는지 공부한다.
 *
 * ============================================================================
 * [11. 최종적으로 기억할 한 문장]
 * Coroutine은
 *   "실행 상태를 저장한 채 잠시 멈췄다가,
 *    나중에 그 지점에서 다시 실행할 수 있는 실행 단위"
 * 이다.
 * 그리고 co_await는 그 coroutine을 suspend시키는 대표적인 지점이다.
 *
 * Coroutine 자체와 Thread는 다른 개념이며,
 * Boost.Asio에서는 io_context/executor/scheduler가
 * coroutine을 실제 실행 가능한 형태로 관리한다.
 *
 * 처음에는 이 실행 엔진의 모든 내부 구현을 이해하려 하지 말고,
 *   "실행 → co_await → suspend → resume → 실행"
 * 이 흐름을 머릿속에 확실히 만드는 것이 우선이다.
 * ============================================================================
 */
```
<br><br/>
5. 컨테이너 SIGABRT 문제
- 이 문제는 특정 컨테이너(std::vector, std::array, std::unordered_map, ...)를 순회하면서 동시에 같은 컨테이너 내부의 요소를 변경(특히 삭제)할 때 발생한다.
- 보통 이런 에러가 뜰 경우, Java 라면 관련 예외를 던지지만, C/C++ 에서는 예외를 던지지 못한채 SIGABRT 를 띄우고 프로그램이 곧장 죽어 버리기 때문에 발견이 쉽지 않다. 
- 해결책은 2 가지가 있다.
  - 원본 컨테이너의 deep copy 복사본 컨테이너를 순회하면서 변경 대상 요소 발견시 원본 컨테이너에서 수정(삭제 등)
  - 순회 루프와 삭제 동작을 완전 분리 -> 순회 루프 종료 후 컨테이너 변경 동작을 실행하는 것
```c++
void Server::start(){
...
    // heart-beat 타이머
    heart_beat_task.setTask(
        [&]()
        {
            // 가장 최근 heart beat 받은 시각이 '임계값'보다 과거이면 그 세션은 제거한다.
            // 단, 원본 맵은 참조하지 않고, 복사본 맵으로 본다.
            // 세션을 복사하는게 아니라 포인터를 복사하는 것이므로 deep copy 해도 상관 없다.
            // 여기서 원본 맵으로 순회하면 동일한 맵을 순회하면서 동시에 수정하는 data race 가 발생한다.
            // 그러면 SIGSEGV, abort() 등이 뜨면서 서버가 예외도 던지지 못하고 바로 죽는다.
            auto copied_session_map = session_id_map;
            int alive_session_count = 0;
            for (const auto& [session_id, session_ptr] : copied_session_map)
            {
                // 가장 최근에 해당 세션의 클라이언트에게서 온 heart beat 시각과 현재 시각을 대조한다.
                const int64_t beat_arrive_time_millis = Util::getCurrentTimeMillis();
                const int64_t latest_heart_beat_time_millis
                    = session_ptr->getLatestHeartBeatTimeMillis();
                if (
                    const int64_t diff = beat_arrive_time_millis - latest_heart_beat_time_millis;
                    diff > static_cast<int64_t>(C::HEART_BEAT_THRESHOLD_MS)
                )
                {
                    session_ptr->setShutdownTrue();
                    continue;
                }
                if (session_ptr->isShutDown() == false)
                {
                    alive_session_count++;
                    session_ptr->sendHeartBeatToClient();
                }
            } //for
            logger->severe(
                "Dongvin, sent hear-beat to alive sessions. cnt : "
                + std::to_string(alive_session_count)
            );
        }
    );
    heart_beat_task.start();
...
}
```
<br><br/>
6. std::erase(컨테이너, 삭제 대상 요소) 사용법.
- 본 프로젝트에서는 TopicInfo 를 std::vector<TopicInfo> info_vec; 에서 삭제할 때 문제가 발생했었다.
  - 처음 구현에서는 std::erase(Container, Element)를 사용했었다.  
  - 그런데 처음 구현이 Windows 11 MSVC 에서는 빌드/실행 됐지만 macOS(M1)에서는 빌드가 안 됐다.
- 이게 의도한대로 작동하기 위해서는 삭제 대상 요소의 클래스에서 '==' 연산자가 오버로딩 돼 있어야 한다.
- 혹은 아래와 같이 반복자를 활용해서 명시적으로 삭제를 하는 방법도 있다.
```c++
// 아래의 코드는 MSVC 에서는 작동하지만, macOS(M1)에서는 빌드에 실패한다.
// 컴파일러 구현체별로 std::erase 함수의 동작이 다르게 정의돼 있어서 발생한 문제이다.
// 본 프로젝트에서는 '확실히 동작하게' 만들기 위해서 반복자를 이용해서 직접 제거하는 방식으로 구현했다.
void Session::unsubscribeTopic(const std::string& id_str)
{
  TopicInfo topic_info;
  for (const auto& info : topic_info_vec)
  {
      if (info.id_str == id_str)
      {
          topic_info.topic = info.topic;
          topic_info.id_str = info.id_str;
          topic_info.session_id = session_id;
          topic_info.idx = info.idx;
          break;
      }
  }
  std::erase(topic_info_vec, topic_info);
  
  auto self = shared_from_this();
  msg_broker_ptr->unsubscribe(self, topic_info);
 }
 
 
 // 이 코드는 동일한 동작을 반복자를 활용해서 처리한다.
 // == 연산자의 오버로딩이 필요 없다는 장점이 있다.
 void Session::unsubscribeTopic(const std::string& id_str)
{
    // 제거 대상 요소를 가리키는 반복자를 찾아낸다.
    const auto it = std::ranges::find_if(
        topic_info_vec,
        [&id_str](const TopicInfo& info){ return info.id_str == id_str; }
    );

    if (it == topic_info_vec.end()) { return; }

    // 반복자로 얻어낸 제거 대상 요소를 std::erase_if()를 써서 실제로 삭제한다.
    const TopicInfo& topic_info = *it;
    std::erase_if(
        topic_info_vec,
        [&id_str](const TopicInfo& info){ return info.id_str == id_str; }
    );

    // MsgBroker에서도 삭제한다.
    auto self = shared_from_this();
    msg_broker_ptr->unsubscribe(self, topic_info);
}
```
<br><br/>
7. Boost beast 라이브러리를 활용해서 STOMP 커넥션 만들기
- 중요한 것은 웬만해서는 '내 생각대로 하는 것'보다는 '라이브러리에서 권장하는 예제'를 따라하는게 좋다는 점이다.
- 또한 boost::beast 라이브러리는 web-socket 연결은 지원하지만, STOMP 프로토콜 까지 구현돼 있지는 않다.
  - 결과적으로, web socket 연결 후 STOMP 프로토콜로 설정 및 동작하게 만드는 것은 개발자의 몫이다.
- 아래는 내가 겪은 과정을 주석 형태로 정리한 것이다. 
```c++
/*
 * ============================================================================
 * C++ STOMP Server - WebSocket / STOMP handshake 복습 노트
 * ============================================================================
 *
 * ============================================================================
 * STOMP 커넥션 생성 과정 요약
 * ============================================================================
 *   TCP
 *    │
 *    ▼
 *   HTTP Upgrade Request
 *    │  GET /gs-guide-websocket
 *    │  Upgrade: websocket
 *    │  Sec-WebSocket-Protocol: v12.stomp, v11.stomp, v10.stomp
 *    ▼
 *   WebSocket Handshake
 *    │  HTTP 101 Switching Protocols
 *    │  Sec-WebSocket-Protocol: v12.stomp
 *    ▼
 *   WebSocket OPEN
 *    │  STOMP CONNECT
 *    ▼
 *   C++ STOMP Server
 *    │  STOMP CONNECTED
 *    ▼
 *   app.js onConnect()
 *
 * ============================================================================
 * 1. WebSocket과 STOMP는 서로 다른 계층이다.
 * ============================================================================
 * WebSocket:
 *   HTTP Upgrade
 *        ↓
 *   101 Switching Protocols
 *        ↓
 *   WebSocket connection
 *
 * STOMP:
 *   WebSocket connection이 만들어진 이후
 *        ↓
 *   CONNECT
 *        ↓
 *   CONNECTED
 *        ↓
 *   SUBSCRIBE / SEND / MESSAGE ...
 *
 * 따라서 HTTP 101이 성공했다고 해서
 * app.js의 onConnect()가 호출되는 것은 아니다.
 *
 * ============================================================================
 * 2. 처음에는 WebSocket handshake까지만 성공했다.
 * ============================================================================
 * 처음 C++ 서버에서 확인한 것:
 *
 *   HTTP/1.1 101 Switching Protocols
 *   Upgrade: websocket
 *   Connection: Upgrade
 *   Sec-WebSocket-Accept: ...
 *   Sec-WebSocket-Protocol: v12.stomp
 *
 * 여기까지 성공했다는 것은:
 *
 *   - TCP 연결 성공
 *   - HTTP Upgrade request 수신 성공
 *   - WebSocket handshake 성공
 *   - STOMP WebSocket sub-protocol 협상 성공
 * 을 의미한다.
 * 하지만 이 시점에서는 아직 STOMP CONNECT가 처리되지 않았다.
 *
 * ============================================================================
 * 3. async_accept(req)가 왜 실패했는가?
 * ============================================================================
 * 처음에는 다음과 비슷하게 생각했다.
 *
 *   websocket.async_accept(req, handler);
 *
 * "서버가 HTTP Upgrade request를 받아서 req에 넣어주겠지?"
 *
 * 하지만 Beast의 해당 overload에서 req는
 * "이미 읽어놓은 HTTP request"이다.
 *
 * 즉:
 *   async_accept(req)
 * 는
 *
 *   HTTP request를 읽는다
 * 가 아니라,
 *   이미 읽은 HTTP request를 가지고
 *   WebSocket handshake를 수행한다.
 *
 * 그래서 빈 request를 넘기면:
 *   The WebSocket handshake method was not GET
 * 같은 에러가 발생했다.
 *
 * ============================================================================
 * 4. 직접 HTTP request를 읽는 방식으로 변경
 * ============================================================================
 * 먼저 raw TCP socket에서 HTTP request를 읽는다.
 *   boost::beast::http::read(
 *       *raw_tcp_socket_ptr,
 *       buffer,
 *       req
 *   );
 *
 * 이제 req에서:
 *   - HTTP method
 *   - URI
 *   - Upgrade
 *   - Sec-WebSocket-Protocol
 *   - Sec-WebSocket-Key
 *   - 기타 header
 *
 * 등을 확인할 수 있다.
 * 그 다음:
 *   websocket::stream
 * 을 생성하고:
 *
 *   web_socket_ptr->accept(req);
 * 로 WebSocket handshake를 완료한다.
 *
 * 즉 수동 handshake의 개념은:
 *
 *   TCP socket
 *      ↓
 *   HTTP read
 *      ↓
 *   request 검사
 *      ↓
 *   WebSocket stream 생성
 *      ↓
 *   accept(req)
 *      ↓
 *   WebSocket connection
 *
 * ============================================================================
 * 5. Sec-WebSocket-Protocol은 WebSocket handshake에서 결정한다.
 * ============================================================================
 * app.js가 보낸 request:
 *   Sec-WebSocket-Protocol:
 *       v12.stomp, v11.stomp, v10.stomp
 *
 * C++ 서버는:
 *   v12.stomp
 *
 * 를 지원하도록 했다.
 * 따라서:
 *
 *   Client: v12.stomp, v11.stomp, v10.stomp
 *   Server: v12.stomp
 *
 * 가 되고,
 *
 * HTTP 101 response에는:
 *   Sec-WebSocket-Protocol: v12.stomp
 * 가 들어간다.
 *
 * 이것은 "STOMP CONNECTED"가 아니다.
 *
 * 단지 WebSocket handshake 단계에서
 * "이 WebSocket connection에서 사용할 sub-protocol은
 *  v12.stomp로 하자."
 *
 * 라고 협상한 것이다.
 *
 * ============================================================================
 * 6. WebSocket handshake가 끝난 뒤 STOMP CONNECT가 들어온다.
 * ============================================================================
 * handshake 이후 async_read()를 시작하자
 * 실제로 다음 데이터가 들어왔다.
 *
 *   CONNECT
 *   accept-version:1.2,1.1,1.0
 *   heart-beat:10000,10000
 *
 *   NUL
 *
 * 여기서 중요한 점:
 *   WebSocket frame 안에 STOMP frame이 들어있다.
 *
 * 즉:
 *   WebSocket
 *       ↓
 *   STOMP
 * 관계이다.
 *
 * ============================================================================
 * 7. 왜 처음에는 app.js onConnect()가 호출되지 않았는가?
 * ============================================================================
 * Client:
 *   CONNECT
 *       ↓
 *   Server
 *
 * 까지는 됐다.
 * 하지만 서버가 아무 응답도 하지 않았다.
 *
 * app.js의:
 *   stompClient.onConnect = ...
 *
 * 는 WebSocket connection이 열렸을 때 실행되는 callback이 아니다.
 *
 * STOMP CONNECT를 보낸 후 서버로부터:
 *   CONNECTED
 * frame을 받아야 실행된다.
 *
 * 따라서:
 *   WebSocket OPEN
 *       ↓
 *   STOMP CONNECT
 *       ↓
 *   C++ Server
 *       ↓
 *   STOMP CONNECTED
 *       ↓
 *   app.js onConnect()
 *
 * 순서가 된다.
 *
 * ============================================================================
 * 8. 야매 CONNECTED 응답으로 실제 동작을 확인했다.
 * ============================================================================
 * 일단 STOMP parser를 만들기 전에
 * Session에서 직접 CONNECTED frame을 만들어서 테스트했다.
 *
 *   CONNECTED
 *   version:1.2
 *   heart-beat:10000,10000
 *
 *   NUL
 *
 * 이것을 WebSocket async_write()로 전송하자
 * app.js의 onConnect()가 실제로 호출되었다.
 *
 * 브라우저 콘솔:
 *   Connected: CONNECTED
 *   heart-beat:10000,10000
 *   version:1.2
 *   content-length:0
 *
 * 따라서 다음 사실을 실제로 검증했다.
 *
 *   "C++ Beast WebSocket → STOMP frame → app.js"
 * 전체 경로가 정상적으로 연결되어 있다.
 *
 * ============================================================================
 * 9. STOMP frame의 끝에는 NUL byte가 필요하다.
 * ============================================================================
 * STOMP frame은 대략 다음 구조다.
 * 
 *   COMMAND
 *   header:value
 *   header:value
 *
 *   body
 *   NUL
 * 즉 frame 마지막의 '\0'이 STOMP frame terminator이다.
 *
 * 브라우저 네트워크 메시지에서:
 *   ^@
 * 로 보인 것은 NUL byte를 의미한다.
 *
 * ============================================================================
 * 10. C++ std::string과 NUL byte에서 주의할 점
 * ============================================================================
 * 다음 코드는 주의해야 한다.
 *
 *   std::string(
 *       "CONNECTED\n"
 *       "version:1.2\n"
 *       "\0"
 *   );
 *
 * const char*를 받는 std::string 생성자는
 * C-style string의 첫 번째 NUL을 문자열 끝으로 판단한다.
 *
 * 따라서 STOMP frame의 NUL을 확실하게 포함시키려면:
 *
 *   auto frame = std::make_shared<std::string>(
 *       "CONNECTED\n"
 *       "version:1.2\n"
 *   );
 *   frame->push_back('\0');
 * 처럼 명시적으로 넣는 것이 이해하기 쉽다.
 *
 *
 * ============================================================================
 * 11. async_write에서 buffer의 lifetime도 중요하다.
 * ============================================================================
 * async_write()는 호출하는 순간 데이터를 전부 복사하는 것이 아니다.
 * 비동기 operation이 완료될 때까지
 * buffer가 가리키는 메모리가 살아 있어야 한다.
 *
 * 그래서 테스트 코드에서는:
 *   auto frame = std::make_shared<std::string>(...);
 *
 *   websocket.async_write(
 *       asio::buffer(*frame),
 *       [frame](...)
 *       {
 *           ...
 *       }
 *   );
 * 처럼 callback에서 frame을 capture하여
 * write가 끝날 때까지 lifetime을 유지했다.
 *
 * ============================================================================
 * 12. 현재 Session의 역할
 * ============================================================================
 * 현재 Session은 우선 WebSocket transport 역할을 한다.
 *   Session
 *      │
 *      ├── TCP socket
 *      │
 *      ├── HTTP Upgrade
 *      │
 *      ├── WebSocket handshake
 *      │
 *      ├── WebSocket async_read
 *      │
 *      └── WebSocket async_write
 *             │
 *             ▼
 *        StompHandler
 * 이후에는 STOMP protocol 처리를 StompHandler로 넘기는 것이 좋다.
 *
 * ============================================================================
 * 13. 앞으로 구현할 구조
 * ============================================================================
 *   Session
 *      │
 *      │ WebSocket message
 *      ▼
 *   StompHandler
 *      │
 *      ├── CONNECT
 *      │      └── CONNECTED
 *      │
 *      ├── SUBSCRIBE
 *      │      └── subscription 등록
 *      │
 *      ├── UNSUBSCRIBE
 *      │
 *      ├── SEND
 *      │      └── MsgBroker / Redis
 *      │
 *      └── DISCONNECT
 *      ...
 *
 * ============================================================================
 * 14. WebSocket read/write 동시성
 * ============================================================================
 * Beast WebSocket에서는 일반적으로:
 *
 *   async_read  1개
 *   async_write 1개
 *
 * 를 동시에 수행할 수 있다.
 * 하지만 여러 곳에서 동시에 async_write()를 호출하면 안 된다.
 * 예:
 *
 *   Redis message
 *       ↓
 *   Session A → write
 *
 *   다른 이벤트
 *       ↓
 *   Session A → 또 다른 write
 *
 * 이런 식으로 write가 동시에 발생할 수 있기 때문에
 * 실제 서버에서는 write queue를 두고 직렬화하는 구조가 필요하다.
 *
 * 현재 Session에 있는 strand는
 * 이런 session-level serialization을 구현할 때 활용할 수 있다.
 *
 * ============================================================================
 * 15. 현재 구현의 한계
 * ============================================================================
 * 현재 start()에서는:
 *
 *   http::read()
 *   websocket.accept()
 *
 * 를 synchronous 방식으로 수행한다.
 *
 * 따라서 클라이언트가 TCP 연결만 맺고
 * HTTP request를 보내지 않는다면
 * 해당 작업이 blocking될 수 있다.
 *
 * 또한 Server의 accept loop도 synchronous accept 방식이다.
 *
 * 현재는 학습 / 테스트용으로 충분하지만
 * 실제 서버 구조에서는 HTTP handshake까지
 * asynchronous하게 처리하는 방향을 고려할 수 있다.
 *
 * ============================================================================
 * 최종적으로 기억할 것
 * ============================================================================
 *   HTTP 101
 *       =
 *   WebSocket handshake 성공
 *
 *   CONNECT
 *       =
 *   STOMP client → server 요청
 *
 *   CONNECTED
 *       =
 *   STOMP server → client 응답
 *
 *   onConnect()
 *       =
 *   app.js가 CONNECTED를 정상적으로 받은 뒤 실행
 *
 * 즉:
 *   "WebSocket 연결 성공" != "STOMP 연결 성공"
 * 이 차이를 이해하는 것이 오늘 구현에서 가장 중요한 내용이었다.
 *
 * ============================================================================
 */
```
<br><br/>
8. Boost asio 라이브러리의 raw TCP 소켓과 Boost beast 라이브러리의 web socket 을 다루는 관점
- 결론은 RAII 에 따라서 리소스 정리를 할 때, 라이브러리 개발자에게 맡기라는 것이다.
  - 내가 억지로 Session의 소멸자에서 소켓을 close() 시키자, 실제 세션 삭제 시 SIGABRT, SIGSEGV 에러들이 터져 나왔다.
  - 여러 가지 방법을 시도했지만, Session 셧다운 시 소켓을 명시적으로 close 하지 않고 단지 새로운 async read/async write만 하지 않게 만드는 것이 문제를 해결해 줬다.
  - async read/async write 동작이 많은 boost::asio 구현은 타이밍 문제에 따른 'life time & memory access' 문제에 취약하다.
  - 이 문제를 내가 직접 해결하기 보다는 'boost 같이 전세계적으로 널리 사용되는 C++ 라이브러리에서 이 문제를 해결하지 않았을 리 없다'라는 생각으로 그냥 라이브러리에게 맡기는 것이 결과적으로 옳은 선택이었다.
```markdown
- 본 프로젝트의 Session 은 클라이언트 1 명 당 1 개씩 할당 된다.
  - STOMP 커넥션 설정이 성공하면, raw TCP 소캣 1 개와 boost beast 웹소켓 1 개가 초기화 된다.
  - Session 셧다운 시 Session의 소멸자에 의해서 소켓들의 소멸자가 호출되고, 소켓들의 소멸자가 리소스 회수를 완료시킨다.
```
<br><br/>
9. std::unordered_map의 [ ] 연산자의 기본 동작
- 결론은 마치 JAVA HashMap의 putIfAbsent() 처럼 작동한다는 것이다.
- 복잡한 if-else{...}; 구현과 map[...][...]; 구현이 동일한 결과를 보여준다.
```c++
// 첫째는 if-else 문으로 명시적으로 구현한 것이다. 작동은 하지만 코드량이 많다.
void MsgBroker::subscribe(
  const std::string& topic,
  const std::string& id_str,
  std::shared_ptr<Session> session_ptr
)
{
  // boost asio strand를 써서 data race를 방지한다.
  auto self = shared_from_this();
  boost::asio::post(
    self->strand,
    [self, topic, id_str, session_ptr]()
    {
      // 인덱스 계산.
      ++self->topic_sub_cnt;
      int idx = self->topic_sub_cnt % C::TOPIC_INDEX_SIZE;
      if (idx == 0) { self->topic_sub_cnt = 0; }

      // 이거 한 줄이면 아래의 복잡한 if-else를 전부 대체할 수 있다.
      // std::unordered_map 의 기본 생성 동작이 JAVA의 put-if-absent 처럼 작동하기 때문이다.
      //self->printMap(); // 개발 & 체크용.
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
      }
      session_ptr->addTopicInfo(topic, id_str, idx);
    }
  );
}

//---------------------------------------

// 두 번째는 std::unordered_map 의 기본 연산자를 활용한 구현이다.
// 복잡한 if-else 들이 단 한 줄의 map[...][...]; 코드로 대체된다.
void MsgBroker::subscribe(
  const std::string& topic,
  const std::string& id_str,
  std::shared_ptr<Session> session_ptr
)
{
  // boost asio strand를 써서 data race를 방지한다.
  auto self = shared_from_this();
  boost::asio::post(
    self->strand,
    [self, topic, id_str, session_ptr]()
    {
      // 인덱스 계산.
      ++self->topic_sub_cnt;
      int idx = self->topic_sub_cnt % C::TOPIC_INDEX_SIZE;
      if (idx == 0) { self->topic_sub_cnt = 0; }

      // 이거 한 줄이면 아래의 복잡한 if-else를 전부 대체할 수 있다.
      // std::unordered_map 의 기본 생성 동작이 JAVA의 put-if-absent 처럼 작동하기 때문이다.
      auto& ptr_vec = self->topic_session_map[topic][idx];
      ptr_vec.push_back(session_ptr);
      session_ptr->addTopicInfo(topic, id_str, idx);
      //self->printMap(); // 개발 & 체크용.
    }
  );
}  
```
<br><br/>
10. C++ 과 의존성 분리
- JAVA Spring 에서 지긋지긋하게 들어봤을 '의존성 분리', 'Dependency Injection' 문제는 C++ 에서도 발생한다.
- 이번 프로젝트에서는 RedisService.h 에서 이와 관련된 문제가 발생했다.
- 자세한 내용은 아래의 코드에서 설명하였다.
```c++
/*
*  본 프로젝트에서는 boost::redis 를 활용하였다.
* 해당 라이브러리를 활용하기 위해서는 아래의 두 가지 헤더가 임포트 돼야 한다.
* #include <boost/redis.hpp>
* #include <boost/redis/src.hpp>
* 
* 그런데 중요한 점은 이러한 임포트가 프로젝트 전체에 결쳐서 단 1 번만 등장해야 한다는 점이다.
* 이러한 임포트 문을 가지고 있는 RedisService.h 를 다른 클래스(예를 들면 MsgBroker.h & .cpp 같은)에서
* #include "../src/redis/RedisService.h" 같이 임포트 하는 순간
* '... duplicate symbol ...' 어쩌고 하는 긴 에러 로그가 출련되면서 빌드에 실패한다.
* 그래서 RedisService.h 는 .cpp 없이 .h 안에 모든 구현이 포함돼 있다.

* 문제는 RedisService.h 의 멤버함수를 다른 클래스에서도 쓸 수 있어야 한다는 점이었다. 
* 즉, MsgBroker 가 RedisService 의 존재를 몰라도 RedisService의 멤버 함수를 호출해야 한다는 점에서
* 의존성 분리가 필요했던 것이다.
* 
* 크게 두 가지 방법이 사용될 수 있다.
* 
* 1. RedisService 의 멤버 함수를 콜백 객체로 만들어서 MsgBroker에게 전달
*   - 기능이 복잡하지 않을 때는 이 방법이 제일 구현이 쉬웠지만, 개별 함수마다 콜백 객체를 전부 별도 정의 & 초기화 해야 한다.
* 
* 2. RedisService 가 abstract class 를 오버라이드 하게 만든 후, 해당 abstract 클래스를 MsgBroker에 참조로서 초기화
*   - 구체 클래스(RedisService 같은)의 기능이 많아질 때는 추상 클래스를 활용한 인터페이스 방식이 코드 관리에 유리하다.
* 
* 본 프로젝트에서는 1. 번 방법이 사용됐다.
* RedisService 내부에서 외부의 호출이 가능해야 하는 멤버 함수가 1, 2 개로 적었기 때문이다.
* 2. 번 방법의 abstract class는 결국 자바의 interface 처럼 작동한다.
* 자세한 구현은 MsgBroker의 redis_publish_callback 객체와, main.cpp 의 관련 코드를 참조하길 바란다.
*/
```
<br><br/>
11. std::from_chars(...) 의 용도
- std::string 같은 char 시퀀스에서 '숫자'를 찾아낼 때 사용한다.
- 자세한 내용은 아래의 코드에서 설명한다.
```c++
#include <charconv>
#include <iostream>
#include <string>
#include <iostrem>

int main()
{
/*
* std::from_chars()는 ptr 과 ec 를 포함하는 std::from_chars_result 를 리턴한다.
* 
* ptr: a pointer to the first character that was not consumed by parsing.
* ec: an error code indicating whether parsing failed.
*/
    std::string value = "123abc";
    std::size_t length = 0;

    const auto [ptr, ec] = std::from_chars(
        value.data(),
         value.data() + value.size(),
        length
    );

    // 문자열이 숫자로 끝나지 않을 경우 아래의 if 문에 걸린다.
    // 이 경우, ptr 은 문자열의 마지막 위치를 가리키지 않게 된다.
    // 이 경우, ec 또한 빈 객체가 아니게 된다.
    if (ec != std::errc{} || ptr != value.data() + value.size())
    {
        std:cout << "문자열이 숫자로 끝나지 않음!!\n";
        return 0;
    }
    
    std::cout << number << '\n';  // 123
    return 0;
}
```
<br><br/>
12. 대량 동시 접속의 충격 분산시키기
- 성능 테스트 도중 STOMP 연결 요청 1만 개를 3~5 초 이내의 짧은 시간에 집중시켜보았다.
- 아래의 스펙을 가진 게이밍 노트북이었음에도 서버는 순식간에 죽었고, Client generator 는 '연결이 거부 됐습니다'라는 메시지로 가득 찼다. 
  - ASUS ROG STRIX SCAR Gaming laptop
  - CPU : 12th Gen Intel(R) Core(TM) i7-12700H(2.30 GHz / 14 core / 20 thread)
  - RAM : 16 GB
  - SSD : 1 TB
  - GPU : GeForce RTX 3060 laptop gpu TGP 140 W, VRAM 6 GB 
- 한 가지 참고할 점은, boost asio의 async_accept() 를 사용하지 않고도 1만 개의 대량 동시 접속 요청을 전부 수용하는 것에 성공했다는 점이다.
- async_accept()를 써도 같은 문제를 해결할 수 있지만, async_accept()는 lambda 캡처 / lambda 내 참조 객체의 수명 문제 / 재귀적 호출 + strand 직렬화 조합으로 루프 구성 /... 등등의 복잡한 구조를 도입해야 한다는 단점이 있다.
- 1 만 명의 대량 동시접속 요청을 짧은 시간 안에 전부 수용하는 것은 본 컴퓨터의 성능이 좋은 편이어서 가능했던 것이기도 하다.
  - 따라서 실제 상용 서비스를 구현한다면 Backpressure 를 제어하는 부분이 필수이다. 아무리 io_context 에다가 비동기 작업들을 마구 post 할 수 있다고 해도 이것 또한 서버 자원을 잡아먹는 것은 동일하기 때문이다.
```c++
// 공통 : main.cpp 에서 Server.cpp 내의 연결 요청 수신 while{...} 루프는 별도 스레드에서 실행
    Server server(main_io_context, msgBroker_ptr);
    Util::delayedExecutorAsyncByThread(
        0, [&server] { server.start(); }
    );
    
/*
* 최초의 구현은 아래와 같이 async 한 동작 없이 전부 sync로 처리하는 코드였다.
* 아래의 코드는 1000 명의 동시 접속은 감당할 수 있었지만, 1만 명으로 늘자 감당하지 못하고 서버가 죽었다.
* 서버가 죽은 주된 원인은 session_ptr->start(); 이 부분이 '언제 끝날지 알 수 없으며 시간을 많이 잡아먹는'
* 작업이었기 때문이다.
* 정확히는 클라이언트의 요청을 받아서 HTTP Get 요청을 STOMP websocket 으로 업그레이드 시킨 후 완료 응답을
* 전송해서 세션의 네트워크 통신을 시작시키는 작업이었다.
* 이 부분에서 blocking 된 채 시간을 잡아먹는 동안에 다른 요청들이 순식간에 빠른 속도로 누적되다가
* 결국 임계점을 넘으면서 후속 연결 요청들이 차단돼 버린 것이다.
* */
void Server::start()
{
...
    tcp::acceptor acceptor(
        io_context, tcp::endpoint(tcp::v4(), C::STOMP_PORT)
    );

    try
    {
        while (is_shutdown.load() == false)
        {
            auto raw_tcp_socket_ptr
                = std::make_shared<boost::asio::ip::tcp::socket>(acceptor.accept());

            session_id_counter += 1;
            auto session_id = session_id_counter.load();

            auto session_ptr = std::make_shared<Session>(
                session_id, raw_tcp_socket_ptr, io_context, *this
            );
            session_ptr->setMsgBroker(msg_broker_ptr);

            auto stomp_handler = std::make_shared<StompHandler>(session_ptr);
            session_ptr->setStompHandler(stomp_handler);
            session_ptr->start();

            session_id_map.emplace(session_id, session_ptr);
            logger->warning(
                "Dongvin, new client arrives, id: " + std::to_string(session_id)
                + " / total session cnt : " + std::to_string(session_id_map.size())
            );
        } //wh
    }
    catch (const std::exception& e){...}
...
}    
    
/*
* 개선된 코드는 async_accept()가 아닌, accept()를 계속 사용하면서도 
* session_ptr->start(); 같이 시간을 잡아먹는 작업만을 별도의 io_context 라는 일종의 태스크 큐에다가 
* 비동기 작업으로서 등록(boost::asio::post(strand, lambda);) 했다. 이 작업은 non-blocking 이다.
* 
* 그 결과, 연결을 수락하는 while 루프가 block 되지 않으면서 대량으로 들어오는 
* 연결 요청들을 연속적으로 치리할 수 있었고, 후속 STOMP 통신들의 활성화 작업은 io_context 가 전부 처리했다. 
* */
void Server::start()
{
...
    tcp::acceptor acceptor(
        io_context, tcp::endpoint(tcp::v4(), C::STOMP_PORT)
    );

    logger->info3("Dongvin, port " + std::to_string(C::STOMP_PORT) + " is listening...");
    try
    {
        while (is_shutdown.load() == false)
        {
            auto raw_tcp_socket_ptr
                = std::make_shared<boost::asio::ip::tcp::socket>(acceptor.accept());

            session_id_counter += 1;
            auto session_id = session_id_counter.load();

            auto session_ptr = std::make_shared<Session>(
                session_id, raw_tcp_socket_ptr, io_context, *this
            );
            session_ptr->setMsgBroker(msg_broker_ptr);

            auto stomp_handler = std::make_shared<StompHandler>(session_ptr);
            session_ptr->setStompHandler(stomp_handler);

            // session.start() 이 부분 만을 io_context 에 post 해서 비동기 실행시킨 이유가 있다.
            // 이 부분이 http 요청을 STOMP websocket 으로 업그레이드 & 초기화 시킨 후
            // 클라이언트에게 websocket 연결 요청 완료 응답을 보내는 부분이기 때문이다.
            // 이렇게 하지 않으면 1 만 명의 클라이언트들이 동시에 접속 요청을 했을 때
            // session.start() 부분이 후속 실행들을 blocking 해버리면서
            // 후속 연결 요청들이 NIC(Netwokr Interface Card) 연결 요청 큐에 쌓이다가 버려진다.
            // 연결 요청을 처리하는 속도보다 쌓이는 속도가 압도적으로 빨라서 발생하는 현상이다.
            // 이 경우 일부 클라이언트는 연결이 거부됐으며, 서버가 먹통이 되면서 비정상 종료됐다.
            boost::asio::post(
                strand, [session_ptr, this, session_id]()
                {
                    if (session_ptr->start())
                    {
                        session_id_map.emplace(session_id, session_ptr);
                        logger->warning(
                            "Dongvin, new client arrives, id: " + std::to_string(session_id)
                            + " / total session cnt : " + std::to_string(session_id_map.size())
                        );
                    } else
                    {
                        // 웹소켓 초기화에 실패했다면 삭제 예정 세션 맵으로 이동시켜서 나중에 삭제되게 만든다.
                        session_ptr->setShutdownTrue();
                    }
                }
            );
        } //wh
    }
    catch (const std::exception& e)
...
}
```
<br><br/>
13. boost asio strand 를 이용해서 자료구조에 대힌 thread safety 확보하기
- Server 클래스 내부에는 현재 클라이언트 세션들의 포인터가 <long K, session ptr V> 형태로 저장돼 있다.
- 예전 구현에서는 세션이 종료될 때마다 각 세션 객체가 Server 에게 '자신을 세션 맵에서 삭제해달라'고 요청했었다.
- 클라이언트 숫자가 많지 않을 때(1000 명 이하)는 잘 작동했지만, 10000 명 정도의 클라이언트가 동시에 전부 DISCONNECT 되자 이러한 접근은 결국 data race 에 의한 SIGABRT 에러를 발생시켰다.
- 해결 방법은 아래의 코드에서 설명한다.
```c++
/*
* 세션이 종료 또는 DISCONNECT 되면 다음과 같은 과정을 통해서 최종적으로 리소스가 회수된다.
* 우선, 개별 세션들 각자가 Server 에가 '자신을 삭제해달라'고 요창하는 방식을 버렸다.
* 대신
* 1. reserve_session_removal_task 가 10 초 간격으로 session_id_map의 copy 맵을 순회하면서 shut down 된 세션들을 '삭제 예정 맵'으로 이동시킴.
* 2. remove_session_task 가 30 초 간격으로 '삭제 예정 세션 맵'내 전체 요소를 삭제 -> 그 결과 shut down 된 세션들의 소멸자 호출.
* 
* 중요한 것은, 두 개의 periodic task 정의에 boost asio strand가 사용됐다는 점이다.
* strand 를 사용해서 task 들을 io_context 에 post 또는 bind 하여 실행하면 동시에 들어온 task 들이 io_context 내부에서 serialized 하게 실행된다.
* 즉, mutex, lock, condition_variable 을 사용하여 직접 thread safety를 구현한 것과 동일한 효과를 낸다.  
* */

//------------------ Server.h
class Session;

class Server : public std::enable_shared_from_this<Server>
{
public:
  ...
private:
  ...
  std::unordered_map<long, std::shared_ptr<Session>> session_id_map{};
  // shutdown 된 세션들은 별도의 맵에 모아뒀다가
  // 별도의 periodic task로 주기적으로(ex : 30 sec) 제거한다. 그래야 SIGABRT 에러 피할 수 있다.
  std::unordered_map<long, std::shared_ptr<Session>> shutdown_session_map{};
  PeriodicTask reserve_session_removal_task;
  PeriodicTask remove_session_task;
  PeriodicTask heart_beat_task;
...
};

//------------------ PeriodicTask.h
class PeriodicTask
{
public:
  using TaskCallback = std::function<void()>;
  explicit PeriodicTask(
    boost::asio::io_context& io_context,
    boost::asio::strand<boost::asio::io_context::executor_type> inputStrand,
    std::chrono::milliseconds inputInterval
  );
...
  void setTask(TaskCallback inputTask);
...
  void start();
  void stop();

private:
  void scheduleTask();

  std::shared_ptr<Logger> logger;
  boost::asio::strand<boost::asio::io_context::executor_type> strand;
  boost::asio::steady_timer timer;
  std::chrono::milliseconds interval;
  TaskCallback task;
  bool running;
  bool isTaskSet;
};


//------------------ PeridicTask.cpp
PeriodicTask::PeriodicTask(
  boost::asio::io_context& io_context,
  boost::asio::strand<boost::asio::io_context::executor_type> inputStrand,
  std::chrono::milliseconds inputInterval
) : strand(std::move(inputStrand)),
    timer(io_context),
    interval(inputInterval),
    running(false),
    isTaskSet(false),
    logger(Logger::getLogger(C::PERIODIC_TASK))
{
}
...
void PeriodicTask::setTask(TaskCallback inputTask)
{
  task = std::move(inputTask);
  isTaskSet = true;
}
...
void PeriodicTask::start()
{
  if (!isTaskSet)
  {
    logger->severe("no task to run!");
  }
  else
  {
    running = true;
    scheduleTask();
  }
}
...
void PeriodicTask::scheduleTask()
{
  timer.expires_after(interval);
  timer.async_wait(boost::asio::bind_executor(
    strand,
    [this](const boost::system::error_code& ec)
    {
      if (!ec)
      {
        if (task)
        {
          task();
        }
        if (running)
        {
          scheduleTask();
        }
      }
      else
      {
        // used ec.message() instead of ec.what() for boost backward comparability
        logger->severe("boost steady timer failed! error message : " + ec.message());
      }
    }
  ));
}

//------------------ Server.cpp
void Server::start()
{
    logger->info2("Server Starts!");

    // 세션 예비 삭제 타이머  - shutDown == true 표시된 세션들을 삭제 예정 맵으로 이동 시킨다.
    // PeriodicTask 들은 전부 main_io_context 로 만들어진 strand 를 통해서
    // io_context에 bind 된 후 실행되기 때문에 여러 개의 periodic task 들은 서로 동시에 실행되는 일이 없다.
    reserve_session_removal_task.setTask([&]()
        {
            auto copied_session_map = session_id_map;
            for (const auto& [session_id, session_ptr] : copied_session_map)
            {
                if (session_ptr->isShutDown())
                {
                    shutdown_session_map.insert({session_id, std::move(session_ptr)});
                    session_id_map.erase(session_id);
                    logger->warning(
                        "Session, " + std::to_string(session_id) +
                        " shuts down. Remaining session cnt : "
                        + std::to_string(session_id_map.size())
                    );
                }
            }
        }
    );
    reserve_session_removal_task.start();
    logger->info3("Dongvin, timer for session delete reservation start!");

    // 세션 실제 제거 타이머
    remove_session_task.setTask([&]()
        {
            shutdown_session_map.clear();
            logger->severe("Dongvin, completely removed sessions.");
        }
    );
    remove_session_task.start();
    logger->info3("Dongvin, timer for closed session removal starts!");

    // heart-beat 타이머
    heart_beat_task.setTask(
        [&]()
        {
            // CppTips.md 5 번 참고.
            auto copied_session_map = session_id_map;
            int alive_session_count = 0;
            for (const auto& [session_id, session_ptr] : copied_session_map)
            {
                // 가장 최근에 해당 세션의 클라이언트에게서 온 heart beat 시각과 현재 시각을 대조한다.
                const int64_t beat_arrive_time_millis = Util::getCurrentTimeMillis();
                const int64_t latest_heart_beat_time_millis
                    = session_ptr->getLatestHeartBeatTimeMillis();
                if (
                    const int64_t diff = beat_arrive_time_millis - latest_heart_beat_time_millis;
                    diff > static_cast<int64_t>(C::HEART_BEAT_THRESHOLD_MS)
                )
                {
                    session_ptr->setShutdownTrue();
                    continue;
                }
                if (session_ptr->isShutDown() == false)
                {
                    alive_session_count++;
                    session_ptr->sendHeartBeatToClient();
                }
            } //for
            logger->severe(
                "Dongvin, sent hear-beat to alive sessions. cnt : "
                + std::to_string(alive_session_count)
            );
        }
    );
    heart_beat_task.start();

    tcp::acceptor acceptor(
        io_context, tcp::endpoint(tcp::v4(), C::STOMP_PORT)
    );

    logger->info3("Dongvin, port " + std::to_string(C::STOMP_PORT) + " is listening...");
    try
    {
        while (is_shutdown.load() == false)
        { ...} //wh
    }
    catch (const std::exception& e){...}
}

```



















