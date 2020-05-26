#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"

#include <sstream>
#include <map>
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("DctcpTest");

uint32_t checkTimes;
double avgQueueSize;

// attributes
std::string link_data_rate;
std::string link_delay;
uint32_t packet_size;
uint32_t queue_size;
uint32_t threhold;

// nodes
NodeContainer nodes;
NodeContainer clients;
NodeContainer switchs;
NodeContainer servers;

// server interfaces
Ipv4InterfaceContainer serverInterfaces;

// receive status
std::map<uint32_t, uint64_t> totalRx;   //fId->receivedBytes
std::map<uint32_t, uint64_t> lastRx;

// throughput result
std::map<uint32_t, std::vector<std::pair<double, double> > > throughputResult; //fId -> list<time, throughput>

QueueDiscContainer queueDiscs;

std::stringstream filePlotQueue;
std::stringstream filePlotQueueAvg;

void
CheckQueueSize (Ptr<QueueDisc> queue)
{
  uint32_t qSize = StaticCast<RedQueueDisc> (queue)->GetQueueSize ();

  avgQueueSize += qSize;
  checkTimes++;

  // check queue size every 1/100 of a second
  Simulator::Schedule (Seconds (0.01), &CheckQueueSize, queue);

  std::ofstream fPlotQueue (filePlotQueue.str ().c_str (), std::ios::out|std::ios::app);
  fPlotQueue << Simulator::Now ().GetSeconds () << " " << qSize << std::endl;
  fPlotQueue.close ();

  std::ofstream fPlotQueueAvg (filePlotQueueAvg.str ().c_str (), std::ios::out|std::ios::app);
  fPlotQueueAvg << Simulator::Now ().GetSeconds () << " " << avgQueueSize / checkTimes << std::endl;
  fPlotQueueAvg.close ();
}

void
TxTrace (uint32_t flowId, Ptr<const Packet> p)
{
  NS_LOG_FUNCTION (flowId << p);
  FlowIdTag flowIdTag;
  flowIdTag.SetFlowId (flowId);
  p->AddByteTag (flowIdTag);
}

void
RxTrace (Ptr<const Packet> packet, const Address &from)
{
  NS_LOG_FUNCTION (packet << from);
  FlowIdTag flowIdTag;
  bool retval = packet->FindFirstMatchingByteTag (flowIdTag);
  NS_ASSERT (retval);
  if (totalRx.find (flowIdTag.GetFlowId ()) != totalRx.end ())
    {
      totalRx[flowIdTag.GetFlowId ()] += packet->GetSize ();
    }
  else
    {
      totalRx[flowIdTag.GetFlowId ()] = packet->GetSize ();
      lastRx[flowIdTag.GetFlowId ()] = 0;
    }
}

void
CalculateThroughput (void)
{
  for (auto it = totalRx.begin (); it != totalRx.end (); ++it)
    {
      double cur = (it->second - lastRx[it->first]) * (double) 8/1e5; /* Convert Application RX Packets to MBits. */
      throughputResult[it->first].push_back (std::pair<double, double> (Simulator::Now ().GetSeconds (), cur));
      lastRx[it->first] = it->second;
    }
  Simulator::Schedule (MilliSeconds (100), &CalculateThroughput);
}

void
SetName (void)
{
  // add name to clients
  int i = 0;
  for(auto it = clients.Begin (); it != clients.End (); ++it)
    {
      std::stringstream ss;
      ss << "CL" << i++;
      Names::Add (ss.str (), *it);
    }
  i = 0;
  for(auto it = switchs.Begin (); it != switchs.End (); ++it)
    {
      std::stringstream ss;
      ss << "SW" << i++;
      Names::Add (ss.str (), *it);
    }
  i = 0;
  for(auto it = servers.Begin (); it != servers.End (); ++it)
    {
      std::stringstream ss;
      ss << "SE" << i++;
      Names::Add (ss.str (), *it);
    }
}

void
SetConfig (bool useEcn, bool useAtp)
{

  GlobalValue::Bind ("ChecksumEnabled", BooleanValue (false));
  // RED params
  NS_LOG_INFO ("Set RED params");
  Config::SetDefault ("ns3::RedQueueDisc::Mode", StringValue ("QUEUE_MODE_PACKETS"));
  Config::SetDefault ("ns3::RedQueueDisc::MeanPktSize", UintegerValue (packet_size));
  // Config::SetDefault ("ns3::RedQueueDisc::Gentle", BooleanValue (false));
  // Config::SetDefault ("ns3::RedQueueDisc::QW", DoubleValue (1.0));
  Config::SetDefault ("ns3::RedQueueDisc::UseMarkP", BooleanValue (true));
  Config::SetDefault ("ns3::RedQueueDisc::MarkP", DoubleValue (2.0));
  Config::SetDefault ("ns3::RedQueueDisc::MinTh", DoubleValue (threhold));
  Config::SetDefault ("ns3::RedQueueDisc::MaxTh", DoubleValue (threhold));
  Config::SetDefault ("ns3::RedQueueDisc::QueueLimit", UintegerValue (queue_size));

  // TCP params
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (packet_size));
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpNewReno"));
  Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue (1));
  if (useEcn)
    {
      Config::SetDefault ("ns3::TcpSocketBase::UseEcn", BooleanValue (true));
      Config::SetDefault ("ns3::RedQueueDisc::UseEcn", BooleanValue (true));
      if (useAtp)
        {
          Config::SetDefault ("ns3::TcpL4Protocol::SocketBaseType", TypeIdValue(TypeId::LookupByName ("ns3::DctcpSocket")));
          Config::SetDefault ("ns3::DctcpSocket::DctcpWeight", DoubleValue (1.0 / 16));
        }
    }
}

