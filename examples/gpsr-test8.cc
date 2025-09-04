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

using namespace ns3;

class GpsrExample
{
public:
  GpsrExample ();
  /// Configure script parameters, \return true on successful configuration
  bool Configure (int argc, char **argv);
  /// Run simulation
  void Run ();
  /// Report results
  void Report (std::ostream & os);

private:
  ///\name parameters
  //\{
  //Node parameters
  /// Number of nodes
  uint32_t size;
  /// Width of the Node Grid
  uint32_t gridWidth;
  /// Node power at the start of the simulation
  double txPowerStart;
  /// Node power at the end of the simulation
  double txPowerEnd;
  
  uint32_t txPowerLevels;
  /// Distance between nodes, meters
  double step;
  /// Simulation time, seconds
  double totalTime;
  /// Write per-device PCAP traces if true
  bool pcap;

  // WiFi parameters
  /// RTS/CTS threshold
  /// If set to 0, RTS/CTS is disabled. If set to a value greater than 0, RTS/CTS is enabled
  /// and packets larger than this threshold will use RTS/CTS.
  uint32_t rtsCtsThreshold;
  /// PHY mode
  std::string phyMode;

  // Application parameters
  uint16_t port;
  uint32_t packetSize;
  uint32_t maxPacketCount;
  double interPacketInterval;
  //\}

  ///\name network
  //\{
  NodeContainer nodes;
  NetDeviceContainer devices;
  Ipv4InterfaceContainer interfaces;
  //\}


  void CreateNodes ();
  void CreateDevices ();
  void InstallInternetStack ();
  void InstallApplications ();

  Ptr<FlowMonitor> monitor;
  FlowMonitorHelper flowmon;

  std::set<Ipv4Address> sourceTargets;
  std::set<Ipv4Address> sinkTargets;
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

//-----------------------------------------------------------------------------
GpsrExample::GpsrExample () :
  size (25),
  gridWidth (5),
  txPowerStart(20.0),
  txPowerEnd(20.0),
  txPowerLevels (1),
  step (50.0),
  totalTime (30.0),
  pcap (true),
  rtsCtsThreshold (0), // enable RTS/CTS by default
  phyMode ("OfdmRate6Mbps"),
  port (9),
  packetSize (1024),
  maxPacketCount (10000000),
  interPacketInterval (0.02)

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
  cmd.AddValue ("grid", "Grid Width.", gridWidth);
  cmd.AddValue ("time", "Simulation time, s.", totalTime);
  cmd.AddValue ("txPowerStart", "Transmission power of nodes at start (dBm)", txPowerStart);
  cmd.AddValue ("txPowerEnd", "Transmission power of nodes at end (dBm)", txPowerEnd);
  cmd.AddValue ("txPowerLevels", "Number of transmission power levels", txPowerLevels);
  cmd.AddValue ("step", "Grid step, m", step);

  // WiFi
  cmd.AddValue ("rtsCts", "RTS/CTS threshold (0 = always, 2347 = disabled)", rtsCtsThreshold);
  cmd.AddValue ("phyMode", "WiFi physical mode (e.g., OfdmRate6Mbps)", phyMode);

  // Applications
  cmd.AddValue ("packetSize", "Packet size in bytes", packetSize);
  cmd.AddValue ("maxPackets", "Number of packets to send", maxPacketCount);
  cmd.AddValue ("interval", "Inter-packet interval (s)", interPacketInterval);

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

  // install FlowMonitor
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

  uint64_t totalControlRxPackets = 0;
  uint64_t totalControlTxPackets = 0;
  uint64_t totalControlRxBytes = 0;
  uint64_t totalControlTxBytes = 0;


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

      else
      {
        totalControlRxPackets += st.rxPackets;
        totalControlTxPackets += st.txPackets;
        totalControlRxBytes += st.rxBytes;
        totalControlTxBytes += st.txBytes;
      }
    }

  double avgDelay = (countedDataPackets > 0) ? totalDataDelay / countedDataPackets : 0.0;
  double duration = timeDataLastRx - timeDataFirstTx;
  double throughputKbps = (duration > 0) ? (totalDataRxBytes * 8.0 / duration / 1024) : 0.0;
  double packetLossRatio = (totalDataTxPackets > 0) ? (double)totalDataLostPackets / totalDataTxPackets : 0.0;


  double rohPackets = ((totalDataRxPackets + totalControlRxPackets) > 0) ? 
  (double)totalControlTxPackets / (double) (totalDataRxPackets + totalControlRxPackets) : 0.0;
  double rohBytes = ((totalDataRxBytes + totalControlRxBytes) > 0) ? 
  (double)totalControlTxBytes / (double) (totalDataRxBytes + totalControlRxBytes) : 0.0;
  
  os << "========== Data Metrics ==========\n";
  os << "Tx Packets: " << totalDataTxPackets << "\n";
  os << "Rx Packets: " << totalDataRxPackets << "\n";
  os << "Lost Packets: " << totalDataLostPackets << "\n";
  os << "Packet Loss Ratio: " << packetLossRatio << "\n";
  os << "Aggregate Throughput: " << throughputKbps << " Kbps\n";
  os << "Average Delay: " << avgDelay << " s\n";
  os << "========== Control Metrics ==========\n";
  os << "Tx Packets: " << totalControlTxPackets << "\n";
  os << "RX Packets: " << totalControlRxPackets << "\n";
  os << "Roh Packets: " << rohPackets << "\n";
  os << "Roh Bytes: " << rohBytes << "\n";
  os << "========================================\n";
}

void
GpsrExample::CreateNodes ()
{
  std::cout << "Creating " << (unsigned)size << " nodes " << step << " m apart.\n";
  nodes.Create (size);
  // Name nodes
  for (uint32_t i = 0; i < size; ++i)
     {
       std::ostringstream os;
       os << "node-" << i;
       Names::Add (os.str (), nodes.Get (i));
     }
  // Create static grid
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                "MinX", DoubleValue (0.0),
                                "MinY", DoubleValue (0.0),
                                "DeltaX", DoubleValue (step),
                                "DeltaY", DoubleValue (step),
                                "GridWidth", UintegerValue (gridWidth),
                                "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
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
//   gpsr.SetSourceTargets (sourceTargets);
//   gpsr.SetSinkTargets (sinkTargets);
  // you can configure GPSR attributes here using gpsr.Set(name, value)
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
  uint16_t server1Position = size - 1; // bottom right
  ApplicationContainer apps = server.Install (nodes.Get(server1Position));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (totalTime - 0.1));

  UdpEchoClientHelper client (interfaces.GetAddress (server1Position), port);
  client.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
  client.SetAttribute ("Interval", TimeValue (Seconds (interPacketInterval)));
  client.SetAttribute ("PacketSize", UintegerValue (packetSize));

  uint16_t clientPosition = 0; // top left
  apps = client.Install (nodes.Get (clientPosition));
  apps.Start (Seconds (2.0));
  apps.Stop (Seconds (totalTime - 0.1));
}

