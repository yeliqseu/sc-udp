#include <fstream>
#include <iostream>
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-dumbbell.h"
#include "ns3/point-to-point-module.h"
#include "ns3/point-to-point-layout-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/traffic-control-module.h"
#include "ns3/config-store-module.h"
#include "stream-server.h"   // libstreamc is written in C

extern "C" {
	#include "stream-client.h"
}
  

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MixedFlowComparison");

static void
CwndChange (std::string context, uint32_t oldCwnd, uint32_t newCwnd)
{
  std::cout << "At " << Simulator::Now ().GetSeconds () <<
            " CWND changes to " << newCwnd << " " << context << std::endl;
}

static void
BytesInFlightTracer (std::string context, uint32_t oldInFlight, uint32_t newInFlight)
{
  std::cout << "At " << Simulator::Now ().GetSeconds () <<
            " BytesInFlight changes to " << newInFlight << " " << context << std::endl;
}

void
PacketsInQueueTrace (std::string context, uint32_t oldVal, uint32_t newVal)
{
  NS_LOG_UNCOND ("Packets in queue changes from " << oldVal << " to " << newVal << " at " << Simulator::Now ().GetSeconds () << " << " << context);
}

static void
BulkSendTxTracer (std::string context, Ptr<const Packet> p)
{
  std::cout 
    << "[Client: " << "10.2." << stoi(context.substr(10,1))-1  <<".1 ][ " <<Simulator::Now ().GetSeconds () << " (s) ]"
	  << "[BulkSendApplication] send " << p->GetSize () << " bytes" << std::endl;
}

static void
SinkRxWithAddress (std::string path, Ptr<const Packet> p, const Address &peerAddress, const Address &localAddress)
{
  std::cout 
    << "[Client: " << InetSocketAddress::ConvertFrom (localAddress).GetIpv4 ()  <<" ][ " <<Simulator::Now ().GetSeconds () << " (s) ]"
	  << "[TcpPacketSink] receive: " << p->GetSize () << " bytes" << std::endl;
}

//QueueDiscDropTracer (Ptr<OutputStreamWrapper>stream, Ptr<const QueueDiscItem> item)
static void
QueueDiscDropTracer (std::string context, Ptr<const QueueDiscItem> item)
{
  Ptr<const Ipv4QueueDiscItem> ipItem = DynamicCast<const Ipv4QueueDiscItem> (item);
  const Ipv4Header ipHeader = ipItem->GetHeader ();     // QueueItem的ipv4地址需要从Ipv4QueueDiscItem中获得
  // 移除QueueItem的IPv4和TCP分组头，看看应用层分组的细节
  Ipv4Header ipv4Header;
  Ptr<Packet> pkt = item->GetPacket ();
  // std::cout<< "Packet Size from queue: " << pkt->GetSize () << std::endl;
  pkt->RemoveHeader (ipv4Header);
  if (int (ipHeader.GetProtocol ()) == 6)
    {
      TcpHeader tcpHeader;
      pkt->RemoveHeader (tcpHeader);
      std::cout 
        << "[Client: " << ipHeader.GetDestination ()  <<" ][ " <<Simulator::Now ().GetSeconds () << " (s) ]"
        << "[QueueDrop] Drop TCP packet of segment id: " << tcpHeader.GetSequenceNumber () << " " 
        << context
        << std::endl;
      return;
    }
  if (int (ipHeader.GetProtocol ()) == 17)
    {
      UdpHeader udpHeader;
      pkt->RemoveHeader (udpHeader);
      // std::cout<< "Packet Size from queue after removing headers: " << pkt->GetSize () << std::endl;
      unsigned char *pktstr = (unsigned char *) calloc(1500, sizeof(unsigned char));
      pkt->CopyData(pktstr, pkt->GetSize ());
      unsigned char *str = pktstr + sizeof(short);
      int sourceid, repairid;
      memcpy(&sourceid, str, sizeof(int));
      memcpy(&repairid, str+sizeof(int), sizeof(int));
      std::cout 
        << "[Client: " << ipHeader.GetDestination ()  <<" ][ " <<Simulator::Now ().GetSeconds () << " (s) ]"
        << "[QueueDrop] Drop SC-UDP packet of sourceid: " <<sourceid << " repairid: " << repairid << " " 
        << context
        << std::endl;
      return;
    }
}

