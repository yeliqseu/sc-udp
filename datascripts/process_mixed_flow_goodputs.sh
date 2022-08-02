#!/bin/bash
awk -v nRTT=$1 -v nPacket=$2 -v pktSize=$3 \
    '$1~/Client/ {split($2,a,"."); hostid=a[3]; if (seen[hostid]==0) {seen[hostid]=1; nhost+=1}} \
     $6~/DecoderSuccess/ {delivered=($16-$14+1)*1440; goodputs[hostid, int($4/nRTT)]+=delivered; total[hostid]+=delivered} \
     $6~/TcpPacketSink/ {goodputs[hostid, int($4/nRTT)]+=$8; total[hostid]+=$8} \
     {if (total[hostid]==nPacket*pktSize) {for (h=1;h<=nhost;h++) {printf("%d ", h); for (n=1;n<=int($4/nRTT)-1;n++) {printf("%d ", goodputs[h, n])}; printf("\n")}; exit}}\
    ' $4 > $5
