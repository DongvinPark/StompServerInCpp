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

## 테스트 결과 요약
- ??
  <br><br/>

## 테스트 결과 데이터
- ??
  <br><br/>

## 테스트 결과 해석
- ??
  <br><br/>