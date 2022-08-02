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

NS_LOG_COMPONENT_DEFINE ("StreamClientServerExample");

void
BytesInQueueTrace (Ptr<OutputStreamWrapper> stream, uint32_t oldVal, uint32_t newVal)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds () << " " << newVal << std::endl;
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
  pkt->RemoveHeader (ipv4Header);
  UdpHeader udpHeader;
  pkt->RemoveHeader (udpHeader);
  unsigned char *pktstr = (unsigned char *) calloc(1500, sizeof(unsigned char));
  pkt->CopyData(pktstr, pkt->GetSize ());
  unsigned char *str = pktstr + sizeof(short);
  int sourceid, repairid;
  memcpy(&sourceid, str, sizeof(int));
  memcpy(&repairid, str+sizeof(int), sizeof(int));
  std::cout 
    << "[Client: " << ipHeader.GetDestination ()  <<" ][ " <<Simulator::Now ().GetSeconds () << " (s) ]"
    << "[QueueDrop] Drop packet of sourceid: " <<sourceid << " repairid: " << repairid << " " 
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
  Ptr<Packet> pkt = p->Copy ();
  pkt->Print(std::cout);
  PppHeader pppHeader;
  pkt->RemoveHeader (pppHeader);
  // pppHeader.Print (std::cout);
  Ipv4Header ipv4Header;
  pkt->RemoveHeader (ipv4Header);
  // ipv4Header.Print (std::cout);
  UdpHeader udpHeader;
  pkt->RemoveHeader (udpHeader);
  // 查看应用层分组的编码信息
  if (pkt->GetSize () < 1400) {
    return;                         // 忽略非数据分组丢失的信息打印
  }
  unsigned char *pktstr = (unsigned char *) calloc(1500, sizeof(unsigned char));
  pkt->CopyData(pktstr, pkt->GetSize ());
  unsigned char *str = pktstr + sizeof(short);
  int sourceid, repairid;
  memcpy(&sourceid, str, sizeof(int));
  memcpy(&repairid, str+sizeof(int), sizeof(int));
  std::cout 
    << "[Client: " << ipv4Header.GetDestination ()  <<" ][ " <<Simulator::Now ().GetSeconds () << " (s) ]"
    << "[PhyRxDrop] Drop packet of sourceid: " <<sourceid << " repairid: " << repairid << " " 
    << context
    << std::endl;
  // NS_LOG_UNCOND ("PhyRxDrop " << p->GetSize() << " bytes at " << Simulator::Now ().GetSeconds () << " << " << context);
}

static void
ResetLinkBandwidth (void)
{
  Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable> ();
  std::string rate = std::to_string(x->GetInteger(10, 100)) + "Mbps";
  Config::Set("/NodeList/0/DeviceList/0/$ns3::PointToPointNetDevice/DataRate", StringValue (rate));
  Config::Set("/NodeList/1/DeviceList/0/$ns3::PointToPointNetDevice/DataRate", StringValue (rate));
  return;
}

static void
ResetLinkDelay (void)
{
  Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable> ();
  std::string delay = std::to_string(x->GetInteger(40, 300)) + "ms";
  Config::Set("/ChannelList/0/$ns3::PointToPointChannel/Delay", StringValue (delay));
  return;
}

static void
ResetLinkLossRate (void)
{
  Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable> ();
  uint32_t y = x->GetInteger(0, 10);
  double pe = ((double) y) / 1000.0;     // pe in 0-0.01
  Config::Set("/NodeList/1/$ns3::Node/DeviceList/0/$ns3::PointToPointNetDevice/ReceiveErrorModel/$ns3::RateErrorModel/ErrorRate", DoubleValue (pe));
  return;
}

