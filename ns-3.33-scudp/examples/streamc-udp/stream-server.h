#ifndef STREAM_SERVER_H
#define STREAM_SERVER_H

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/timer.h"
#include "ns3/ipv4-address.h"
#include "ns3/traced-callback.h"
#include "infoqueue.h"
#include "tiles.h"
#include "ns3/random-variable-stream.h"
#include "ns3/ptr.h"
#include "ns3/double.h"
#include "ns3/rng-seed-manager.h"
#include "ns3/data-rate.h"
#include "ns3/windowed-filter.h"

extern  "C" {
	#include "streamcodec.h"
}

namespace ns3 {

class Socket;
class Packet;

class StreamServerApp : public Application
{
public:
  StreamServerApp ();

  void SetAttribute (Address clientAddr, int32_t numPackets, int32_t packetSize);

  static TypeId GetTypeId (void);
  
private:
  virtual void StartApplication (void);

  virtual void StopApplication (void);
  
private:
  Ptr<Socket> m_socket;
  Address m_peerAddress;
  int32_t m_packetSize;
  int32_t m_numPackets;
  unsigned char *m_buf;
  struct parameters m_cp;
  struct encoder *m_enc;
  double m_extraRepair;         // 超出补偿丢包率所需的修复分组比例（有序译码时延正比于1/repairExcess）
  double m_paramSendInterval;   // 编码系数传输间隔（直至客户端确认参数）
  bool m_paramAcked = false;            // 接收端是否确认了传输前编码参数的设置
  double m_dataStartTime;       // 数据传输的开始时间

  InfoQueue m_pktInfoQueue;     // 记录发送各分组时间，以及相应其它分组发送数

  int32_t m_lastAckedSourceId = -1;
  int32_t m_lastAckedRepairId = -1;
  int32_t m_lastAckedInorderId = -1;
  int32_t m_lastAckedSourceNum = 0;
  int32_t m_lastAckedRepairNum = 0;
  int32_t m_lastSentSourceId = -1;
  int32_t m_lastSentRepairId = -1;
  int32_t m_nSourceSinceLastRepair = 0;

  int32_t m_packetsInFlight = 0;
  double m_lastAckTime = 0.0;           // time of receiving last ACK (in sec.)

  int32_t m_initCWnd;
  int32_t m_cWnd;
  // rtt estimation
  typedef WindowedFilter<Time, MinFilter<Time>, uint64_t, uint64_t> RTTFilter;
  RTTFilter m_minRttFilter;
  double m_iRtt = 0.0;    // instant RTT
  double m_sRtt = 0.0;    // smoothed RTT

  // bandwidth estimation
  typedef WindowedFilter<double, MaxFilter<double>, uint64_t, uint64_t> BWFilter;
  BWFilter m_maxBwFilter;
  int32_t m_newAcked = 0;
  int32_t m_bwWindowLength;
  double m_estBw = 0.0;
  double m_lastBwEstTime = 0.0;
  double m_avgAckInterval = 0.0;    // TIBET

  // loss rate estimation
  double m_lossRate = 0.0;

  // pacing
  bool m_pacing;
  double m_pacingGain;
  DataRate m_pacingRate;
  Timer m_pacingTimer {Timer::CANCEL_ON_DESTROY}; //!< Pacing Event
  
  /// Callbacks for tracing the packet Tx events
  TracedCallback<Ptr<const Packet> > m_txTrace;
  
  /// Callbacks for tracing the packet Rx events
  TracedCallback<Ptr<const Packet> > m_rxTrace;
  
  /// Callbacks for tracing the packet Tx events, includes source and destination addresses
  TracedCallback<Ptr<const Packet>, const Address &, const Address &> m_txTraceWithAddresses;
  
  /// Callbacks for tracing the packet Rx events, includes source and destination addresses
  TracedCallback<Ptr<const Packet>, const Address &, const Address &> m_rxTraceWithAddresses;
  
private:
  int32_t PacketsInFlight (void);

  bool TimeToSendRepairPacket (void);

  void RttEstimation (double recvTime, double sendTime);

  void BwEstimationTibet(int32_t inorder, int32_t nsource, int32_t nrepair);

  void BwEstimationJersy(int32_t inorder, int32_t nsource, int32_t nrepair);

  int32_t PeEstimation (int32_t nSource, int32_t nRepair);

  void UpdateCWnd (void);

  void NotifyPacingPerformed (void);

  void SendDataPackets (void);

  void SendParameter (void);

  void CompleteSend (void);

  void HandleRead (Ptr<Socket> socket);

  void PacketAckRecv (Ptr<Packet> packet);

  int32_t ParamAckRecv (Ptr<Packet> prm);
};

} //ns3 namespace 

#endif