static void
QueueDiscDropWithReason (std::string context, Ptr<const QueueDiscItem> item, const char *reason)
{
  Ptr<const Ipv4QueueDiscItem> ipItem = DynamicCast<const Ipv4QueueDiscItem> (item);
  const Ipv4Header ipHeader = ipItem->GetHeader ();     // QueueItem的ipv4地址需要从Ipv4QueueDiscItem中获得
  // 移除QueueItem的IPv4和TCP分组头，看看应用层分组的细节
  Ipv4Header ipv4Header;
  Ptr<Packet> pkt = item->GetPacket ();
  // std::cout<< "Packet Size from queue: " << pkt->GetSize () << std::endl;
  pkt->RemoveHeader (ipv4Header);
  UdpHeader udpHeader;
  pkt->RemoveHeader (udpHeader);
  std::cout 
    << "[Client: " << ipHeader.GetDestination ()  <<" ][ " <<Simulator::Now ().GetSeconds () << " (s) ]"
    << "[QueueDropWithReason] Drop packet of reason " 
    << context
    << std::endl;

}
static void
Ipv4L3Drop (std::string context, const Ipv4Header &ipHeader,
            Ptr<const Packet> p, Ipv4L3Protocol::DropReason reason,
            Ptr<Ipv4> ipv4, uint32_t interface)
{
  NS_LOG_UNCOND ("Ipv4L3Drop " << p->GetSize() << " bytes at " << Simulator::Now ().GetSeconds () << " << " << context);
}

static void
DeviceTxDrop (std::string context, Ptr<const Packet> p)
{
  NS_LOG_UNCOND ("DeviceDrop " << p->GetSize() << " bytes at " << Simulator::Now ().GetSeconds () << " << " << context);
}

static void
PhyRxDrop (std::string context, Ptr<const Packet> p)
{
  if (p->GetSize () < 536)
    {
      return;
    }
  Ptr<Packet> pkt = p->Copy ();
  pkt->Print(std::cout);
  PppHeader pppHeader;
  pkt->RemoveHeader (pppHeader);
  // pppHeader.Print (std::cout);
  Ipv4Header ipv4Header;
  pkt->RemoveHeader (ipv4Header);
  // std::cout << "Ipv4Header protocol: " << int(ipv4Header.GetProtocol ()) << std::endl;
  // ipv4Header.Print (std::cout);
  if (int (ipv4Header.GetProtocol ()) == 6)
    {
      TcpHeader tcpHeader;
      pkt->RemoveHeader (tcpHeader);
      std::cout 
        << "[Client: " << ipv4Header.GetDestination ()  <<" ][ " <<Simulator::Now ().GetSeconds () << " (s) ]"
        << "[PhyRxDrop] Drop TCP packet of segment id: " << tcpHeader.GetSequenceNumber () << " " 
        << context
        << std::endl;
      return;
    }
  if (int (ipv4Header.GetProtocol ()) == 17)
    {
      UdpHeader udpHeader;
      pkt->RemoveHeader (udpHeader);
      // 查看应用层分组的编码信息
      unsigned char *pktstr = (unsigned char *) calloc(1500, sizeof(unsigned char));
      pkt->CopyData(pktstr, pkt->GetSize ());
      unsigned char *str = pktstr + sizeof(short);
      int sourceid, repairid;
      memcpy(&sourceid, str, sizeof(int));
      memcpy(&repairid, str+sizeof(int), sizeof(int));
      std::cout 
        << "[Client: " << ipv4Header.GetDestination ()  <<" ][ " <<Simulator::Now ().GetSeconds () << " (s) ]"
        << "[PhyRxDrop] Drop SC-UDP packet of sourceid: " <<sourceid << " repairid: " << repairid << " " 
        << context
        << std::endl;
      return;
    }
  std::cout 
    << "[Client: " << ipv4Header.GetDestination ()  <<" ][ " <<Simulator::Now ().GetSeconds () << " (s) ]"
    << "[PhyRxDrop] Drop a packet with no TCP or UDP header "
    << context
    << std::endl;
  // NS_LOG_UNCOND ("PhyRxDrop " << p->GetSize() << " bytes at " << Simulator::Now ().GetSeconds () << " << " << context);
}