int
main (int argc, char *argv[])
{
//
// Enable logging for UdpClient and
//

  int      nPackets = 1000;
  uint32_t nLeaf = 1;
  double   lossRate = 0.0;
  double   ackLossRate = 0.0;
  uint32_t queueDiscSize = 1024;
  double extraRepair = 0.02;
  bool isPacingEnabled = false;
  double pacingGain = 1.1;

  int appPktSize = 1440;  // in bytes

  DataRate clientAckRate = DataRate("100Mbps");     // initial client ACK rate
  Time clientAckInterval (Seconds (1500 * 8 / static_cast<double> (clientAckRate.GetBitRate ())));


  CommandLine cmd;
  cmd.AddValue ("nPackets", "Number of source packets", nPackets);
  cmd.AddValue ("nLeaf", "Number of leaves", nLeaf);
  cmd.AddValue ("lossRate", "Link packet loss rate", lossRate);
  cmd.AddValue ("ackLossRate", "Link packet loss rate", ackLossRate);
  cmd.AddValue ("queueDiscSize", "Bottleneck queue disc size in packets", queueDiscSize);
  cmd.AddValue ("extraRepair", "Enable pacing", extraRepair);
  cmd.AddValue ("isPacingEnabled", "Enable pacing", isPacingEnabled);
  cmd.AddValue ("pacingGain", "Pacing gain", pacingGain);
  cmd.Parse (argc, argv);

  Config::SetDefault ("ns3::StreamServerApp::EnablePacing", BooleanValue (isPacingEnabled));
  Config::SetDefault ("ns3::StreamServerApp::PacingGain", DoubleValue (pacingGain));
  Config::SetDefault ("ns3::StreamServerApp::ExtraRepair", DoubleValue (extraRepair));

// Explicitly create the channels required by the topology (shown above).
  PointToPointHelper pointToPointRouter;
  pointToPointRouter.SetDeviceAttribute ("DataRate", StringValue ("20Mbps"));
  pointToPointRouter.SetChannelAttribute ("Delay", StringValue ("250ms"));
  // pointToPointRouter.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue ("50p"));    // Left bottleneck node queue size (i.e., sending queue)


  PointToPointHelper pointToPointLeaf;
  pointToPointLeaf.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  pointToPointLeaf.SetChannelAttribute ("Delay", StringValue ("25ms"));

  uint32_t nLeftLeaf = nLeaf;
  uint32_t nRightLeaf = nLeaf; 

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

  InternetStackHelper stack;
  d.InstallStack (stack);

  // Bottleneck link traffic control configuration
  // 若要设置与默认(FqCoDel)不同的流量控制。操作必须在InstallStack()之后，但在Assign Address之前。
  // [参见Model Library文档：Traffic Control Layer]
  Config::SetDefault ("ns3::FqCoDelQueueDisc::MaxSize", QueueSizeValue (QueueSize (QueueSizeUnit::PACKETS, queueDiscSize)));

  
  // uint32_t queueDiscSize = 1000;
  // TrafficControlHelper tchBottleneck;
  // tchBottleneck.SetRootQueueDisc ("ns3::FqCoDelQueueDisc");
  // Config::SetDefault ("ns3::FqCoDelQueueDisc::MaxSize",
  //                     QueueSizeValue (QueueSize (QueueSizeUnit::PACKETS, queueDiscSize)));
  // NetDeviceContainer devicesBottleneckLink;
  // devicesBottleneckLink.Add(d.GetLeft()->GetDevice(0));
  // devicesBottleneckLink.Add(d.GetRight()->GetDevice(0));
  // QueueDiscContainer qdiscs;
  // qdiscs = tchBottleneck.Install (devicesBottleneckLink);
  // Ptr<QueueDisc> qdiscLeft = qdiscs.Get(0);

  // AsciiTraceHelper ascii;
  // //Ptr<Queue<Packet> > queue = StaticCast<PointToPointNetDevice> (devicesBottleneckLink.Get (0))->GetQueue ();
  // Ptr<OutputStreamWrapper> streamBytesInQueue = ascii.CreateFileStream ("FqCoDel-bytesInQueue.txt");
  // //queue->TraceConnectWithoutContext ("BytesInQueue", MakeBoundCallback (&BytesInQueueTrace, streamBytesInQueue));
  // //queue->TraceConnectWithoutContext ("Drop", MakeBoundCallback (&DropTrace, streamBytesInQueue));
  // qdiscLeft->TraceConnectWithoutContext ("Drop", MakeBoundCallback (&QueueDiscDropTracer, streamBytesInQueue));

  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/MacTxDrop", MakeCallback (&DeviceTxDrop));
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/PhyTxDrop", MakeCallback (&DeviceTxDrop));
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/PhyRxDrop", MakeCallback (&PhyRxDrop));
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

  for(uint32_t i = 0; i < d.LeftCount (); ++i)
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
  
	
  serverApps.Start (Seconds (0.0));
  //serverApps.StartWithJitter (Seconds (0.0), x);
  clientApps.Start (Seconds (0.0));
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Now, do the actual simulation.
  // Set link initial conditions to random values and reset them at a random time later
  ResetLinkBandwidth ();
  ResetLinkDelay ();
  ResetLinkLossRate ();
  Simulator::Schedule (Seconds (x->GetInteger(10, 50)), &ResetLinkBandwidth );
  Simulator::Schedule (Seconds (x->GetInteger(10, 50)), &ResetLinkDelay );
  Simulator::Schedule (Seconds (x->GetInteger(10, 50)), &ResetLinkLossRate );
  NS_LOG_UNCOND ("Run Simulation.");
  Simulator::Stop (Seconds(3000.0));

  // Output default attributes
  Config::SetDefault ("ns3::ConfigStore::Filename", StringValue ("scudp-output-attributes.xml"));
  Config::SetDefault ("ns3::ConfigStore::Mode", StringValue ("Save"));
  ConfigStore outputConfig;
  outputConfig.ConfigureAttributes ();

  Simulator::Run ();
  //Simulator::Destroy ();
  NS_LOG_UNCOND ("Done.");

}
