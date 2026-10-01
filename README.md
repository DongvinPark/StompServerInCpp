# C++ STOMP Server Performance Test

---

# 개발 환경

---

- **_C++ 20 or Higher_**
  - boost::redis 는 C++17 부터 사용 가능하지만, Redis Pub/Sub 리스너 구현에 사용된 코루틴 때문에 C++20 or higher 가 필수입니다.
  - 2026년 10월 1일 기준 각 OS들의 최신 C++ 컴파일러들을 사용하는 것을 권장합니다.
    - Linux : gcc latest
    - Apple Silicon Mac : clang(XCode) latest
    - Windows : Visual Studio latest(설치시 MSVC도 같이 설치)
---
- **_Boost 1.92 or Higher_**
  - boost::redis 는 boost 1.84 버전부터 사용 가능하지만, C++ 코루틴을 사용한 redis pub/sub listener 를 에러 없이 실행하려면 OS들의 boost lib 모듈 전체의 버전을 1.92로 통일시키는 것을 권장합니다.
  - 1.92는 2026년 10월 1일 기준 가장 최신 버전입니다.
---
- **_Install OpenSSL and Link It_**
  - 각 OS 별로 OpenSSL이 설치돼 있어야 합니다.
  - 현재 CMakeLists.txt에서는 OpenSSL과 Boost 라이브러리가 설치 돼 있다고 가정하고 이 둘을 링크합니다.