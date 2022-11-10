# Round Trip Time Measurement

## Abstract
변화하는 traffic 상황에 적응하는 RUDP를 만들기 위해서는, 실시간 RTT 측정이 필수적입니다
RTT 측정 즉, Round Trip Time Measurement를 줄여서 RTTM이라고 명명합니다.
본 문서와 관련된 RFC 표준은 아래와 같습니다

* [RFC 1323](http://www.networksorcery.com/enp/rfc/rfc1323.txt)

## Kan's Algorithm
retransmit이 발생한 segment에 대해서는 RTT를 측정하지 않는 알고리즘입니다.

## Time Stamp Option