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

---

## 용어 정의
- 동시 접속 클라이언트
  -  STOMP Server의 입장에서 최초 접속 클라이언트와 마지막 접속 클라이언트 간의 시간 차이가 10 초 미만이고 그 사이에 모든 클라이언트들이 접속을 완료했을 때, 클라이언트 전체를 테스트 결과에 반영 가능한 유효 동시 접속 클라이언트로 간주합니다.
  <br><br/>

---

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

#### Graphs
- Average S -> C Latency
  <br/>
  <img width="790" height="490" alt="Average S→C Latency" src="https://github.com/user-attachments/assets/59596abc-4fd4-45fa-8a69-70573f279872" />
- S -> C Latency Standard Deviation
  <br/>
  <img width="790" height="490" alt="Latency Standard Deviation" src="https://github.com/user-attachments/assets/1c0f5efe-63e9-4e80-8a0c-4f6a0a9c1bf5" />
- P99 S -> C Latency
  <br/>
  <img width="790" height="490" alt="P99 S→C Latency" src="https://github.com/user-attachments/assets/694ce092-18a7-4c81-95cd-04bb8286163f" />

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

### 4. Heap Memory Trace Graphs
#### Java / 2.5 K concurrent clients / measured by jconsole
<img width="587" height="380" alt="heap-memory-singlethread-2 5K" src="https://github.com/user-attachments/assets/be65e231-fe96-46f2-8f58-ab5c3afbeb36" />
<br/>

#### C++ / 2.5 K concurrent clients / measured by valgrind
<img width="734" height="752" alt="cpp-mem-2500" src="https://github.com/user-attachments/assets/1ba92d43-5b71-4d73-8201-19cfbebc0771" />
<br/>

#### Java / 5.0 K concurrent clients / measured by jconsole
<img width="578" height="382" alt="jvm-heap-memory-singlethread-5k" src="https://github.com/user-attachments/assets/5ba07fab-a5c8-4dac-802b-20b424832be8" />
<br/>

#### C++ / 5.0 K concurrent clients / measured by valgrind
<img width="739" height="636" alt="cpp-mem-5000" src="https://github.com/user-attachments/assets/8d4cf95b-8c48-4570-b253-133ceafefe9e" />
<br/>

#### Java / 10.0 K concurrent clients / measured by jconsole
<img width="578" height="381" alt="stomp-singlethread-heap-memory" src="https://github.com/user-attachments/assets/41862cc3-f394-4850-afd2-6446631927bd" />
<br/>

#### C++ / 10.0 K concurrent clients / measured by valgrind
<img width="737" height="618" alt="cpp-mem-10000" src="https://github.com/user-attachments/assets/e30a5a3c-5b4d-4109-a886-7301c742c0eb" />

<br><br/>

---

## 테스트 결과 해석
- ***[C++이 Java보다 빠르고 메모리 사용량도 적었지만, 그 성능 차이가 직접 구현하고 관리해야 하는 복잡성과 비용을 감수할 만큼 압도적이지는 않았습니다.]()***
  - STOMP 서버의 주요 작업은 Network I/O를 중심으로 이루어지며, 메시지 수신 및 전송, 세션 관리, STOMP 프로토콜 파싱 등의 작업이 복합적으로 수행됩니다.
  - Network I/O의 성능은 애플리케이션 코드뿐만 아니라 Network H/W, OS Kernel, TCP/IP Stack, Network Bandwidth 등 다양한 요소에 의해 결정되기 때문입니다.
  - 따라서 서버를 어떤 프로그래밍 언어로 구현하더라도 네트워크 하드웨어와 OS Kernel의 기본적인 처리 한계는 동일하며, 애플리케이션 코드의 효율화만으로 전체 시스템의 성능을 극적으로 개선하는 데에는 한계가 있습니다.
  - 물론 시스템 콜 호출 방식, 메모리 복사, 스레드 모델, I/O 처리 방식 등의 최적화를 통해 성능을 개선할 수 있습니다. 실제 테스트에서도 C++이 Java보다 일관되게 낮은 S -> C Latency를 보여주었습니다.
  - 하지만 I/O 중심의 서버에서는 프로그래밍 언어 자체의 성능만큼이나 네트워크 환경, 시스템 아키텍처, 동시성 처리 방식, H/W의 성능 등이 중요합니다.
  - 트래픽 규모가 커질수록 작은 성능 차이도 서버 인스턴스 수, 인프라 비용 및 운영 리스크 절감으로 이어질 수 있습니다.
  - 따라서 C++의 성능적 이점이 작다는 이유만으로 무시할 필요는 없으며, 요구되는 성능과 개발 및 유지보수 비용을 함께 고려하여 기술을 선택하는 것이 좋다고 생각합니다.

<br><br/>