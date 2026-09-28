# C++ STOMP Server Performance Test

---

# 필요 사항
- **_C++ 17 or Higher_**
  - boost::redis 는 C++17 or higher 를 필요로 합니다.
  - boost::redis 는 OS 별로 아래의 컴파일러를 필요로 합니다.
    - Linux : gcc 11 and later
    - Mac : clang 11 and later
    - Windows : Visual Studio 16(2019) and later
- **_Boost 1.84 or Higher_**
  - boost::redis 는 boost 1.84 버전부터 사용 가능합니다. 
- **_Install OpenSSL and Link It_**
  - 현재 CMakeLists.txt에서 링크하고 있습니다.