/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ns3/gpsr-module.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/v4ping-helper.h"
#include "ns3/udp-echo-server.h"
#include "ns3/udp-echo-client.h"
#include "ns3/udp-echo-helper.h"
#include "ns3/flow-monitor-module.h"
#include <iostream>
#include <cmath>
#include "ns3/onoff-application.h"
#include "ns3/on-off-helper.h"
#include "ns3/udp-server.h"
#include "ns3/udp-socket-factory.h"



using namespace ns3;

class GpsrExample
{
public:
  GpsrExample ();
  bool Configure (int argc, char **argv);
  void Run ();
  void Report (std::ostream & os);

private:
  // Parameters
  uint32_t size;
  double txPowerStart;
  double txPowerEnd;
  uint32_t txPowerLevels;
  double totalTime;
  bool pcap;

  // WiFi
  uint32_t rtsCtsThreshold;
  std::string phyMode;

  // Applications
  uint16_t port;
  uint32_t packetSize;
  uint32_t maxPacketCount;
  double interPacketInterval;

  // Mobility
  double mapWidth;
  double mapHeight;
  double speedMin;
  double speedMax;
  double pauseMin;
  double pauseMax;

  // Network
  NodeContainer nodes;
  NetDeviceContainer devices;
  Ipv4InterfaceContainer interfaces;

  void CreateNodes ();
  void CreateDevices ();
  void InstallInternetStack ();
  void InstallApplications ();

  Ptr<FlowMonitor> monitor;
  FlowMonitorHelper flowmon;

  std::set<Ipv4Address> sourceTargets;
  std::set<Ipv4Address> sinkTargets;

  bool showPaths;
};

int main (int argc, char **argv)
{
  GpsrExample test;
  if (! test.Configure(argc, argv))
    NS_FATAL_ERROR ("Configuration failed. Aborted.");

  test.Run ();
  test.Report (std::cout);
  return 0;
}

GpsrExample::GpsrExample () :
  size (50),
  txPowerStart(20.0),
  txPowerEnd(20.0),
  txPowerLevels (1),
  totalTime (30.0),
  pcap (false),
  rtsCtsThreshold (0),
  phyMode ("OfdmRate6Mbps"),
  port (9),
  packetSize (1024),
  maxPacketCount (10000000),
  interPacketInterval (0.001),
  mapWidth (100),
  mapHeight (100),
  speedMin (1.0),
  speedMax (10),
  pauseMin (0.0),
  pauseMax (5.0),
  showPaths (false)
{
}

bool
GpsrExample::Configure (int argc, char **argv)
{
  SeedManager::SetSeed(12345);
  CommandLine cmd;

  // General
  cmd.AddValue ("pcap", "Write PCAP traces.", pcap);
  cmd.AddValue ("size", "Number of nodes.", size);
  cmd.AddValue ("time", "Simulation time, s.", totalTime);
  cmd.AddValue ("txPowerStart", "Transmission power of nodes at start (dBm)", txPowerStart);
  cmd.AddValue ("txPowerEnd", "Transmission power of nodes at end (dBm)", txPowerEnd);
  cmd.AddValue ("txPowerLevels", "Number of transmission power levels", txPowerLevels);

  // WiFi
  cmd.AddValue ("rtsCts", "RTS/CTS threshold (0 = always, 2347 = disabled)", rtsCtsThreshold);
  cmd.AddValue ("phyMode", "WiFi physical mode (e.g., OfdmRate6Mbps)", phyMode);

  // Applications
  cmd.AddValue ("packetSize", "Packet size in bytes", packetSize);
  cmd.AddValue ("maxPackets", "Number of packets to send", maxPacketCount);
  cmd.AddValue ("interval", "Inter-packet interval (s)", interPacketInterval);

  // Mobility
  cmd.AddValue ("mapWidth", "Width of the simulation area (m)", mapWidth);
  cmd.AddValue ("mapHeight", "Height of the simulation area (m)", mapHeight);
  cmd.AddValue ("speedMin", "Minimum node speed (m/s)", speedMin);
  cmd.AddValue ("speedMax", "Maximum node speed (m/s)", speedMax);
  cmd.AddValue ("pauseMin", "Minimum pause time (s)", pauseMin);
  cmd.AddValue ("pauseMax", "Maximum pause time (s)", pauseMax);

  cmd.AddValue ("showPaths", "Show discovered paths", showPaths);

  cmd.Parse (argc, argv);
  return true;
}

void
GpsrExample::Run ()
{
  CreateNodes ();
  CreateDevices ();
  InstallInternetStack ();
  InstallApplications ();

  GpsrHelper gpsr;
  gpsr.Install ();

  monitor = flowmon.InstallAll();

  std::cout << "Starting simulation for " << totalTime << " s ...\n";

  Simulator::Stop (Seconds (totalTime));
  Simulator::Run ();
  Simulator::Destroy ();
}

