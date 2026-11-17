# C++ STOMP Server Performance Test

---

## 프로젝트 목표
### SpringBoot 웹소켓 서버에서 발생한 P99 Latency 증가 문제 해결 및 성능 측정
- SpringBoot/STOMP 웹소켓 서버에서는 동시접속자의 숫자가 많아 질수록, 서버 -> 클라이언트 도달 기준 P99 Latency 가 선형적으로 증가했습니다.  
  - 이는 SpringBoost 내부의 메시지 브로커가 멀티 스레드로 작동하지 못하고 싱글 스레드 & 순차 실행으로 메시지를 전달하기 때문인 것으로 추정됩니다.
  - [SpringBoost/STOMP 서버 Github](https://github.com/DongvinPark/stomp-redis-test)
- ***C++ Boost Asio 를 활용한 서버에서는 이러한 문제를 해결할 수 있는지 확인해보고자 합니다.***

---

## 성능 테스트 결과
- ***약간의 개선은 가능했지만, 근본적인 해결 또는 압도적인 격차를 내는 것은 불가능 했습니다.***
- ***동시접속자가 늘어날수록 P99 Latency 가 증가하는 것은 C++에서도 피할 수 없었습니다.***
- ***테스트 결과, Spring/STOMP 측에서 '싱글 스레드로 전송한다'라는 가정은 틀렸다고 봐야 합니다.***
- [테스트 결과 링크](https://github.com/DongvinPark/StompServerInCpp/blob/main/PerformanceTestResult.md)

---

## 배운점
### [부하에도 종류가 있으며, 병목의 원인에 따라 애플리케이션 계층에서 해결할 수 있는 문제와 그렇지 않은 문제가 있습니다.]()
- 부하의 종류는 크게 5 가지로 볼 수 있습니다.
  - I/O-bound: Disk I/O, Network I/O 등 데이터 입출력에 의해 발생하는 병목
  - CPU-bound: 데이터 처리, 프로토콜 파싱, 암호화 등 연산 능력에 의해 발생하는 병목
  - Memory-bound: 메모리 용량, 메모리 접근 속도 및 대역폭에 의해 발생하는 병목
  - GPU-bound / Hardware-bound: GPU 또는 특정 하드웨어의 처리 능력에 의해 발생하는 병목
  - Synchronization-bound: Thread 간 Lock Contention, Context Switching 등 동기화 비용에 의해 발생하는 병목
- 이번 STOMP 서버는 I/O-bound 부하를 주로 다루었습니다.
  - C++과 Java 양측이 각자가 할 수 있는 '거의 최대한의 효율적 구조'를 이미 채택하고 있었기 때문에 I/O 를 담당하는 H/W가 달라지지 않는 이상 양측이 '압도적인 개선'을 할 수 있는 여지가 부족했습니다. 
### [특정 기술을 무조건 선택하기보다, 워크로드를 기반으로 병목을 분석한 후 성능과 개발 비용 사이에서 타협을 해야합니다.]()
- C++은 기타 고생산성 언어(Python 등)에 비해 생산성은 떨어지지만, 머신의 리소스를 세밀하게 제어하고 성능을 최대치로 끌어낼 수 있습니다.
- 반면 Java는 여러 편의 기능들과 성숙한 라이브러리 생태계를 활용하여 C++ 보다 빠른 시간에 개발할 수 있습니다.
- 이번 테스트에서는 C++이 Java보다 대부분은 빨랐지만 항상 압도적으로 빠른 것은 아니었으며, 부하의 특성과 구현 방식에 따라 성능 차이가 달라졌습니다.
- 특히 현재 규모(10K 동시접속자)의 STOMP 메시지 Fan-Out 부하에서는 Java 역시 충분히 경쟁력 있는 성능을 보여 주었습니다.

---

## 서버 아키텍처
<img width="816" height="580" alt="스크린샷 2026-11-16 오후 3 46 11" src="https://github.com/user-attachments/assets/8d1bb66d-bd08-40b0-9183-47aaab7b2bd8" /><br/>
- Message Broker 는 생성 & 초기화 완료된 STOMP Session 들의 포인터를 가지고 있습니다.
  - 이때, 하나의 List에 pointer 들을 전부 저장하지 않고, N 개의 std::vector 에 나누어 저장합니다.
- Msg 를 Fan-Out 해야 할 때, N 개의 스레드를 만들고, 각각의 스레드들이 Session Pointer 들이 저장된 std::vector를 하나씩 담당합니다.
  - 1 개의 스레드는 1 개의 std::vector 안에 있는 Session Pointer 들을 순처적으로 접근하면서 Msg를 websocket write 합니다.

---

## 유의사항
### STOMP 프로토콜 일부 미구현
- 성능 테스트의 편의를 위해서 STOMP 프로토콜 중 필요한 부분 만을 구현했습니다.

---

## 개발 환경

---

### **_C++ 20 or Higher_**
  - boost::redis 는 C++17 부터 사용 가능하지만, Redis Pub/Sub 리스너 구현에 사용된 코루틴 때문에 C++20 or higher 가 필수입니다.
  - 2026년 10월 1일 기준 각 OS들의 최신 C++ 컴파일러들을 사용하는 것을 권장합니다.
    - Linux : Ubuntu 22.04 LTS or higher / gcc 11.4 or higher / g++ 11.4 or higher
    - Apple Silicon Mac : clang(XCode) latest
    - Windows : Visual Studio latest(설치시 MSVC도 같이 설치)
---
### **_Boost 1.92 or Higher_**
  - boost::redis 는 boost 1.84 버전부터 사용 가능하지만, C++ 코루틴을 사용한 redis pub/sub listener 를 에러 없이 실행하려면 OS들의 boost lib 모듈 전체의 버전을 1.92로 통일시키는 것을 권장합니다.
  - 1.92는 2026년 10월 1일 기준 가장 최신 버전입니다.
  - boost 라이브러리 설치 방법
    - macOS 에서는 homebrew 를 사용합니다.
    - Windows 에서는 vcpkg 를 사용합니다.
    - Linux 에서는 Ubuntu 22.04 LTS or higher 그리고 Amazon Linux 기준으로 아래의 터미널 명령어들로 설치하면 됩니다. 
```text
- 사전 준비 작업
gcc/g++/cmake/openssl 이 전부 설치돼 있어야 하고,
C++20을 빌드 할 수 있어야 합니다.

- 빌드 도구 준비
sudo apt update
(필요시) sudo apt upgrade
sudo apt install build-essential

- 부스트 라이브러리 압축 파일 다운로드
cd ~/Downloads
wget https://archives.boost.io/release/1.92.0/source/boost_1_92_0.tar.gz

- 압축해제 후 결과물 디렉토리로 이동
tar -xzf boost_1_92_0.tar.gz
cd boost_1_92_0

- 해당 디렉토리 안에 있는 부트스트랩 실행
./bootstrap.sh

- /usr/local 디렉토리에 설치합니다.
  그래야 CMakeLists.txt 로 빌드할 때 편리하게 찾아낼 수 있습니다.
sudo ./b2 install --prefix=/usr/local

- 설치 확인
ls /usr/local/include/boost/version.hpp

- 이 명령어를 실행했을 때, 터미널에서 결과가 아래와 같이 나왔다면 성공입니다.
grep BOOST_LIB_VERSION /usr/local/include/boost/version.hpp

//  BOOST_LIB_VERSION must be defined to be the same as BOOST_VERSION
#define BOOST_LIB_VERSION "1_92"
```
---
### **_Install OpenSSL and Link It_**
  - 각 OS 별로 development 용 OpenSSL 이 설치돼 있어야 합니다.
  - 현재 CMakeLists.txt에서는 OpenSSL과 Boost 라이브러리가 설치 돼 있다고 가정하고 이 둘을 링크합니다.
---
### 테스트용 Stomp Client 사용법
  - python 을 설치해줍니다.
  - 현 프로잭트의 test-stomp-browser-cliet 디렉토리로 이동합니다.
    - 그러면 index.html / app.js / main.css 파일들이 있습니다.
  - 아래의 명령어를 실행해서 간단한 http 서버를 실행하고, browser 에서 http://localhost:3000 으로 접속합니다.
  - 포트 번호가 일치해야 합니다.
```text
python3 -m http.server 3000
```