static void
IncreaseBtlkRate (void)
{
  Config::Set("/NodeList/0/DeviceList/0/$ns3::PointToPointNetDevice/DataRate", StringValue ("40Mbps"));
  Config::Set("/NodeList/1/DeviceList/0/$ns3::PointToPointNetDevice/DataRate", StringValue ("40Mbps"));
  return;
}

static void
DecreaseBtlkRate (void)
{
  Config::Set("/NodeList/0/DeviceList/0/$ns3::PointToPointNetDevice/DataRate", StringValue ("20Mbps"));
  Config::Set("/NodeList/1/DeviceList/0/$ns3::PointToPointNetDevice/DataRate", StringValue ("20Mbps"));
  return;
}

int
main (int argc, char *argv[])
{

  uint32_t mtu_bytes = 1500;
  int      nPackets = 1000;
  int      appPktSize = 1440;  // in bytes
  double   lossRate = 0.0;
  double   ackLossRate = 0.0;
  uint32_t queueDiscSize = 10240;
  uint32_t tcpBufAdu = 0;  // TCP's send and recv buffer size in the number of application data unit (if 0, use default 128k bytes)

  // TCP 业务流参数
  uint32_t nTcpLeaf = 1;
  std::string tcp_prot = "TcpCubic";
  // SCUDP 业务流参数
  uint32_t nScUdpLeaf = 0;
  double extraRepair = 0.02;
  bool isPacingEnabled = false;
  double pacingGain = 1.0;
  DataRate clientAckRate = DataRate("20Mbps");     // initial client ACK rate
  Time clientAckInterval (Seconds (1500 * 8 / static_cast<double> (clientAckRate.GetBitRate ())));
  std::string leafDelay = "25ms";
  std::string btlkDelay = "250ms";
  bool lowRttRef = false;
  bool randomStart = false;
  bool heterRtt = false;
  bool midChngRate = false;
  bool isFecEnabled = false;
  double   minRto = 1.0;
  // 从命令行读取参数
  CommandLine cmd;
  cmd.AddValue ("nPackets", "Number of source packets", nPackets);
  cmd.AddValue ("nTcpLeaf", "Number of leaves using TCP", nTcpLeaf);
  cmd.AddValue ("nScUdpLeaf", "Number of leaves using SC-UDP", nScUdpLeaf);
  cmd.AddValue ("tcpVariant", "Transport protocol to use: TcpNewReno, TcpLinuxReno, "
                "TcpHybla, TcpHighSpeed, TcpHtcp, TcpVegas, TcpScalable, TcpVeno, "
                "TcpBic, TcpYeah, TcpIllinois, TcpWestwood, TcpWestwoodPlus, TcpLedbat, "
		            "TcpLp, TcpDctcp, TcpCubic, TcpCopa, TcpCopa2, TcpBbr", tcp_prot);
  cmd.AddValue ("lossRate", "Link packet loss rate", lossRate);
  cmd.AddValue ("ackLossRate", "Link packet loss rate", ackLossRate);
  cmd.AddValue ("queueDiscSize", "Bottleneck queue disc size in packets", queueDiscSize);
  cmd.AddValue ("extraRepair", "Enable pacing", extraRepair);
  cmd.AddValue ("isPacingEnabled", "Enable pacing", isPacingEnabled);
  cmd.AddValue ("pacingGain", "Pacing gain", pacingGain);
  cmd.AddValue ("lowRttRef", "Simulate low-RTT (60ms) sceneario for reference", lowRttRef);
  cmd.AddValue ("tcpBufAdu", "TCP send and recv buffer size in the number of application data units", tcpBufAdu);
  cmd.AddValue ("randomStart", "Flows start at random time between 0 and 20s", randomStart);
  cmd.AddValue ("heterRtt", "RTT of the flows are different, where leaf RTprops are different", heterRtt);
  cmd.AddValue ("midChngRate", "Change bottleneck's link rate in the transmissions", midChngRate);
  cmd.AddValue ("tcpEnableFec", "Enable FEC for TCP" , isFecEnabled);
  cmd.AddValue ("minRto", "minimum RTO", minRto);
  cmd.Parse (argc, argv);

  if (lowRttRef) {
    // If set lowRttRef, change leaf and bottleneck links' propagation delay as 5ms and 20ms, respectively
    leafDelay = "5ms";
    btlkDelay = "20ms";
  }

  // 若前向链路丢包率非0而ackLossRate未设置，则将ackLossRate设为与前向链路一致
  if (lossRate != 0 && ackLossRate == 0) {
    ackLossRate = lossRate;
  }

  uint32_t dataBytes = nPackets * appPktSize;

  tcp_prot = std::string ("ns3::") + tcp_prot;
  // Select TCP variant
  if (tcp_prot.compare ("ns3::TcpWestwoodPlus") == 0)
    { 
      // TcpWestwoodPlus is not an actual TypeId name; we need TcpWestwood here
      Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpWestwood::GetTypeId ()));
      // the default protocol type in ns3::TcpWestwood is WESTWOOD
      Config::SetDefault ("ns3::TcpWestwood::ProtocolType", EnumValue (TcpWestwood::WESTWOODPLUS));
    }
  else
    {
      TypeId tcpTid;
      NS_ABORT_MSG_UNLESS (TypeId::LookupByNameFailSafe (tcp_prot, &tcpTid), "TypeId " << tcp_prot << " not found");
      Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TypeId::LookupByName (tcp_prot)));
    }

  if (tcp_prot.compare ("ns3::TcpBbr") == 0  ||
      tcp_prot.compare ("ns3::TcpCopa") == 0 ||
      tcp_prot.compare ("ns3::TcpCopa2") == 0)
    {
      Config::SetDefault ("ns3::TcpSocketState::EnablePacing", BooleanValue (true));      // These variants require ns-3's default TCP pacing to be turned on
    }
  // 计算MTU支持的TCP应用层数据包长度（Application Data Unit）
  Header* temp_header = new Ipv4Header ();
  uint32_t ip_header = temp_header->GetSerializedSize ();
  NS_LOG_UNCOND ("IP Header size is: " << ip_header);
  delete temp_header;
  temp_header = new TcpHeader ();
  uint32_t tcp_header = temp_header->GetSerializedSize ();
  NS_LOG_UNCOND ("TCP Header size is: " << tcp_header);
  delete temp_header;
  uint32_t tcp_adu_size = mtu_bytes - 20 - (ip_header + tcp_header);
  NS_LOG_UNCOND ("TCP ADU size is: " << tcp_adu_size);
  if (tcpBufAdu != 0) {
    // use customized SegmentSize, SndBufSize, RcvBufSize
    Config::SetDefault("ns3::TcpSocket::SegmentSize",UintegerValue(tcp_adu_size));
    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(tcpBufAdu*tcp_adu_size));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(tcpBufAdu*tcp_adu_size));
  }
  Config::SetDefault ("ns3::TcpSocketBase::EnableFec", BooleanValue (isFecEnabled));
  Config::SetDefault ("ns3::TcpSocketBase::MinRto", TimeValue (Seconds(minRto)));

  // 设置SC-UDP相关属性
  Config::SetDefault ("ns3::StreamServerApp::EnablePacing", BooleanValue (isPacingEnabled));
  Config::SetDefault ("ns3::StreamServerApp::PacingGain", DoubleValue (pacingGain));
  Config::SetDefault ("ns3::StreamServerApp::ExtraRepair", DoubleValue (extraRepair));

