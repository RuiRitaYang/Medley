/**
 *  Main file to simulate Medley protocol with bandwith records
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <stdlib.h>
#include <typeinfo>
#include <time.h>
#include <unistd.h>
#include <unordered_map>
#include <math.h>

#include "ns3/object.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/global-route-manager.h"
#include "ns3/netanim-module.h"
#include "ns3/assert.h"
#include "ns3/ipv4-nix-vector-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/energy-module.h"

// For wifi
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/olsr-helper.h"

#include "ns3/SwimHelper.h"
#include "ns3/swim.h"


using namespace std;
using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SWIM_TEST");


// Entry format: edge1, edge2, distance
typedef tuple<int,int,double> Entry;

Entry parseInput(string line){
  stringstream ss(line);
  string item;
  Entry tokens;
  int i=0;
  while (getline(ss, item, ',')) {
    if(i==0){
        get<0>(tokens) = stoi(item);
        i++;
      }
    else if (i==1){
        get<1>(tokens) = stoi(item);
        i++;
    }
    else{
      get<2>(tokens) = stod(item);
      i=0;
    }
  }
  return tokens;
}

std::pair<double,double> parse2Doubles(string line){
  stringstream ss(line);
  string item;
  std::pair<double,double> p;
  int i = 0;
  while(getline(ss, item, ',')){
    if (i == 0 ){
      p.first = stod(item);
    } else{
      p.second = stod(item);
    }
  }
  return p;
}


Ptr<SWIM> GetSWIMApp(Ptr <Node> node){
  Ptr<Application> swimApp = node->GetApplication(0);
  return DynamicCast<SWIM>(swimApp);
}

void
RemainingEnergy (double oldValue, double remainingEnergy) {
  NS_LOG_INFO (Simulator::Now ().GetSeconds () 
    << "s Current remaining energy = " << remainingEnergy << "J");
}
void
TotalEnergy (double oldValue, double totalEnergy){
  NS_LOG_INFO (Simulator::Now ().GetSeconds ()
    << "s Total energy consumed by radio = " << totalEnergy << "J");
}

void Simulation(uint32_t num_failure, uint64_t failure_time, 
                uint64_t t_max, int timeout_multi, double power_k,
                double r_tmp_outage,uint32_t t_tmp_outage, uint32_t packet_loss,
                uint32_t topo_type, uint32_t n_direct_ping, uint32_t n_ind_ping,
                string mode, string fail_mode)
{
  NS_LOG_INFO("Start reading from file");

  ifstream input;
  std::cout << "Parameters:\n  mode -- " << mode 
            << "\n  fail_mode -- " << fail_mode << "\n  num_failure -- " << num_failure
            << "\n  t_max -- " << t_max << "\n  timeout multi -- " << timeout_multi
            << "\n  failure_time -- " << failure_time
            << "\n  power_k -- " << power_k
            << std::endl;

  /****Change directory to where topology is stored****/
  input.open("/home/ruiyang/Developer/ns3/ns-allinone-3.27/ns-3.27/topology/Topology_" + mode + ".txt");
  string line;
  getline(input,line);
  int numNode = stoi(line);
  std::cout << "  total number of node -- " << numNode << std::endl;
  std::string phyMode ("DsssRate1Mbps");
  double interval = 1.0; // seconds
  bool verbose = false;
  // Convert to time object
  Time interPacketInterval = Seconds (interval);

  // disable fragmentation for frames below 2200 bytes
  Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("2200"));
  // turn off RTS/CTS for frames below 2200 bytes
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("2200"));
  // Fix non-unicast data rate to be the same as that of unicast
  Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode",
                      StringValue (phyMode));

  NodeContainer nodes;
  nodes.Create(numNode);

  WifiHelper wifi;
  if (verbose)
  {
    wifi.EnableLogComponents ();  // Turn on all Wifi logging
  }

  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  // set it to zero; otherwise, gain will be added
  wifiPhy.Set ("RxGain", DoubleValue (-10) );
  // ns-3 supports RadioTap and Prism tracing extensions for 802.11b
  wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);

  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
  wifiPhy.SetChannel (wifiChannel.Create ());

  // Add an upper mac and disable rate control
  WifiMacHelper wifiMac;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode",StringValue (phyMode),
                                "ControlMode",StringValue (phyMode));
  // Set it to adhoc mode
  wifiMac.SetType ("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);


  /****Add Nodes***/
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();

  ifstream f_pos;
  // f_pos.open("/home/yifei/work/ns-allinone-3.27/ns-3.27/topology/Position.txt");
  f_pos.open("/home/ruiyang/Developer/ns3/ns-allinone-3.27/ns-3.27/topology/Position_" + mode + ".txt");
  std::pair<double, double> single_pos;
  while (getline(f_pos, line)){
    single_pos = parse2Doubles(line);
    positionAlloc->Add(Vector(single_pos.first, single_pos.second, 0.0));
  }
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);
  f_pos.close();

  /***************************************************************************/
  /* energy source */
  BasicEnergySourceHelper basicSourceHelper;
  // configure energy source
  basicSourceHelper.Set ("BasicEnergySourceInitialEnergyJ", DoubleValue (t_max * 20.0));
  // install source
  EnergySourceContainer sources = basicSourceHelper.Install (nodes);
  /* device energy model */
  WifiRadioEnergyModelHelper radioEnergyHelper;
  // configure radio energy model
  radioEnergyHelper.Set ("TxCurrentA", DoubleValue (0.0174));
  // install device model
  DeviceEnergyModelContainer deviceModels = radioEnergyHelper.Install (devices, sources);
  /***************************************************************************/

  // Enable OLSR
  OlsrHelper olsr;
  Ipv4StaticRoutingHelper staticRouting;

  Ipv4ListRoutingHelper list;
  list.Add (staticRouting, 0);
  list.Add (olsr, 10);

  /* Stack thing, not sure whether still need in wifi mode */
  NS_LOG_INFO ("creating internet stack");
  InternetStackHelper stack;
  stack.SetRoutingHelper(list);
  stack.Install(nodes);


  NS_LOG_INFO ("create ipv4 address");
  Ipv4AddressHelper ipv4Addr;
  ipv4Addr.SetBase("10.0.0.0","255.255.255.0");

  vector<Entry> topology;
  while (getline(input,line)){
    topology.push_back(parseInput(line));
  }
  input.close();

  int totalLinks = (int)topology.size();

  NS_LOG_INFO ("creating node containers");
  Ipv4InterfaceContainer ipic = ipv4Addr.Assign(devices);

  /*** Setup Energy Tracing ****/
  Ptr<BasicEnergySource>* basicSourcePtr = new Ptr<BasicEnergySource>[numNode];

  for (int i = 0; i < numNode; ++i) {
    basicSourcePtr[i] = DynamicCast<BasicEnergySource> (sources.Get(i));
    basicSourcePtr[i]->TraceConnectWithoutContext ("RemainingEnergy", MakeCallback (&RemainingEnergy));
    // device energy model
    Ptr<DeviceEnergyModel> basicRadioModelPtr =
      basicSourcePtr[i]->FindDeviceEnergyModels ("ns3::WifiRadioEnergyModel").Get (0);
    NS_ASSERT (basicRadioModelPtr != NULL);
    basicRadioModelPtr->TraceConnectWithoutContext ("TotalEnergyConsumption", MakeCallback (&TotalEnergy));
  }

  NS_LOG_INFO ("Create Swim container.");
  SwimHelper swimhelper;
  ApplicationContainer swimApp = swimhelper.Install(nodes);

  /**** Add neighbor info to each node ***/
  for (int i=0; i < totalLinks; i++){
    int edge1 = get<0>(topology[i]);
    int edge2 = get<1>(topology[i]);
    double distance = get<2>(topology[i]);
    GetSWIMApp(nodes.Get(edge1))->SetMyAddress(ipic.GetAddress(edge1));
    GetSWIMApp(nodes.Get(edge1))->AddNeighbor(ipic.GetAddress(edge2), distance);
    GetSWIMApp(nodes.Get(edge2))->SetMyAddress(ipic.GetAddress(edge2));
    GetSWIMApp(nodes.Get(edge2))->AddNeighbor(ipic.GetAddress(edge1), distance);
  }

  /**** Simulate fail-stop failures ***/
  for (int i = 0; i < numNode; ++i) {
    GetSWIMApp(nodes.Get(i))->SetMaxRunningTime(t_max);
  }

  vector<int> chosen_fail_node;
  if (fail_mode == "fixed") {
    for (int i = 0; i < (int) num_failure; ++i) {
      int failed_node = 0;
      Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable> ();
      failed_node = x->GetInteger(0, numNode) % numNode;
      while (std::find(chosen_fail_node.begin(), chosen_fail_node.end(), failed_node) != chosen_fail_node.end()) {
        failed_node = x->GetInteger(0, numNode) % numNode;
      }
      chosen_fail_node.push_back(failed_node);
      std::cout << "  failed_node -- " << failed_node + 1 << std::endl;
      GetSWIMApp(nodes.Get(failed_node))->SetFailureTime(failure_time);
    }
  } else if (fail_mode == "intra") {
    for (int i = 0; i < (int) num_failure; ++i) {
      GetSWIMApp(nodes.Get(i))->SetFailureTime(failure_time);
    }
  } else if (fail_mode == "inter" ) {
    GetSWIMApp(nodes.Get(2))->SetFailureTime(failure_time);
    GetSWIMApp(nodes.Get(4))->SetFailureTime(failure_time);
    GetSWIMApp(nodes.Get(7))->SetFailureTime(failure_time);
  } else if (fail_mode == "separate" ) {
    GetSWIMApp(nodes.Get(5))->SetFailureTime(failure_time);
    GetSWIMApp(nodes.Get(10))->SetFailureTime(failure_time);
    GetSWIMApp(nodes.Get(20))->SetFailureTime(failure_time);
  } else {
    for (int i = 0; i < (int) num_failure; ++i) {
      GetSWIMApp(nodes.Get(i))->SetFailureTime(failure_time);      
    }
  }
  
  NS_LOG_INFO("Set parameters");
  for (int i = 0; i < numNode; ++i) {
    if (timeout_multi < 0) {
      GetSWIMApp(nodes.Get(i))->SetTimeout(ceil(log(numNode + 1)), 20);
    } else {
      GetSWIMApp(nodes.Get(i))->SetTimeout(timeout_multi, 20);
    }

    GetSWIMApp(nodes.Get(i))->SetPowerk(power_k);
    GetSWIMApp(nodes.Get(i))->SetTempOutagePara(r_tmp_outage, t_tmp_outage);
    GetSWIMApp(nodes.Get(i))->SetNumDirectPing(n_direct_ping);
    GetSWIMApp(nodes.Get(i))->SetNumIndirectPing(n_ind_ping);
    GetSWIMApp(nodes.Get(i))->SetPacketLoss(packet_loss);
  }

  AsciiTraceHelper ascii;
  wifiPhy.EnableAsciiAll (ascii.CreateFileStream ("swim.log"));
  //  swifiPhy.EnablePcap ("wifi-simple-adhoc-grid", devices);


  /**** Prepare Flow monitor ******/
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor=flowmon.InstallAll();

  // Trace routing tables
  Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> ("wifi-simple-adhoc-grid.routes", std::ios::out);
  olsr.PrintRoutingTableAllEvery (Seconds (2), routingStream);
  Ptr<OutputStreamWrapper> neighborStream = Create<OutputStreamWrapper> ("wifi-simple-adhoc-grid.neighbors", std::ios::out);
  olsr.PrintNeighborCacheAllEvery (Seconds (2), neighborStream);


  Simulator::Run();
  ofstream fbandwidth, fbwinfo;
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  map<FlowId,FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
  fbandwidth.open("/home/ruiyang/Developer/ns3/ns-allinone-3.27/ns-3.27/result/bandwidth_" + \
    mode + "_failmode_" + fail_mode + "_num_f_" + to_string(num_failure) + "_powerk_" + to_string(power_k) + \
    "_timeout_" + to_string(timeout_multi) + "_pktloss_" + to_string(packet_loss) + ".txt");
  fbwinfo.open("/home/ruiyang/Developer/ns3/ns-allinone-3.27/ns-3.27/result/bw_info_" + \
    mode + "_failmode_" + fail_mode + "_num_f_" + to_string(num_failure) + "_powerk_" + to_string(power_k) + \
    "_timeout_" + to_string(timeout_multi) + "_pktloss_" + to_string(packet_loss) + ".txt");

  for(auto it = stats.begin(); it != stats.end();it++){
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (it->first);
    fbwinfo << "Flow " << it->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress 
            << " KiloBytes Received: " << it->second.rxBytes * 8.0/ 1024 << " KB";
    fbandwidth << it->second.rxBytes * 8.0 / 1024 << std::endl;
  }
  fbandwidth.close();
  fbwinfo.close();

  // print remaining energy
  for (DeviceEnergyModelContainer::Iterator iter = deviceModels.Begin (); iter != deviceModels.End (); iter ++) {
    double energyConsumed = (*iter)->GetTotalEnergyConsumption ();
    std::cout << "Total energy consumed by radio = " << energyConsumed << "J \n";
  }

  Simulator::Destroy();
}


