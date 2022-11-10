# Mesh RUDP Retransmission

## Abstract
Mesh RUDP는 Reliable transmission을 보장합니다.
Reliable transmission이 이루어지기 위해서는, 통신중에 drop된 segment에 대한 재전송(Retransmission)이 필수적입니다.
본 문서에서는 Mesh RUDP의 Retransmission 구현에 관해 다룹니다.
본 문서와 관련된 RFC 표준은 아래와 같습니다.

* RFC 2988
* RFC 6298


## Key terms explanation
* RTT : Round Trip Time
    * segment를 전송하고, 전송한 segment에 대한 ACK를 받을때까지의 걸린 시간
* RTO : Retransmission Timeout
    * 전송된 한 세그먼트에 대한 ACK를 기다려야 하는 시간
    * Retransmission이 trigger되는 시간
    * RTO는 MeshRUDP의 RTT보다 약간 더 큰게 이상적입니다. (즉, 세그먼트 전송부터 ACK 받기까지 걸리는 시간보다 약간 더 크면 좋음)
    * RTT보다 약간 더 큰게 얼마나 커야하는지가 핵심


## Variables used for retransmission
* MRTT : Measured RTT
    * 측정된 RTT 값
* SRTT : Smoothed RTT
    * 단순 평균 방식보다 최근 망 상황에 더 높은 비중을 주는 가중 평균 방식으로 계산
    * 최근 RTT값에 더 높은 가중치를 부여함 : EWMA (Exponential Weighted Moving Average)
* RTTVAR : RTT Variation
    * RTT 편차
    * 측정된 RTT 값의 변동성을 고려함
* RTO : Retransmission Timeout
    * SRTT, RTTVAR를 통해서 계산되어짐
* G : Clock Granularity
    * RTO 계산할때, RTTVAR이 0일 경우에만 활용되는 변수
    * Timer가 측정할 수 있는 minimum interval 값을 의미


## SRTT, RTTVAR, RTO calculation

### Initial state
```
RTO = InitRTO(1000 : 1초)
```

### First measurement
```
SRTT    = MRTT
RTTVAR  = MRTT / 2
RTO     = SRTT + Max(G, 4 * RTTVAR)
```

### Subsequent measurement
```
alpha   = 0.125 (1/8)
beta    = 0.25  (1/4)

SRTT    = (1 * a) SRTT + a * MRTT
RTTVAR  = (1 * b) RTTVAR + b * |RTTVAR * MRTT|
RTO     = SRTT + Max(G, 4 * RTTVAR)
```

* a, b값은 SRTT, RTTVAR의 최근 측정에 가중치를 주기 위한 상수
* 위에서 RTO값은 평균 RTT보다 약간 더 큰게 이상적이라고 했고, RTT보다 얼마나 더 큰게 핵심이라고 했다.
    * 여기서 평균 RTT는 최근 측정에 가중치를 둔 SRTT로 하고,
    * RTO가 RTT보다 더 커야하는 정도는 RTT 편차 값인 RTTVAR의 4배가 더 큰 값이 된다.

### After retransmission
타임아웃 되고나서, 실제로 retransmission 발생시에는 아래처럼 계산한다

* Karn's algorithm
    * 재전송 segment는 RTT 측정에서 제외합니다
* Exponential backoff
    * 기존에 계산된 RTO 값은 재전송이 일어난 이후에는, congestion control을 위해 RTO를 점차 2배씩 늘려 나가는 방식을 사용합니다.