void
GpsrExample::Report (std::ostream &os)
{
  if (monitor == nullptr)
    {
      os << "No FlowMonitor data available.\n";
      return;
    }

  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

  uint64_t totalDataTxPackets = 0;
  uint64_t totalDataRxPackets = 0;
  uint64_t totalDataLostPackets = 0;
  uint64_t totalDataRxBytes = 0;
  uint64_t totalDataTxBytes = 0;
  double totalDataDelay = 0.0;
  uint64_t countedDataPackets = 0;
  double timeDataFirstTx = std::numeric_limits<double>::max ();
  double timeDataLastRx = 0.0;

  for (auto const &flow : stats)
    {
      const FlowMonitor::FlowStats &st = flow.second;
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);

      if (sourceTargets.count(t.sourceAddress) > 0 && sinkTargets.count(t.destinationAddress) > 0)
      {
        totalDataTxPackets += st.txPackets;
        totalDataRxPackets += st.rxPackets;
        totalDataLostPackets += st.lostPackets;
        totalDataRxBytes += st.rxBytes;
        totalDataTxBytes += st.txBytes;

        if (st.rxPackets > 0)
          {
            totalDataDelay += st.delaySum.GetSeconds ();
            countedDataPackets += st.rxPackets;
          }

        if (st.timeFirstTxPacket.GetSeconds () < timeDataFirstTx)
          timeDataFirstTx = st.timeFirstTxPacket.GetSeconds ();
        if (st.timeLastRxPacket.GetSeconds () > timeDataLastRx)
          timeDataLastRx = st.timeLastRxPacket.GetSeconds ();
      }
    }

  double avgDelay = (countedDataPackets > 0) ? totalDataDelay / countedDataPackets : 0.0;
  double duration = timeDataLastRx - timeDataFirstTx;
  double throughputKbps = (duration > 0) ? (totalDataRxBytes * 8.0 / duration / 1024) : 0.0;
  double packetLossRatio = (totalDataTxPackets > 0) ? (double)totalDataLostPackets / totalDataTxPackets : 0.0;

  os << "========== Data Metrics ==========\n";
  os << "Tx Packets: " << totalDataTxPackets << "\n";
  os << "Rx Packets: " << totalDataRxPackets << "\n";
  os << "Lost Packets: " << totalDataLostPackets << "\n";
  os << "Packet Loss Ratio: " << packetLossRatio << "\n";
  os << "Aggregate Throughput: " << throughputKbps << " Kbps\n";
  os << "Average Delay: " << avgDelay << " s\n";
}

void
GpsrExample::CreateNodes ()
{
  std::cout << "Creating " << (unsigned)size << " nodes with Random Waypoint mobility.\n";
  nodes.Create (size);

  for (uint32_t i = 0; i < size; ++i)
    {
      std::ostringstream os;
      os << "node-" << i;
      Names::Add (os.str (), nodes.Get (i));
    }

  MobilityHelper mobility;

  Ptr<UniformRandomVariable> speed = CreateObject<UniformRandomVariable> ();
  speed->SetAttribute ("Min", DoubleValue (speedMin));
  speed->SetAttribute ("Max", DoubleValue (speedMax));

  Ptr<UniformRandomVariable> pause = CreateObject<UniformRandomVariable> ();
  pause->SetAttribute ("Min", DoubleValue (pauseMin));
  pause->SetAttribute ("Max", DoubleValue (pauseMax));

  Ptr<RandomRectanglePositionAllocator> positionAlloc = CreateObject<RandomRectanglePositionAllocator> ();
  positionAlloc->SetX (CreateObjectWithAttributes<UniformRandomVariable> (
                          "Min", DoubleValue (0.0),
                          "Max", DoubleValue (mapWidth)));
  positionAlloc->SetY (CreateObjectWithAttributes<UniformRandomVariable> (
                          "Min", DoubleValue (0.0),
                          "Max", DoubleValue (mapHeight)));

  mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
                             "Speed", PointerValue (speed),
                             "Pause", PointerValue (pause),
                             "PositionAllocator", PointerValue (positionAlloc));

  mobility.SetPositionAllocator (positionAlloc);
  mobility.Install (nodes);
}

void
GpsrExample::CreateDevices ()
{
  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  wifiPhy.Set ("TxPowerStart", DoubleValue (txPowerStart));
  wifiPhy.Set ("TxPowerEnd", DoubleValue (txPowerEnd));
  wifiPhy.Set ("TxPowerLevels", UintegerValue (txPowerLevels));

  WifiHelper wifi;
  wifi.SetRemoteStationManager (
      "ns3::ConstantRateWifiManager",
      "DataMode", StringValue (phyMode),
      "RtsCtsThreshold", UintegerValue (rtsCtsThreshold));

  devices = wifi.Install (wifiPhy, wifiMac, nodes);

  if (pcap)
    {
      wifiPhy.EnablePcapAll (std::string ("gpsr"));
    }
}

void
GpsrExample::InstallInternetStack ()
{
  Ipv4Address source_t("10.0.0.1");
  sourceTargets.insert(source_t);

  std::string ip_sink = "10.0.0." + std::to_string(size);
  Ipv4Address sink_t(ip_sink.c_str());
  sinkTargets.insert(sink_t);

  GpsrHelper gpsr;


  InternetStackHelper stack;
  stack.SetRoutingHelper (gpsr);
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.0.0.0", "255.255.0.0");
  interfaces = address.Assign (devices);
}

void
GpsrExample::InstallApplications ()
{
  UdpEchoServerHelper server (port);
  uint16_t server1Position = size - 1;
  ApplicationContainer apps = server.Install (nodes.Get(server1Position));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (totalTime - 0.1));

   OnOffHelper onoff(
      "ns3::UdpSocketFactory",
      InetSocketAddress(interfaces.GetAddress(server1Position), port));

  // Configure aqui a TAXA (ex: 5 Mbps)
  onoff.SetAttribute("DataRate", DataRateValue(DataRate("5Mbps")));

  // Tamanho do pacote (vai ser variado nos seus testes)
  onoff.SetAttribute("PacketSize", UintegerValue(packetSize));

  // Sempre ligado (geração contínua)
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

  uint16_t clientPosition = 0;
  ApplicationContainer clientApp = onoff.Install(nodes.Get(clientPosition));
  clientApp.Start(Seconds(2.0));
  clientApp.Stop(Seconds(totalTime - 0.1));
}

