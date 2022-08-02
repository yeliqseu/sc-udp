/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/ipv4-address.h"
#include "ns3/traced-callback.h"
#include "ns3/log.h"
#include "ns3/nstime.h"
#include "ns3/inet-socket-address.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/trace-source-accessor.h"
#include "handlepacket.h"
#include "infoqueue.h"
#include <iomanip>
#include <string>
#include <deque>
#include <math.h>
#include "stream-server.h"

extern "C" {
	#include "streamcodec.h"
	//#include "handlepacket.h"
}

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("StreamServerApp");

NS_OBJECT_ENSURE_REGISTERED (StreamServerApp);

TypeId
StreamServerApp::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::StreamServerApp")
    .SetParent<Application> ()
    .SetGroupName("Applications")
    .AddConstructor<StreamServerApp> ()
    .AddAttribute ("EnablePacing", 
                   "Enable Pacing",
                   BooleanValue (false),
                   MakeBooleanAccessor (&StreamServerApp::m_pacing),
                   MakeBooleanChecker ())
    .AddAttribute ("PacingGain", 
                   "Pacing gain to probe extra bandwidth",
                   DoubleValue (1.1),
                   MakeDoubleAccessor (&StreamServerApp::m_pacingGain),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("MaxPackets", 
                  "The maximum number of packets the application will send",
                   UintegerValue (100000),
                   MakeUintegerAccessor (&StreamServerApp::m_numPackets),
                   MakeUintegerChecker<int32_t> ())
      .AddAttribute ("ExtraRepair", 
                   "Extra fraction of repair packets to reduce in-order decoding delay",
                   DoubleValue (0.02),
                   MakeDoubleAccessor (&StreamServerApp::m_extraRepair),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("PeerAddress", 
                   "The destination Address of the outbound packets",
                   AddressValue (),
                   MakeAddressAccessor (&StreamServerApp::m_peerAddress),
                   MakeAddressChecker ())
	  .AddTraceSource ("Tx", "A new packet is created and is sent",
	                 MakeTraceSourceAccessor (&StreamServerApp::m_txTrace),
					        "ns3::Packet::TracedCallback")
	  .AddTraceSource ("Rx", "A packet has been received",
					        MakeTraceSourceAccessor (&StreamServerApp::m_rxTrace),
					        "ns3::Packet::TracedCallback")
	  .AddTraceSource ("TxWithAddresses", "A new packet is created and is sent",
					        MakeTraceSourceAccessor (&StreamServerApp::m_txTraceWithAddresses),						
					        "ns3::Packet::TwoAddressTracedCallback")																	
	  .AddTraceSource ("RxWithAddresses", "A packet has been received",								
					        MakeTraceSourceAccessor (&StreamServerApp::m_rxTraceWithAddresses),									
					        "ns3::Packet::TwoAddressTracedCallback")
  ;
  return tid;
}

StreamServerApp::StreamServerApp() 
 : m_minRttFilter(Seconds (10).GetMicroSeconds (), Time (0), 0),
   m_maxBwFilter(Seconds (10).GetMicroSeconds (), 0.0 , 0)
{
  srand(static_cast<uint32_t>(time(0)));
  // Construct network coding encoder and decoder
  m_cp.gfpower = 8;
  m_socket = 0;
  m_paramSendInterval = 0.01;
  m_paramAcked = false;

  m_initCWnd = 10;
  m_cWnd = m_initCWnd;
  m_pacingTimer = Timer(Timer::CANCEL_ON_DESTROY);
  m_pacingRate = DataRate ("100Mbps");
  m_pacingTimer.SetFunction (&StreamServerApp::NotifyPacingPerformed, this);
}

void
StreamServerApp::SetAttribute (Address clientAddr, int32_t numPackets, int32_t packetSize)
{
  m_peerAddress = clientAddr;
  m_numPackets = numPackets;
  m_packetSize = packetSize;
  m_cp.pktsize = packetSize;
  // A buffer containing random bytes
  int32_t datasize = m_packetSize * m_numPackets;
  m_buf = (unsigned char *) malloc(datasize);
  generate_buf(m_buf, datasize);
  if(m_buf == NULL) {
  	std::cout << "ERROR : client generate m_buf failure" << std::endl;
  }
  int32_t pktlen = sizeof(short) + 4 * sizeof(int) + m_cp.pktsize;     // 包括streamc报头的分组长度
  std::cout << "Datagram packet size: " << pktlen << " bytes " << std::endl;
}

void
StreamServerApp::SendParameter ()
{
  unsigned char *prmstr = serialize_parameter(&m_cp, m_numPackets);
  int32_t prmlen = sizeof(short)+ sizeof(int)*4 + sizeof(double);
  Ptr<Packet> prm = Create<Packet>(prmstr, prmlen);
  m_socket->Send(prm); 
  /*NS_LOG_UNCOND (
    "At time "
	  << Simulator::Now ().GetSeconds ()
	  << " (s) "
    << InetSocketAddress::ConvertFrom (m_peerAddress).GetIpv4 () 
	  << " server send parameters success "
  );*/

  if(prmstr != NULL) {
    free(prmstr);
	  prmstr = NULL;
  }
	
  if(!m_paramAcked) {
    Simulator::Schedule(Seconds(m_paramSendInterval), &StreamServerApp::SendParameter, this);
  }
}

int32_t 
StreamServerApp::ParamAckRecv (Ptr<Packet> prm)
{
  int32_t prmlen = sizeof(short) + 4 * sizeof(int) + sizeof(double);
  unsigned char *prmstr = (unsigned char *) calloc(prmlen, sizeof(unsigned char));
  prm->CopyData (prmstr, prmlen);
  unsigned char *prm_cp = serialize_ack_prm(&m_cp, m_numPackets);

  /*NS_LOG_UNCOND (
    "At time "
	<< Simulator::Now ().GetSeconds ()
	<< " (s) "
    << InetSocketAddress::ConvertFrom (m_peerAddress).GetIpv4 () 
	<< " server receive parameter ack success "
  );*/
  int judge = strcmp((const char *)prm_cp, (const char *)prmstr);

  if(prm_cp != NULL) {
    free(prm_cp);
	  prm_cp = NULL;
  }
  if(prmstr != NULL) {
    free(prmstr);
	  prmstr = NULL;
  }

  if(judge == 0) {
  	m_paramAcked = true;
    return 1;     //indicate receive parameters success
  } else {
    return 0;   //failure
  }

}

void
StreamServerApp::RttEstimation(double receive_time, double send_time)
{
  if(send_time == -1) {
    return ;
  }
  double alpha = 0.9;
  double historyRtt = m_sRtt;
  double new_rtt = receive_time - send_time;
  // std::cout << "Update new RTT: " << Seconds (new_rtt).GetMilliSeconds () << " to minRttFilter at " << Simulator::Now ().GetMicroSeconds () << std::endl;
  m_minRttFilter.Update (Seconds (new_rtt), Simulator::Now ().GetMicroSeconds ());
  if(historyRtt == 0) {
    m_sRtt = new_rtt;
  } else {
    m_sRtt = alpha * historyRtt + (1 - alpha) * new_rtt;     // smoothed RTT estimation (follow standard TCP, Karn's algorithm is not needed since no retransmission is incurred)
  }
  m_iRtt = new_rtt;
  return;
}

void
StreamServerApp::BwEstimationTibet(int32_t inorder, int32_t nsource, int32_t nrepair)
{
  // TCP TIBET 
  // 1) 平滑ACK间隔
  if (m_lastAckTime == m_dataStartTime) {
    // 首个data ack，无法计算间隔，不进行估计
    m_lastAckTime = Simulator::Now ().GetSeconds ();
    return;
  }
  double sampleInterval = Simulator::Now ().GetSeconds () - m_lastAckTime;
  double alpha = 0.9;
  if (m_avgAckInterval != 0) {
    m_avgAckInterval = alpha * m_avgAckInterval + (1 - alpha) * sampleInterval;
  } else {
    m_avgAckInterval = sampleInterval;
  }
  int numAcked = nsource + nrepair - m_lastAckedSourceNum - m_lastAckedRepairNum;
  double bwe = (double) numAcked / m_avgAckInterval;
  // 2） 自适应bwe滤波
  double T0 = 0.1;
  if (m_estBw == 0) {
    m_estBw = bwe;
  } else {
    m_estBw = (1 - exp (- sampleInterval / T0)) * bwe + exp (- sampleInterval / T0) * m_estBw;
  }
  // m_estBw = bwe;
  std::cout 
    << "[Client: " << InetSocketAddress::ConvertFrom (m_peerAddress).GetIpv4 ()  <<" ][ " <<Simulator::Now ().GetSeconds () << " (s) ]"
	  << "[TIBET-ABE] sampleInterval: " << sampleInterval << " averageAckInterval: " << m_avgAckInterval << " numAcked: " << numAcked << " bwe: " << bwe << " pkt./sec." 
    << " (Averaged) Estimated-BW: " << m_estBw << endl;
  m_lastAckTime = Simulator::Now ().GetSeconds ();
  m_lastBwEstTime = Simulator::Now ().GetSeconds ();
  return;
}

void
StreamServerApp::BwEstimationJersy(int32_t inorder, int32_t nsource, int32_t nrepair)
{
  // int numAcked = nsource + nrepair - m_lastAckedSourceNum - m_lastAckedRepairNum;
  // 1) 平滑ACK间隔
  double ackInterval = Simulator::Now ().GetSeconds () - m_lastAckTime;
  if (m_lastAckTime == m_dataStartTime) {
    // 首个data ack，无法计算间隔，不进行估计
    m_lastAckTime = Simulator::Now ().GetSeconds ();
    // m_estBw = m_newAcked / (m_lastAckTime - m_dataStartTime) * 2;
    return;
  }

  /*
  if (Simulator::Now ().GetSeconds () - m_dataStartTime < 2 * m_sRtt) {
    // 还未收到一个完整的RTT时间段内的数据
    m_estBw = (nsource + nrepair) / (Simulator::Now ().GetSeconds () - (m_dataStartTime + m_sRtt));
  } else {
    m_estBw = (m_sRtt * m_estBw + m_newAcked) / (ackInterval + m_sRtt);
  }
  */
  if (m_estBw == 0) {
    m_estBw = m_newAcked / ackInterval;
  } else {
    m_estBw = (m_sRtt * m_estBw + m_newAcked) / (ackInterval + m_sRtt);
  }
  m_maxBwFilter.Update(m_estBw, Simulator::Now ().GetMicroSeconds ());

  std::cout 
    << "[Client: " << InetSocketAddress::ConvertFrom (m_peerAddress).GetIpv4 ()  <<" ][ " <<Simulator::Now ().GetSeconds () << " (s) ]"
	  << "[Jersy-ABE] ackInterval: " << ackInterval << " numAcked: " << m_newAcked << " Estimated-BW: " << m_estBw << endl;
  m_lastAckTime = Simulator::Now ().GetSeconds ();
  m_lastBwEstTime = Simulator::Now ().GetSeconds ();
  return;
}

int32_t 
StreamServerApp::PeEstimation (int32_t nSource, int32_t nRepair)
{
  int32_t nLoss = nSource + nRepair - m_lastAckedSourceNum - m_lastAckedRepairNum;
  double alpha = 0.9;
  double new_lossRate = 1 - ((double) m_lastAckedSourceNum + m_lastAckedRepairNum) / (nSource + nRepair);
  if (m_lossRate == 0) {
    m_lossRate = new_lossRate;
  } else {
    m_lossRate = m_lossRate * alpha + new_lossRate * (1 - alpha);
  }
  return nLoss;
}

void 
StreamServerApp::UpdateCWnd (void)
{
  double cWndGain = 1.0;
  auto rtt_min=m_minRttFilter.GetBest();
  // m_cWnd = (int32_t) std::floor(std::max (double(m_initCWnd), m_estBw * rtt_min.GetSeconds () * cWndGain));
  m_cWnd = (int32_t) std::floor(std::max (double(m_initCWnd), m_maxBwFilter.GetBest () * rtt_min.GetSeconds () * cWndGain));
  // 更新pacing rate
  // DataRate pacingRate ((std::max (m_cWnd, m_packetsInFlight) * m_packetSize * 8 * gain) / m_rtt);
  if (m_estBw != 0 && m_pacing && m_cWnd > m_packetsInFlight) {
    DataRate pacingRate (m_estBw * 8 * m_packetSize * m_pacingGain);
    m_pacingRate = pacingRate;
    // NS_LOG_UNCOND ("Pacing rate updated to: " << pacingRate);
  }
  return;
}

void
StreamServerApp::PacketAckRecv(Ptr<Packet> packet)
{
  /*
  int32_t inodlen=sizeof(short)+sizeof(int)*3+sizeof(PacketInfo);
  unsigned char *inodstr = (unsigned char *) calloc(inodlen, sizeof(unsigned char));
  packet->CopyData (inodstr, inodlen);
  int inorder, nsource, nrepair;
  PacketInfo* pkt_info = deserialize_inorder_ack(inodstr, &inorder, &nsource, &nrepair);
  // 如果如下三个ACK信息与上次相同，则意味着接收端状态没有变化，则没有必要估计信息，也不要打印
  if (m_lastAckedInorderId==inorder && m_lastAckedSourceNum==nsource && m_lastAckedRepairNum==nrepair) {
    if(pkt_info != NULL) {
      free(pkt_info);
      pkt_info = NULL;
    }
    return;
  }
  */  
  int32_t inodlen=sizeof(short)+sizeof(int)*7;   // 7 is the numbers of int feedback from the client. We know it is ugly, but okay for quick development.
  unsigned char *inodstr = (unsigned char *) calloc(inodlen, sizeof(unsigned char));
  packet->CopyData (inodstr, inodlen);
  int inorder, nsource, nrepair;
  int ackPktType, ackPktId;
  int dwWidth;
  int dof;
  deserializeAckPacket(inodstr, &ackPktType, &ackPktId, &inorder, &nsource, &nrepair, &dwWidth, &dof);
  if (m_lastAckedInorderId==inorder && m_lastAckedSourceNum==nsource && m_lastAckedRepairNum==nrepair) {
    // Nothing changed, skip
    free(inodstr);
    return;
  }



  // 估算可用带宽(available bandwidth estimation)
  // BwEstimationTibet(inorder, nsource, nrepair);
  m_newAcked = nsource + nrepair - m_lastAckedRepairNum - m_lastAckedSourceNum;
  BwEstimationJersy(inorder, nsource, nrepair);

  m_lastAckedInorderId = inorder;
  m_lastAckedSourceNum = nsource;
  m_lastAckedRepairNum = nrepair;

  double recv_ack_time = double(Simulator::Now ().GetSeconds ());
  m_lastAckTime = recv_ack_time;
  /*
  SearchedInfo info = m_pktInfoQueue.find(pkt_info);
  double send_time = info.time;
  int other_type_packet_num = info.other_kind_packet_num;
  */
  PacketInfo info = m_pktInfoQueue.find(ackPktId, ackPktType);
  double send_time = info.m_time;
  int other_type_packet_num = info.m_another_packet_num;

  // 估计RTT
  RttEstimation(recv_ack_time, send_time);


  // 更新吞吐量观测值等其它相关变量
  // double dataDuration = double(Simulator::Now ().GetSeconds ()) - m_dataStartTime;        // 数据发送已经持续的时间

  // 估计丢包率
  /*
  int source_count = 0,repair_count = 0;    // 至发送客户端收到的最新分组的时刻，发送端共发出的两类分组数目
  std::string s;
  if(pkt_info->m_packet_type == SOURCE_PACKET) {
    s=std::string("SOURCE");
    source_count = pkt_info->m_id + 1;      // 注意分组从0开始编号
    repair_count = other_type_packet_num;
    m_lastAckedSourceId = pkt_info->m_id;
  } else {
    s=std::string("REPAIR");
    repair_count = pkt_info->m_id + 1;
    source_count = other_type_packet_num;
    m_lastAckedRepairId = pkt_info->m_id;
  }
  */
  int source_count = 0,repair_count = 0;    // Count the numbers of source and repair packets that had been sent up to the time sending the ACKed packet
  int32_t previousAckedSourceId = m_lastAckedSourceId;
  std::string s;
  if(ackPktType == SOURCE_PACKET) {
    s=std::string("SOURCE");
    source_count = ackPktId + 1;      // packets are indexed from 0
    repair_count = other_type_packet_num;
    m_lastAckedSourceId = ackPktId;
  } else {
    s=std::string("REPAIR");
    repair_count = ackPktId + 1;
    source_count = other_type_packet_num;
    m_lastAckedRepairId = ackPktId;
  }


  int32_t nTotalLoss = PeEstimation (source_count, repair_count);
  
  m_packetsInFlight = m_lastSentSourceId - m_lastAckedSourceId + m_lastSentRepairId - m_lastAckedRepairId;

  UpdateCWnd();

  std::cout 
    << "[Client: " << InetSocketAddress::ConvertFrom (m_peerAddress).GetIpv4 ()  <<" ][ " <<Simulator::Now ().GetSeconds () << " (s) ]"
	  << "[ACK] inorder: "
	  << m_lastAckedInorderId 
    << " latest received "
    << s
    << " packet: "
    << ackPktId
    << " total-received [nSource nRepair] = [ "
    << m_lastAckedSourceNum
    << " "
    << m_lastAckedRepairNum
    << " ]. "
    << std::endl;
  
  std::cout
    << "[Client: " << InetSocketAddress::ConvertFrom (m_peerAddress).GetIpv4 ()  <<" ][ " <<Simulator::Now ().GetSeconds () << " (s) ]"
	  << "[StatusUpdate] lastSentSourceId: "
    << m_lastSentSourceId
    << " lastSentRepairId: "
    << m_lastSentRepairId
    << " lastAckedSourceId: "
    << m_lastAckedSourceId
    << " lastAckedRepairId: "
    << m_lastAckedRepairId
    << " ACKed [inorder nsource nrepair] = [ "
    << inorder << " " << nsource << " " << nrepair << " ]"
    << std::endl;

  std::cout
    << "[Client: " << InetSocketAddress::ConvertFrom (m_peerAddress).GetIpv4 ()  <<" ][ " <<Simulator::Now ().GetSeconds () << " (s) ]"
	  << "[Estimation] BandWidth: " << m_estBw << " (pkts./sec.) smoothed-RTT : " << m_sRtt * 1000 
    << " (ms) LossRate: " << m_lossRate << " Current cWnd: " << m_cWnd << " (pkts.)" 
    << " packetsInFlight: " << m_packetsInFlight
    << " totalLoss: " << nTotalLoss
    << " filteredRttMin: " << m_minRttFilter.GetBest ().GetMilliSeconds () << " (ms) "
    << " instant-RTT: " << m_iRtt * 1000 << " (ms) "
    << std::endl;


  if (inorder >= 0 && inorder < m_numPackets - 1) {
  	flush_acked_packets(m_enc, inorder);
  }
  if (inorder >= m_numPackets - 1) {
    Simulator::ScheduleNow(&StreamServerApp::CompleteSend, this);
	  StopApplication();
  }	

  // Continue to send data packets if there is data and cWnd allows
  // if pacing, sending is controlled by pacingTimer
  if (m_cWnd > m_packetsInFlight) {
    Simulator::ScheduleNow(&StreamServerApp::SendDataPackets, this);
  }
}

void
StreamServerApp::NotifyPacingPerformed (void)
{
  SendDataPackets ();
}

void 
StreamServerApp::SendDataPackets (void)
{
  double curr_time = double(Simulator::Now ().GetSeconds ());
  while (m_cWnd > m_packetsInFlight) {
    // 如果开启了pacing，则检查pacing timer
    if (m_pacing && m_cWnd != m_initCWnd) {
      if (m_pacingTimer.IsRunning ()) {
            // NS_LOG_UNCOND ("Skipping Packet due to pacing" << m_pacingTimer.GetDelayLeft ());
            break;
      }
      // NS_LOG_UNCOND ("Timer is not running");
    }

    struct packet *cpkt = NULL;
    if (TimeToSendRepairPacket ()) {
      cpkt = output_repair_packet (m_enc);
      m_nSourceSinceLastRepair = 0;
    } else {
      cpkt = output_source_packet (m_enc);
      m_nSourceSinceLastRepair++;
    }
    // 序列化分组并发送
    int pktlen = sizeof(short) + 4 * sizeof(int) + m_cp.pktsize;
    if(cpkt != NULL) {
      unsigned char *str = serialize_packet(m_enc, cpkt);
      unsigned char *pktstr = add_packet_type(m_enc, str);
      
      Ptr<Packet> pkt = Create<Packet>(pktstr, pktlen);
      m_socket->Send(pkt);

      double send_time = double(Simulator::Now ().GetSeconds ());
      // 记录分组发送时刻和其它相应状态值
      if (cpkt->sourceid != -1) {
        m_pktInfoQueue.add(SOURCE_PACKET, cpkt->sourceid, send_time, m_enc->rcount, 0);
        m_lastSentSourceId = cpkt->sourceid;
        std::cout 
          << "[Client: " << InetSocketAddress::ConvertFrom (m_peerAddress).GetIpv4 ()  <<" ][ " << send_time << " (s) ]"
          << "[SendPacket] server sends SOURCE packet " << cpkt->sourceid << std::endl;
      } else {
        m_pktInfoQueue.add(REPAIR_PACKET, cpkt->repairid, send_time, m_enc->nextsid, 0);
        m_lastSentRepairId = cpkt->repairid;
        std::cout 
          << "[Client: " << InetSocketAddress::ConvertFrom (m_peerAddress).GetIpv4 ()  <<" ][ " << send_time << " (s) ]"
          << "[SendPacket] server sends REPAIR packet " << cpkt->repairid << std::endl;
      }
      free_packet(cpkt);
	    //free space
      if(pktstr != NULL) {
        free(pktstr);
        pktstr = NULL;
      }
      m_packetsInFlight++;
      // 根据pacing rate设定下一次发送时刻
      if (m_pacing) {
        if (m_pacingTimer.IsExpired ()) {
              // NS_LOG_UNCOND ("Current Pacing Rate " << m_pacingRate);
              // NS_LOG_UNCOND ("Timer is in expired state, activate it " << m_pacingRate.CalculateBytesTxTime (pkt->GetSize ()));
              m_pacingTimer.Schedule (m_pacingRate.CalculateBytesTxTime (pkt->GetSize ()));
              break;
        }
      }
    }
  }
}

// Currently is a function placeholder
bool
StreamServerApp::TimeToSendRepairPacket (void)
{
  if (m_lastSentSourceId == m_numPackets - 1) {
    return true;
  }
  double targetRepairFreq = m_lossRate + m_extraRepair;     // 修复分组目标插入频率，使得译码时延期望的均值为1/repairExcess个分组
  double currRepairFreq = (double) (m_lastSentRepairId + 1) / (m_lastSentSourceId + 1 + m_lastSentRepairId + 1);   // +1 is because ID starts from 0
  if (currRepairFreq < targetRepairFreq) {
    return true;
  }
  return false;
}

void 
StreamServerApp::CompleteSend (void)
{
  short type = COMPLETE;
  int pktlen = sizeof(short);
  unsigned char *pktstr = (unsigned char *)calloc(pktlen, sizeof(unsigned char));
  memcpy(pktstr, &type, sizeof(short));
  Ptr<Packet> stop_pkt = Create<Packet>(pktstr, pktlen);
  m_socket->Send(stop_pkt); 
}

void
StreamServerApp::StartApplication (void)
{
  if (m_socket == 0)
    {
      TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
      m_socket = Socket::CreateSocket (GetNode (), tid);
      if (InetSocketAddress::IsMatchingType(m_peerAddress) == true)
        {
          if (m_socket->Bind () == -1)
            {
              //NS_FATAL_ERROR ("Failed to bind socket");
			  std::cout << "Failed to bind socket" << std::endl; 
            }
		  if (m_socket->Connect (m_peerAddress) != 0)
		    {
			  std::cout << "server Failed to connect " << std::endl;
			}

        }
      else
        {
          //NS_ASSERT_MSG (false, "Incompatible address type: " << m_peerAddress);
		      std::cout << "Incompatible address type: " << m_peerAddress << std::endl;
        }

    }
  
	
  int datasize = m_cp.pktsize * m_numPackets;
  m_enc = initialize_encoder(&m_cp, m_buf, datasize);
  m_socket->SetRecvCallback (MakeCallback (&StreamServerApp::HandleRead, this));
  Simulator::ScheduleNow(&StreamServerApp::SendParameter, this);
}
  
void 
StreamServerApp::StopApplication (void)
{

  if (m_socket != 0) 
    {
      m_socket->Close ();
      m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
    }
  if (m_pacing) {
    m_pacingTimer.Cancel ();
  }
}

void
StreamServerApp::HandleRead (Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;
  Address localAddress;
  while ((packet = socket->RecvFrom (from)))
  {
      if (InetSocketAddress::IsMatchingType (from))
        {
          /*NS_LOG_UNCOND ("At time " << Simulator::Now ().GetSeconds () << "s server received " << packet->GetSize () << " bytes from " <<
                       InetSocketAddress::ConvertFrom (from).GetIpv4 () << " port " <<
                       InetSocketAddress::ConvertFrom (from).GetPort ());*/
        }
     
      socket->GetSockName (localAddress);
      m_rxTrace (packet);
      m_rxTraceWithAddresses (packet, from, localAddress);
      //判断packet类型 ＄1�71 ack类型
      //                3 inorder 
      int type_len=sizeof(short);
      unsigned char *type_str = (unsigned char *) calloc(type_len, sizeof(unsigned char));
      packet->CopyData(type_str, type_len);
      short type;
      memcpy(&type, type_str, sizeof(short));
      // 检查消息类型
      switch (type) {
        case ACK_PRM : {//与server不同的是 这个parameter 实际上是ack类型的1�7 用来判断server端接收到的参数是否正硄1�7
          if(!m_paramAcked && ParamAckRecv(packet)) {
            NS_LOG_UNCOND (
              "At time "
	          << Simulator::Now ().GetSeconds ()
			      << " (s) "
      		  << InetSocketAddress::ConvertFrom (m_peerAddress).GetIpv4 () 
	          << " server receive parameters ack correctly "
            );
            m_dataStartTime = Simulator::Now ().GetSeconds ();      // mark actual data start time
            m_lastAckTime = m_dataStartTime;
			      ///开始数据分组发送
            Simulator::ScheduleNow(&StreamServerApp::SendDataPackets, this);
          } else if(!m_paramAcked) {
            Simulator::ScheduleNow(&StreamServerApp::SendParameter, this);  
          }
        }
          break;
        case INORDER :
		      PacketAckRecv(packet);
          break;
        default : 
          NS_LOG_UNCOND (
            "At time "
	        << Simulator::Now ().GetSeconds ()
			    << " (s) "
			    << m_peerAddress 
	        << " server receive packet type error "
	      );
      }
  }
}

} // namespace ns3