int
main (int argc, char *argv[])
{

    // LogComponentEnable ("DctcpSocket", LOG_LEVEL_INFO);
    bool useEcn = 1;              // 使用ECN
    bool useAtp = 1;              // 使用ATP
    std::string pathOut;          // 输出路径
    bool writePcap = 1;           // 输出pcap

    link_data_rate = "100Mbps";
    link_delay = "10ms";
    packet_size = 512;
    queue_size = 1024;
    threhold = 20;

    // Will only save in the directory if enable opts below
    CommandLine cmd;
    cmd.Parse (argc, argv);

    SetConfig (useEcn, useAtp);

    // 1-1、初始化client个数、switch个数、server个数
    NS_LOG_INFO ("Create nodes");
    clients.Create (3);
    switchs.Create (1);
    servers.Create (1);

    SetName ();

    // 1-2、把client switch server装入InternetStackHelper中
    NS_LOG_INFO ("Install internet stack on all nodes.");
    InternetStackHelper internet;
    internet.Install (clients);
    internet.Install (switchs);
    internet.Install (servers);

    // 1-3、初始化TrafficContronHelper fifo和red两种队列
    TrafficControlHelper tchPfifo;
    // use default limit for pfifo (1000)
    uint16_t handle = tchPfifo.SetRootQueueDisc ("ns3::PfifoFastQueueDisc", "Limit", UintegerValue (queue_size));
    tchPfifo.AddInternalQueues (handle, 3, "ns3::DropTailQueue", "MaxPackets", UintegerValue (queue_size));

    TrafficControlHelper tchRed;
    tchRed.SetRootQueueDisc ("ns3::RedQueueDisc", "LinkBandwidth", StringValue (link_data_rate),
                            "LinkDelay", StringValue (link_delay));


    // 1-4、初始化p2p helper（channel）
    NS_LOG_INFO ("Create channels");
    PointToPointHelper p2p;

    p2p.SetQueue ("ns3::DropTailQueue");
    p2p.SetDeviceAttribute ("DataRate", StringValue (link_data_rate));
    p2p.SetChannelAttribute ("Delay", StringValue (link_delay));


    // 1-5、初始化Ipv4AddressHelper
    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.1.1.0", "255.255.255.0");

    // 1-6、把每个client和switch0连起来
    for (auto it = clients.Begin (); it != clients.End (); ++it)
    {
      NetDeviceContainer devs = p2p.Install (NodeContainer (*it, switchs.Get (0)));
      tchPfifo.Install (devs);
      ipv4.Assign (devs);
      ipv4.NewNetwork ();
    }
    // 1-7、把switch1和server连起来
    for (auto it = servers.Begin (); it != servers.End (); ++it)
    {
      NetDeviceContainer devs = p2p.Install (NodeContainer (switchs.Get (0), *it));
      tchPfifo.Install (devs);
      serverInterfaces.Add (ipv4.Assign (devs).Get (1));
      ipv4.NewNetwork ();
    }
    // Set up the routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    // SINK is in the right side
    uint16_t port = 50000;
    Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
    PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = sinkHelper.Install (servers.Get (0));
    sinkApp.Start (Seconds (1.0));
    sinkApp.Stop (Seconds (11.0));
    sinkApp.Get (0)->TraceConnectWithoutContext ("Rx", MakeCallback (&RxTrace));


    BulkSendHelper clientHelper ("ns3::TcpSocketFactory", InetSocketAddress (serverInterfaces.GetAddress (0), port));
    clientHelper.SetAttribute ("SendSize", UintegerValue (512));

    ApplicationContainer clientApps = clientHelper.Install (clients);

    // set different start/stop time for each app
    double clientStartTime = 1.0;
    double clientStopTime = 10.0;
    uint32_t i = 1;
    for (auto it = clientApps.Begin (); it != clientApps.End (); ++it)
        {
        Ptr<Application> app = *it;
        //app->SetAttribute
        // 100Mbps一秒钟能传25600个512B的报文
        app->SetAttribute ("MaxBytes", UintegerValue(512*25600*2));//一个客户端占骨干2s，三个占6s
        app->SetStartTime (Seconds (clientStartTime));
        app->SetStopTime (Seconds (clientStopTime));
        app->TraceConnectWithoutContext ("Tx", MakeBoundCallback (&TxTrace, i++));
    }

    std::cout << "--------" << std::endl;
    if (writePcap)
        {
        PointToPointHelper ptp;
        std::stringstream stmp;
        stmp << "./final_res_2" << "/test2";
        ptp.EnablePcapAll (stmp.str ());
        }

   
    Simulator::Stop (Seconds (11.0));
    std::cout << "begin running ... ..." << std::endl;
    Simulator::Run ();
    std::cout << "finished" << std::endl;
    
    Simulator::Destroy ();

    return 0;
}
