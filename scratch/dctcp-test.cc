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

// The times
double global_start_time;
double global_stop_time;
double sink_start_time;
double sink_stop_time;
double client_start_time;
double client_stop_time;
double client_interval_time;

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
BuildTopo (uint32_t clientNo, uint32_t serverNo)
{
  NS_LOG_INFO ("Create nodes");
  clients.Create (clientNo);
  switchs.Create (2);
  servers.Create (serverNo);

  SetName ();

  NS_LOG_INFO ("Install internet stack on all nodes.");
  InternetStackHelper internet;
  internet.Install (clients);
  internet.Install (switchs);
  internet.Install (servers);


  TrafficControlHelper tchPfifo;
  // use default limit for pfifo (1000)
  uint16_t handle = tchPfifo.SetRootQueueDisc ("ns3::PfifoFastQueueDisc", "Limit", UintegerValue (queue_size));
  tchPfifo.AddInternalQueues (handle, 3, "ns3::DropTailQueue", "MaxPackets", UintegerValue (queue_size));

  TrafficControlHelper tchRed;
  tchRed.SetRootQueueDisc ("ns3::RedQueueDisc", "LinkBandwidth", StringValue (link_data_rate),
                           "LinkDelay", StringValue (link_delay));

  NS_LOG_INFO ("Create channels");
  PointToPointHelper p2p;

  p2p.SetQueue ("ns3::DropTailQueue");
  p2p.SetDeviceAttribute ("DataRate", StringValue (link_data_rate));
  p2p.SetChannelAttribute ("Delay", StringValue (link_delay));

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  for (auto it = clients.Begin (); it != clients.End (); ++it)
    {
      NetDeviceContainer devs = p2p.Install (NodeContainer (*it, switchs.Get (0)));
      tchPfifo.Install (devs);
      ipv4.Assign (devs);
      ipv4.NewNetwork ();
    }
  for (auto it = servers.Begin (); it != servers.End (); ++it)
    {
      NetDeviceContainer devs = p2p.Install (NodeContainer (switchs.Get (1), *it));
      tchPfifo.Install (devs);
      serverInterfaces.Add (ipv4.Assign (devs).Get (1));
      ipv4.NewNetwork ();
    }
  {
    p2p.SetQueue ("ns3::DropTailQueue");
    p2p.SetDeviceAttribute ("DataRate", StringValue (link_data_rate));
    p2p.SetChannelAttribute ("Delay", StringValue (link_delay));
    NetDeviceContainer devs = p2p.Install (switchs);
    // only backbone link has RED queue disc
    queueDiscs = tchRed.Install (devs);
    ipv4.Assign (devs);
  }
  // Set up the routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
}

void
BuildAppsTest (void)
{
  // SINK is in the right side
  uint16_t port = 50000;
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp = sinkHelper.Install (servers.Get (0));
  sinkApp.Start (Seconds (sink_start_time));
  sinkApp.Stop (Seconds (sink_stop_time));
  sinkApp.Get (0)->TraceConnectWithoutContext ("Rx", MakeCallback (&RxTrace));

  // Connection one
  // Clients are in left side
  /*
   * Create the OnOff applications to send TCP to the server
   * onoffhelper is a client that send data to TCP destination
   */

  BulkSendHelper clientHelper ("ns3::TcpSocketFactory", InetSocketAddress (serverInterfaces.GetAddress (0), port));
  clientHelper.SetAttribute ("SendSize", UintegerValue (packet_size));

  ApplicationContainer clientApps = clientHelper.Install (clients);

  // set different start/stop time for each app
  double clientStartTime = client_start_time;
  double clientStopTime = client_stop_time;
  uint32_t i = 1;
  for (auto it = clientApps.Begin (); it != clientApps.End (); ++it)
    {
      Ptr<Application> app = *it;
      app->SetStartTime (Seconds (clientStartTime));
      app->SetStopTime (Seconds (clientStopTime));
      app->TraceConnectWithoutContext ("Tx", MakeBoundCallback (&TxTrace, i++));
      clientStartTime += client_interval_time;
      clientStopTime -=  client_interval_time;
    }
}

void
SetConfig (bool useEcn, bool useDctcp)
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
      if (useDctcp)
        {
          Config::SetDefault ("ns3::TcpL4Protocol::SocketBaseType", TypeIdValue(TypeId::LookupByName ("ns3::DctcpSocket")));
          Config::SetDefault ("ns3::DctcpSocket::DctcpWeight", DoubleValue (1.0 / 16));
        }
    }
}