// Explicitly create the channels required by the topology (shown above).
  PointToPointHelper pointToPointRouter;
  pointToPointRouter.SetDeviceAttribute ("DataRate", StringValue ("20Mbps"));
  pointToPointRouter.SetChannelAttribute ("Delay", StringValue (btlkDelay));
  // pointToPointRouter.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue ("50p"));    // Left bottleneck node queue size (i.e., sending queue)


  PointToPointHelper pointToPointLeaf;
  pointToPointLeaf.SetDeviceAttribute ("DataRate", StringValue ("40Mbps"));
  pointToPointLeaf.SetChannelAttribute ("Delay", StringValue (leafDelay));

  uint32_t nLeftLeaf = nTcpLeaf + nScUdpLeaf;
  uint32_t nRightLeaf = nTcpLeaf + nScUdpLeaf; 

  PointToPointDumbbellHelper d (nLeftLeaf,pointToPointLeaf,
  								              nRightLeaf,pointToPointLeaf,
								                pointToPointRouter);

  // Packet loss on the forward link of the bottleneck
  Ptr<RateErrorModel> em = CreateObject<RateErrorModel> ();
  em->SetAttribute ("ErrorRate", DoubleValue (lossRate));
  em->SetAttribute ("ErrorUnit", EnumValue (RateErrorModel::ERROR_UNIT_PACKET));
  d.GetRight()->GetDevice (0)->SetAttribute ("ReceiveErrorModel", PointerValue (em));

  // Packet loss on the reverse link (ACK) of the bottleneck
  Ptr<RateErrorModel> ackEm = CreateObject<RateErrorModel> ();
  ackEm->SetAttribute ("ErrorRate", DoubleValue (ackLossRate));
  ackEm->SetAttribute ("ErrorUnit", EnumValue (RateErrorModel::ERROR_UNIT_PACKET));
  d.GetLeft()->GetDevice (0)->SetAttribute ("ReceiveErrorModel", PointerValue (ackEm));

  // Change left leaf channel's propagation time if heterRtt is set
  if (heterRtt == true)
    {
      for (int i=0; i<nLeftLeaf; i++) {
        std::string delay = std::to_string((i+1) * 20) + "ms";
        d.GetLeft (i)->GetDevice (0)->GetChannel ()->SetAttribute ("Delay", StringValue(delay));
      }
    }

  InternetStackHelper stack;
  d.InstallStack (stack);

  // Bottleneck link traffic control configuration
  // 若要设置与默认(FqCoDel)不同的流量控制。操作必须在InstallStack()之后，但在Assign Address之前。
  // [参见Model Library文档：Traffic Control Layer]
  // Config::SetDefault ("ns3::FqCoDelQueueDisc::MaxSize", QueueSizeValue (QueueSize (QueueSizeUnit::PACKETS, queueDiscSize)));

  
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/MacTxDrop", MakeCallback (&DeviceTxDrop));
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/PhyTxDrop", MakeCallback (&DeviceTxDrop));
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/PhyRxDrop", MakeCallback (&PhyRxDrop));
  //Config::Connect ("/NodeList/0/DeviceList/*/$ns3::PointToPointNetDevice/TxQueue/PacketsInQueue", MakeCallback (&PacketsInQueueTrace));
