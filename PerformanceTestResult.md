# 성능 테스트 결과

## 테스트 방법
<img width="900" height="537" alt="Image" src="https://github.com/user-attachments/assets/1c73a088-80c6-4cac-9501-115be841258b" />

- 1 개의 virtual thread == 1 명의 web socket client 가 성립함.
  - Fan Generator Cluster 내의 EC2는 JDK 21의 virtual thread로 클라이언트 생성. 
- 순수하게 STOMP web socket 서버에만 부하를 주기 위해서,
    - 로컬 WIFI 라우터를 사용하지 않고 AWS 내부망 만을 사용함.
    - Fan Generator Cluster 내의 EC2를 4개를 사용해서 Client EC2가 수신을 늦게 해서 성능 테스트의 결과가 왜곡되지 않게 함.
- STOMP Server는 동시접속해 있는 Client에게 '서버 측 전송 시작 시각'을 body에 포함하는 STOMP frame Fan-Out 전송 함.
- 각각의 Client 들은 STOMP Server의 응답 frame을 수신한 시각을 기록하고, 아래의 값을 계산함.  
```text
['Server 응답 수신 시각' - 'Server의 전송 시작 식각'] == [S -> C Delay time(ms)]
```
- 전체 클라이언트들의 [S -> C Delay time(ms)] 들의 분포를 보고 최소, 최대, 평균, 표준편차, P99 Latency 를 측정함.
- 동시 접속 클라이언트의 숫자를 2500, 5000, 10000 으로 올려가며 테스트 함.
- C++/BoostAsio STOMP 서버와 Spring/STOMP 서버에 대해서 동일한 테스를 1 번씩 실행하여 두 서버의 행동 양태를 비교함. 
  <br><br/>

## 용어 정의
- 동시 접속 클라이언트
  -  STOMP Server의 입장에서 최초 접속 클라이언트와 마지막 접속 클라이언트 간의 시간 차이가 10 초 미만이고 그 사이에 모든 클라이언트들이 접속을 완료했을 때, 클라이언트 전체를 테스트 결과에 반영 가능한 유효 동시 접속 클라이언트로 간주합니다.
  <br><br/>

## 테스트 결과
### 1. STOMP Server -> Each Fan Latency
Unit: ms

| Concurrent Users | Implementation | Min | Max | Average | Std Dev | P50 | P95 | P99 |
|---:|:---:|---:|---:|---:|---:|---:|---:|---:|
| 2,500 | C++ | 12 | 52 | **31.10** | 9.49 | 31 | 46 | 49 |
| 2,500 | Java | 18 | 67 | 42.19 | 12.15 | 43 | 62 | 66 |
| 5,000 | C++ | 12 | 82 | **45.82** | 17.41 | 44 | 78 | 81 |
| 5,000 | Java | 27 | 103 | 68.22 | 21.20 | 71 | 97 | 102 |
| 10,000 | C++ | 12 | 113 | **69.34** | 28.24 | 73 | 109 | 112 |
| 10,000 | Java | 31 | 151 | 91.31 | 32.50 | 93 | 139 | 145 |

### 2. Server TX Completion Latency(서버가 메시지 fan-out 완료에 걸린 시간)
Unit: ms

| Concurrent Users | C++ | Java | Faster Implementation |
|---:|---:|---:|:---:|
| 2,500 | **13** | 16 | C++ |
| 5,000 | 31 | **30** | Java |
| 10,000 | 53 | **49** | Java |  

### 3. Average Fan Latency Comparison

| Concurrent Users | C++ Average | Java Average | C++ Latency Reduction |
|---:|---:|---:|---:|
| 2,500 | 31.10 ms | 42.19 ms | **26.3%** |
| 5,000 | 45.82 ms | 68.22 ms | **32.8%** |
| 10,000 | 69.34 ms | 91.31 ms | **24.1%** |

<br><br/>

## 테스트 결과 해석
- ***[C++ 이 자바보다 빠르고, 메모리를 덜 사용하기는 하지만 그게 '직접 다 개발 및 컨트롤 해야 하는' 비용을 감수할 만큼 '압도적'이지는 않았습니다.]()***
  - STOMP 서버는 3 종류의 연산(CPU heavy, I/O heavy, GPU heavy) 중에서 I/O heavy(SSD, Network read/write 등) 작업이 대부분이기 때문입니다.
  - 컴퓨터의 Network H/W와 Network를 살제로 담당하는 커널은 STOMP 서버 코드가 어떤 언어로 작성돼 있든 동일하기 때문입니다.
  - I/O heavy 작업을 하는 서버(API, Messaging, Streaming 등)는 코드의 효율화로 성능 개선은 가능 하지만, 성능은 '7 계층 앱의 코드' 만으로 결정 되지 않습니다.
  - 물론, 트래픽의 규모가 커지면 그 '약간'의 개선이라 할지라도 비용(혹은 리스크) 절감에 큰 도움을 줄 수도 있습니다.

<br><br/>