int main(int argc, char *argv[]){

  uint32_t num_failure = 1;
  uint64_t t_failure = 1500;
  uint64_t t_max = 3000;
  double power_k = 3;
  double r_tmp_outage = 0.0;
  uint32_t t_tmp_outage = 100;
  uint32_t topo_type = 0; // 0 - grid, 1 - random, 2 - cluster
  uint32_t n_direct_ping = 1;
  uint32_t n_ind_ping = 3;
  int timeout_multi = -1;
  uint32_t packet_loss = 0;
  string input_mode = "random_25";
  string fail_mode = "fixed";
  NS_LOG_UNCOND ("SWIM Simulator");

  LogComponentEnable("SWIM", LOG_LEVEL_ALL);

  CommandLine cmd;
  cmd.Usage("For Spatial SWIM.");
  cmd.AddValue("num_failure", "number of fail-stop node", num_failure);
  cmd.AddValue("t_failure", "the fail-stop time", t_failure);
  cmd.AddValue("t_max", "the max running time", t_max);
  cmd.AddValue("power_k", "the power parameter k", power_k);
  cmd.AddValue("r_tmp_outage", "tmp outage rate", r_tmp_outage);
  cmd.AddValue("t_tmp_outage", "max tmp outage time", t_tmp_outage);
  cmd.AddValue("topo_type", "Topology type 0 - grid, 1 - random, 2 - cluster", topo_type);
  cmd.AddValue("n_direct_ping", "Number of direct ping for one time", n_ind_ping);
  cmd.AddValue("n_ind_ping", "Number of indirect ping for one time", n_ind_ping);
  cmd.AddValue("input_mode", "postfix of the filename", input_mode);
  cmd.AddValue("fail_mode", "how the fail-stop failures happen \n \
                fixed - grid/random \n \
                intra - num_failure <= 6, only for cluster mode \n \
                inter - 3 collaberate fails, only for cluster mode \n \
                separate - 3 discrete node fails, only for cluster", fail_mode);
  cmd.AddValue("timeout_multi", "How many times the suspecion timeout is of peoriod", timeout_multi);
  cmd.AddValue("packet_loss", "Packet loss rate (1-100)", packet_loss);
  
  cmd.Parse(argc, argv);

  Simulation(num_failure, t_failure, t_max, timeout_multi, power_k, 
              r_tmp_outage, t_tmp_outage, packet_loss, topo_type, 
              n_direct_ping, n_ind_ping, input_mode, fail_mode);

  return 0;
}