//
// We've got the "hardware" in place.  Now we need to add IP addresses.
//
  NS_LOG_UNCOND ("Assign IP Addresses.");


  uint16_t clientPort = 8080;

  d.AssignIpv4Addresses (Ipv4AddressHelper ("10.1.1.0", "255.255.255.0"),
  					             Ipv4AddressHelper ("10.2.1.0", "255.255.255.0"),
					               Ipv4AddressHelper ("10.3.1.0", "255.255.255.0"));

  // 连接默认的qdisc则要在Assign Address之后[参见Model Library：Traffic Control Layer]
  Config::Connect("/NodeList/*/$ns3::TrafficControlLayer/RootQueueDiscList/0/Drop", MakeCallback (&QueueDiscDropTracer));
  // Config::Connect("/NodeList/*/$ns3::TrafficControlLayer/RootQueueDiscList/0/Requeue", MakeCallback (&QueueDiscDropTracer));
  // Config::Connect("/NodeList/*/$ns3::TrafficControlLayer/RootQueueDiscList/0/DropBeforeEnqueue", MakeCallback (&QueueDiscDropWithReason));
  // Config::Connect("/NodeList/*/$ns3::TrafficControlLayer/RootQueueDiscList/0/DropAfterDequeue", MakeCallback (&QueueDiscDropWithReason));
  // Config::Connect("/NodeList/0/$ns3::TrafficControlLayer/RootQueueDiscList/0/PacketsInQueue", MakeCallback (&PacketsInQueueTrace));

  NS_LOG_UNCOND ("Create Applications.");
