/*
*   UDP
*
*/


#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;
NS_LOG_COMPONENT_DEFINE ("FirstScriptExample");
int 
main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse (argc, argv); 

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
 
    // 创建客户端、交换机和服务器的nodeContainer
    NodeContainer client_nodes_container;
    NodeContainer switch_nodes_container;
    NodeContainer server_nodes_container;
    int client_num = 3;
    client_nodes_container.Create(client_num);
    switch_nodes_container.Create(1);
    server_nodes_container.Create(1);

    // 创建p2p类，设置速度和延迟
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
    pointToPoint.SetChannelAttribute ("Delay", StringValue("10ms"));

    // 创建ip地址类
    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    
    // 创建因特网协议栈类
    InternetStackHelper stack;
    stack.Install (client_nodes_container); // 执行时会install一个Internet Stack(TCP, UDP, IP...) on each of the nodes in the node container
    stack.Install (switch_nodes_container);
    stack.Install (server_nodes_container);

    // 把client和switch连起来
    NetDeviceContainer devices_cl_sw;
    for(int i = 0; i < client_num; i++)
    {
        devices_cl_sw = pointToPoint.Install(NodeContainer (client_nodes_container.Get(i), switch_nodes_container.Get(0)));
        address.Assign (devices_cl_sw);
    // address.NewNetwork();
    }

    // 把switch和server连起来
    NetDeviceContainer devices_sw_se;
    devices_sw_se = pointToPoint.Install(NodeContainer (switch_nodes_container.Get(0), server_nodes_container.Get(0)));
    Ipv4InterfaceContainer interfaces = address.Assign (devices_sw_se);
    //address.NewNetwork();

    // 创建udp回响服务器
    UdpEchoServerHelper echoServer (9);
    ApplicationContainer serverApps = echoServer.Install (server_nodes_container);
    serverApps.Start (Seconds (1.0));
    serverApps.Stop (Seconds (20.0));

    // 创建udp回响客户端
    UdpEchoClientHelper echoClient (interfaces.GetAddress(1), 9);
    echoClient.SetAttribute ("MaxPackets", UintegerValue (25600*2));
    echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.00004)));
    echoClient.SetAttribute ("PacketSize", UintegerValue (512));

    ApplicationContainer clientApps = echoClient.Install (client_nodes_container);
    for (auto it = clientApps.Begin (); it != clientApps.End (); ++it)
    {
        Ptr<Application> app_ptr = *it;
        app_ptr->SetStartTime (Seconds(2.0));
        app_ptr->SetStopTime (Seconds(20.0));
    }

    bool writePcap = true;
    if (writePcap)
    {
        PointToPointHelper ptp;
        std::stringstream stmp;
        stmp << "./final_res_1" << "/final_1";
        ptp.EnablePcapAll (stmp.str ());
    }
    Simulator::Run ();
    Simulator::Destroy ();
}