int
main (int argc, char *argv[])
{

  // LogComponentEnable ("DctcpSocket", LOG_LEVEL_DEBUG);
  bool useEcn = 1;
  bool useDctcp = 1;
  std::string pathOut;
  bool writeForPlot = 1;
  bool writePcap = 1;
  bool flowMonitor = 1;
  bool writeThroughput = 1;

  bool printRedStats = true;

  global_start_time = 0.0;      // 全局开始时间
  global_stop_time = 1;      // 全局停止时间
  sink_start_time = global_start_time;
  sink_stop_time = global_stop_time + 0.3;
  client_start_time = sink_start_time + 0.02;
  client_stop_time = global_stop_time;
  client_interval_time = 0.03;

  link_data_rate = "1Mbps";
  link_delay = "50us";
  packet_size = 512;
  queue_size = 128;
  threhold = 20;

  // Will only save in the directory if enable opts below
  pathOut = "./dctcp_res"; // Current directory
  CommandLine cmd;
  cmd.AddValue ("useEcn", "<0/1> to use ecn in test", useEcn);
  cmd.AddValue ("useDctcp", "<0/1> to use dctcp in test", useDctcp);
  cmd.AddValue ("pathOut", "Path to save results from --writeForPlot/--writePcap/--writeFlowMonitor", pathOut);
  cmd.AddValue ("writeForPlot", "<0/1> to write results for plot (gnuplot)", writeForPlot);
  cmd.AddValue ("writePcap", "<0/1> to write results in pcapfile", writePcap);
  cmd.AddValue ("writeFlowMonitor", "<0/1> to enable Flow Monitor and write their results", flowMonitor);
  cmd.AddValue ("writeThroughput", "<0/1> to write throughtput results", writeThroughput);

  cmd.Parse (argc, argv);

  SetConfig (useEcn, useDctcp);
  BuildTopo (30, 1);
  BuildAppsTest ();
  std::cout << "00000" << std::endl;
  if (writePcap)
    {
      PointToPointHelper ptp;
      std::stringstream stmp;
      stmp << pathOut << "/dctcp";
      ptp.EnablePcapAll (stmp.str ());
    }

  Ptr<FlowMonitor> flowmon;
  if (flowMonitor)
    {
      FlowMonitorHelper flowmonHelper;
      flowmon = flowmonHelper.InstallAll ();
    }

  if (writeForPlot)
    {
      filePlotQueue << pathOut << "/" << "red-queue.plotme";
      filePlotQueueAvg << pathOut << "/" << "red-queue_avg.plotme";

      remove (filePlotQueue.str ().c_str ());
      remove (filePlotQueueAvg.str ().c_str ());
      Ptr<QueueDisc> queue = queueDiscs.Get (0);
      Simulator::ScheduleNow (&CheckQueueSize, queue);
    }

  if (writeThroughput)
    {
      Simulator::Schedule (Seconds (sink_start_time), &CalculateThroughput);
    }

  std::cout << "11111" << std::endl;
  Simulator::Stop (Seconds (sink_stop_time));
  Simulator::Run ();

  if (flowMonitor)
    {
      std::stringstream stmp;
      stmp << pathOut << "/red.flowmon";

      flowmon->SerializeToXmlFile (stmp.str (), false, false);
    }
  std::cout << "22222" << std::endl;

  if (printRedStats)
    {
      RedQueueDisc::Stats st = StaticCast<RedQueueDisc> (queueDiscs.Get (0))->GetStats ();
      std::cout << "*** RED stats from Node 2 queue ***" << std::endl;
      std::cout << "\t " << st.unforcedDrop << " drops due prob mark" << std::endl;
      std::cout << "\t " << st.unforcedMark << " marks due prob mark" << std::endl;
      std::cout << "\t " << st.forcedDrop << " drops due hard mark" << std::endl;
      std::cout << "\t " << st.qLimDrop << " drops due queue full" << std::endl;

      st = StaticCast<RedQueueDisc> (queueDiscs.Get (1))->GetStats ();
      std::cout << "*** RED stats from Node 3 queue ***" << std::endl;
      std::cout << "\t " << st.unforcedDrop << " drops due prob mark" << std::endl;
      std::cout << "\t " << st.unforcedMark << " marks due prob mark" << std::endl;
      std::cout << "\t " << st.forcedDrop << " drops due hard mark" << std::endl;
      std::cout << "\t " << st.qLimDrop << " drops due queue full" << std::endl;
    }

  if (writeThroughput)
    {
      for (auto& resultList : throughputResult)
        {
          std::stringstream ss;
          ss << pathOut << "/throughput-" << resultList.first << ".txt";
          std::ofstream out (ss.str ());
          for (auto& entry : resultList.second)
            {
              out << entry.first << "," << entry.second << std::endl;
            }
        }
    }
  Simulator::Destroy ();

  return 0;
}