//
// Create one udpServer applications on node one.
//
  ApplicationContainer serverApps;
  ApplicationContainer clientApps;
 
  NodeContainer nodes;
  // Jitter start
  double min=0.0;
  double max=20.0;
  Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable> ();
  x->SetAttribute ("Min", DoubleValue (min));
  x->SetAttribute ("Max", DoubleValue (max));

  // 安装远端TCP节点的PacketSink应用
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), clientPort));
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
  for (uint32_t i = 0; i < nTcpLeaf; ++i)
    {
      AddressValue remoteAddress (InetSocketAddress (d.GetRightIpv4Address (i), clientPort));
      BulkSendHelper bulkHelper ("ns3::TcpSocketFactory", Address ());
      bulkHelper.SetAttribute ("Remote", remoteAddress);
      bulkHelper.SetAttribute ("SendSize", UintegerValue (tcp_adu_size));
      bulkHelper.SetAttribute ("MaxBytes", UintegerValue (dataBytes));

      ApplicationContainer bulkServer = bulkHelper.Install (d.GetLeft (i));
      serverApps.Add (bulkServer.Get (0));

      sinkHelper.SetAttribute ("Protocol", TypeIdValue (TcpSocketFactory::GetTypeId ()));
      ApplicationContainer sinkClient = sinkHelper.Install (d.GetRight (i));
      clientApps.Add (sinkClient.Get (0));

      std::ostringstream oss;
      oss << "/NodeList/"
          << d.GetRight (i)->GetId ()
          << "/ApplicationList/*/$ns3::PacketSink/RxWithAddresses";
      Config::Connect (oss.str (), MakeCallback (&SinkRxWithAddress));

      std::ostringstream oss2;
      oss2 << "/NodeList/"
          << d.GetLeft (i)->GetId ()
          << "/ApplicationList/*/$ns3::BulkSendApplication/Tx";
      Config::Connect (oss2.str (), MakeCallback (&BulkSendTxTracer));

      nodes.Add(d.GetRight (i));
      nodes.Add(d.GetLeft (i));
    }
  
  // 剩余节点安装SC-UDP应用
  for(uint32_t i = nTcpLeaf; i < nTcpLeaf + nScUdpLeaf; ++i)
  	{
  	  Ptr<Application> app;
  	  Ptr<StreamClientApp> client = CreateObject<StreamClientApp> ();
	    Address clientAddress = InetSocketAddress (d.GetRightIpv4Address (i), clientPort);
	    client->Setup(clientAddress, clientAckInterval);
	    d.GetRight (i)->AddApplication (client);
      //client->SetStartTime (Seconds (delay_time));
	    app = client;
	    clientApps.Add (app);

  	  Ptr<StreamServerApp> server = CreateObject<StreamServerApp> ();
  	  server->SetAttribute(clientAddress, nPackets, appPktSize);
	    d.GetLeft (i)->AddApplication (server);
      //server->SetStartTime (Seconds (delay_time));

      nodes.Add(d.GetRight (i));
      nodes.Add(d.GetLeft (i));
	    app = server;
	    serverApps.Add (app);

      std::ostringstream oss;
      oss << "/NodeList/"
          << d.GetLeft(i)->GetId ()
          << "/$ns3::Ipv4L3Protocol/Drop";
      Config::Connect (oss.str (), MakeCallback (&Ipv4L3Drop));
    } 
	
  if (randomStart) 
    {
      serverApps.StartWithJitter (Seconds (0.0), x);
    } 
  else 
    {
      serverApps.Start (Seconds (0.0));
    }
  clientApps.Start (Seconds (0.0));
  Ptr<FlowMonitor> flowMonitor;
  FlowMonitorHelper flowHelper;
  flowMonitor = flowHelper.Install(nodes);

  if (nTcpLeaf != 0) {
    Simulator::Schedule (Seconds (0.1), Config::Connect,"/NodeList/2/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow",MakeCallback (&CwndChange));
    Simulator::Schedule (Seconds (0.1), Config::Connect,"/NodeList/2/$ns3::TcpL4Protocol/SocketList/0/BytesInFlight",MakeCallback (&BytesInFlightTracer));
  }
  if (midChngRate == true) {
    Simulator::Schedule (Seconds (10), &IncreaseBtlkRate );
  }

	
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

//
// Now, do the actual simulation.
//
  NS_LOG_UNCOND ("Run Simulation.");
  Simulator::Stop (Seconds(4000.0));

  // Output default attributes
  Config::SetDefault ("ns3::ConfigStore::Filename", StringValue ("mixed-flows-attributes.xml"));
  Config::SetDefault ("ns3::ConfigStore::Mode", StringValue ("Save"));
  ConfigStore outputConfig;
  outputConfig.ConfigureAttributes ();

  Simulator::Run ();
  //Simulator::Destroy ();
  NS_LOG_UNCOND ("Done.");

  flowMonitor->SerializeToXmlFile("mixed-flows-monitor-results.xml", true, true);
}
