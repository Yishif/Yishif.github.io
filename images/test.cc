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
#include "ns3/point-to-point-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/olsr-module.h"

// Default Network Topology
//
//                                                    n2
//                                 192.1.2.0        -    -        192.1.5.0
//   Wifi 192.2.1.0                 p-to-p2      -          -       p-to-p5
//                 AP                         -                -
//  *    *    *    *                       -                      -
//  |    |    |    |    192.1.1.0       -   192.1.3.0      192.1.6.0  -
// n8   n7   n6   n0 -------------- n1 -------------- n3 --------------- n5
//                        p-to-p1       -   p-to-p3        p-to-p6  -
//                                         -                     -
//                                            -               -
//                                  192.1.4.0     -         -      192.1.7.0
//                                    p-to-p4        -   -          p-to-p7
//                                                    n4
//
//

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("test");

int
main (int argc, char *argv[])
{
    bool verbose = true;
    uint32_t nWifi = 3;
    bool tracing = false;

    CommandLine cmd (__FILE__);
    cmd.AddValue ("nWifi", "Number of wifi STA devices", nWifi);
    cmd.AddValue ("verbose", "Tell echo applications to log if true", verbose);
    cmd.AddValue ("tracing", "Enable pcap tracing", tracing);

    cmd.Parse (argc,argv);

    if (nWifi > 18)
    {
        std::cout << "nWifi should be 18 or less; otherwise grid layout exceeds the bounding box" << std::endl;
        return 1;
    }

    if (verbose)
    {
        LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
        LogComponentEnable ("PointToPointNetDevice", LOG_LEVEL_INFO);
    }

    //  ################################## p2p网络 ###########################################

    NodeContainer p2pNodes1, p2pNodes2, p2pNodes3, p2pNodes4, p2pNodes5, p2pNodes6, p2pNodes7;
    p2pNodes1.Create(2);
    p2pNodes2.Add(p2pNodes1.Get(1));    p2pNodes2.Create(1);
    p2pNodes3.Add(p2pNodes1.Get(1));    p2pNodes3.Create(1);
    p2pNodes4.Add(p2pNodes1.Get(1));    p2pNodes4.Create(1);
    p2pNodes5.Add(p2pNodes2.Get(1));    p2pNodes5.Create(1);
    p2pNodes6.Add(p2pNodes3.Get(1));    p2pNodes6.Add(p2pNodes5.Get(1));
    p2pNodes7.Add(p2pNodes4.Get(1));    p2pNodes7.Add(p2pNodes5.Get(1));

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
    pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

    NetDeviceContainer p2pDevices1, p2pDevices2, p2pDevices3, p2pDevices4, p2pDevices5, p2pDevices6, p2pDevices7;
    p2pDevices1 = pointToPoint.Install (p2pNodes1);
    p2pDevices2 = pointToPoint.Install (p2pNodes2);
    p2pDevices3 = pointToPoint.Install (p2pNodes3);
    p2pDevices4 = pointToPoint.Install (p2pNodes4);
    p2pDevices5 = pointToPoint.Install (p2pNodes5);
    p2pDevices6 = pointToPoint.Install (p2pNodes6);
    p2pDevices7 = pointToPoint.Install (p2pNodes7);

    // ################################## Wi-Fi网络 ###########################################

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create (nWifi);
    NodeContainer wifiApNode = p2pNodes1.Get (0);

    // 设置 wifi channel 和 WifiPhy
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();    // 默认 传播模型、损耗模型
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();    // 默认 误码率模型
    phy.SetChannel (channel.Create ());

    // 设置 WifiMac 并在节点中安装 NetDevic
    WifiHelper wifi;
    wifi.SetRemoteStationManager ("ns3::AarfWifiManager");    // 用于速率控制
    WifiMacHelper mac;
    Ssid ssid = Ssid ("ns-3-ssid-1");   // AP和移动结点的ssid要一致才能通信

    // 移动结点
    mac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (ssid), "ActiveProbing", BooleanValue (false));
    NetDeviceContainer staDevices;
    staDevices = wifi.Install (phy, mac, wifiStaNodes);

    // AP结点
    mac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid));
    NetDeviceContainer apDevices;
    apDevices = wifi.Install (phy, mac, wifiApNode);

    // 设置移动模型，在wifi网络设备安装至结点后配置
    MobilityHelper mobility;
    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                   "MinX", DoubleValue (0.0), // 起始坐标（0，0），同时也是AP结点的位置
                                   "MinY", DoubleValue (0.0),
                                   "DeltaX", DoubleValue (5.0),   // x，y轴结点间距 5m,10m
                                   "DeltaY", DoubleValue (10.0),
                                   "GridWidth", UintegerValue (3),    // 每行最大结点数
                                   "LayoutType", StringValue ("RowFirst"));
    mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                               "Bounds", RectangleValue (Rectangle (-50, 50, -50, 50)));
    mobility.Install (wifiStaNodes);
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (wifiApNode);

    //  ########################### 安装TCP/IP协议簇 #########################

    OlsrHelper olsr;
    Ipv4StaticRoutingHelper staticRouting;
    Ipv4ListRoutingHelper list;
    list.Add(staticRouting, 1);
    list.Add(olsr, 10);

    InternetStackHelper stack;

    stack.SetRoutingHelper(list);

    stack.Install (p2pNodes2);
    stack.Install (p2pNodes3.Get(1));
    stack.Install (p2pNodes4.Get(1));
    stack.Install (p2pNodes5.Get(1));
    stack.Install (wifiStaNodes);
    stack.Install (wifiApNode);

    Ipv4AddressHelper address;

    address.SetBase ("192.2.1.0", "255.255.255.0");
    address.Assign (apDevices);
    address.Assign (staDevices);

    address.SetBase ("192.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces1;
    p2pInterfaces1 = address.Assign (p2pDevices1);

    address.SetBase ("192.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces2;
    p2pInterfaces2 = address.Assign (p2pDevices2);

    address.SetBase ("192.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces3;
    p2pInterfaces3 = address.Assign (p2pDevices3);

    address.SetBase ("192.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces4;
    p2pInterfaces4 = address.Assign (p2pDevices4);

    address.SetBase ("192.1.5.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces5;
    p2pInterfaces5 = address.Assign (p2pDevices5);

    address.SetBase ("192.1.6.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces6;
    p2pInterfaces6 = address.Assign (p2pDevices6);

    address.SetBase ("192.1.7.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces7;
    p2pInterfaces7 = address.Assign (p2pDevices7);

    // ######################## 安装应用程序 #############################

    UdpEchoServerHelper echoServer (9); // 监听9号窗口

    // 在 n5 中安装服务端程序（接收）
    ApplicationContainer serverApps = echoServer.Install (p2pNodes5.Get (1));
    serverApps.Start (Seconds (1.0));
    serverApps.Stop (Seconds (10.0));

    // 配置客户端程序属性
    UdpEchoClientHelper echoClient (p2pInterfaces5.GetAddress (1), 9);
    echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
    echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
    echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

    ApplicationContainer clientApps = echoClient.Install (wifiStaNodes.Get (nWifi - 1));
//    ApplicationContainer clientApps = echoClient.Install (p2pNodes5.Get (1));
    clientApps.Start (Seconds (2.0));
    clientApps.Stop (Seconds (10.0));

    // ######################### 设置路由 #################################

//    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
    Simulator::Stop (Seconds (10.0));

    // 数据追踪
    if (tracing == true)
    {
        pointToPoint.EnablePcapAll ("third"); // 收集这个信道上所有结点的链路层分组收发记录 前缀名-结点标号-网络设备编号
        phy.EnablePcap ("third", apDevices.Get (0));
    }

    Simulator::Run ();
    Simulator::Destroy ();
    return 0;
}