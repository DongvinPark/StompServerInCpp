# C++ STOMP Server Performance Test

---

# 개발 환경

---

- **_C++ 20 or Higher_**
  - boost::redis 는 C++17 부터 사용 가능하지만, Redis Pub/Sub 리스너 구현에 사용된 코루틴 때문에 C++20 or higher 가 필수입니다.
  - 2026년 10월 1일 기준 각 OS들의 최신 C++ 컴파일러들을 사용하는 것을 권장합니다.
    - Linux : gcc & g++ latest
    - Apple Silicon Mac : clang(XCode) latest
    - Windows : Visual Studio latest(설치시 MSVC도 같이 설치)
---
- **_Boost 1.92 or Higher_**
  - boost::redis 는 boost 1.84 버전부터 사용 가능하지만, C++ 코루틴을 사용한 redis pub/sub listener 를 에러 없이 실행하려면 OS들의 boost lib 모듈 전체의 버전을 1.92로 통일시키는 것을 권장합니다.
  - 1.92는 2026년 10월 1일 기준 가장 최신 버전입니다.
  - boost 라이브러리 설치 방법
    - macOS 에서는 homebrew 를 사용합니다.
    - Windows 에서는 vcpkg 를 사용합니다.
    - Linux 에서는 Ubuntu 24.04 LTS 기준으로 아래의 터미널 명령어들로 설치합니다. 
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
- **_Install OpenSSL and Link It_**
  - 각 OS 별로 OpenSSL이 설치돼 있어야 합니다.
  - 현재 CMakeLists.txt에서는 OpenSSL과 Boost 라이브러리가 설치 돼 있다고 가정하고 이 둘을 링크합니다.