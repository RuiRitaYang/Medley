/**
 * Medley protocol
 */


#include "swim.h"
#include "PktTypeHeader.h"

namespace ns3{
NS_LOG_COMPONENT_DEFINE ("SWIM");

NS_OBJECT_ENSURE_REGISTERED(SWIM);

TypeId
SWIM::GetTypeId (void) {
  static TypeId tid = TypeId("ns3::SWIM")
      .SetParent<Application> ()
      .SetGroupName("Applications")
      .AddConstructor<SWIM> ()
      .AddAttribute ("SendInterval",
                 "How long the hearbeat sent once, in s",
                 UintegerValue(20),
                 MakeUintegerAccessor (&SWIM::m_send_interval),
                 MakeUintegerChecker<uint32_t>())
      .AddAttribute ("SuspectInverval",
                     "How long to timeout, in s",
                     UintegerValue(80),
                     MakeUintegerAccessor (&SWIM::m_timeout),
                     MakeUintegerChecker<uint32_t>())
      .AddAttribute ("TimeoutCheckInterval",
                     "How freqent to check timeout, in s",
                     UintegerValue(3),
                     MakeUintegerAccessor (&SWIM::m_check_timeout_intv),
                     MakeUintegerChecker<uint32_t>())
      .AddAttribute ("MaxRunningTime",
                     "Maximum running time s",
                     UintegerValue(50000),
                     MakeUintegerAccessor (&SWIM::t_running),
                     MakeUintegerChecker<uint64_t>())
      .AddAttribute ("ProbPowerParameter",
                     "Power param k of probability",
                     DoubleValue(3.0),
                     MakeDoubleAccessor (&SWIM::m_param_k),
                     MakeDoubleChecker<double>(-10.0, 100000000000.0))
      //!< Failure attributes
      .AddAttribute ("TmpOutageProb",
                     "Probability to meet temporary meet outage",
                     DoubleValue(0.0),
                     MakeDoubleAccessor (&SWIM::m_tmp_outage_rate),
                     MakeDoubleChecker<double>(0.0, 1.0))
      .AddAttribute ("MaxTmpOutageInterval",
                     "Maximal Recover time for Temporary Outage (Uniform), in ms",
                     UintegerValue(50),
                     MakeUintegerAccessor (&SWIM::m_tmp_outage_tmax),
                     MakeUintegerChecker<uint32_t>())
      .AddAttribute ("PacketLossRate",
                     "PacketLossRate",
                     UintegerValue(10),
                     MakeUintegerAccessor (&SWIM::m_packetLoss),
                     MakeUintegerChecker<uint32_t>())
  ;
  return tid;
}

SWIM::SWIM()
  : socket_send (0),
    socket_recv (0),
    m_connected (false),
    m_failed(false),
    port_recv (7000),
    port_send (8000),
    m_address (Ipv4Address::GetAny()),
    m_rtt(2.0),
    t_failure (60000),
    t_start (Simulator::Now ().GetTimeStep()),
    n_direct_ping (1), 
    n_ind_ping (3)
{
  port_send = 8000;
  m_connected = false;
}

SWIM::~SWIM() {
}

void SWIM::SetMyAddress(Ipv4Address own) {
  m_address = own; return;
}

void SWIM::SetFailureTime(uint64_t t_failed) {
  t_failure = t_failed; return;
}

void SWIM::SetTimeout(uint32_t multi, uint32_t timeout = 20) {
  m_send_interval = timeout;
  m_timeout = timeout * multi;
  m_check_timeout_intv = m_send_interval / 3 - 1;
  return;
}

void SWIM::SetPowerk(double k) {
  m_param_k = k; return;
}

void SWIM::SetMaxRunningTime(uint64_t t) {
  t_running = t; return;
}

void SWIM::SetTempOutagePara(double r, uint32_t t) {
  m_tmp_outage_rate = r; 
  m_tmp_outage_tmax = t;
  return;
}

void SWIM::SetNumDirectPing(uint32_t n) {
  n_direct_ping = n; return;
}

void SWIM::SetNumIndirectPing(uint32_t n) {
  n_ind_ping = n; return;
}

void SWIM::SetPacketLoss(uint32_t packetLoss){
  m_packetLoss = packetLoss; return;
}

void
SWIM::AddNeighbor(Ipv4Address neighbor, double distance) {
  if (neighborSet.find(neighbor) == neighborSet.end()) {
    neighborDistance.push_back(std::make_pair(neighbor, distance));
    neighborSet.insert(neighbor);
  }
}

std::vector<std::string> split(const std::string& s, char delimiter) {
  std::vector<std::string> tokens;
  std::string token;
  std::istringstream tokenStream(s);
  while (std::getline(tokenStream, token, delimiter)) {
    tokens.push_back(token);
  }
  return tokens;
}

std::map<Ipv4Address, std::pair<int,Time>>
deserialize(Ptr<Packet> pkt, Ipv4Address sender_address, Ipv4Address m_address){
  //PktTypeHeader pktTs;
  std::cout << m_address << " get UpdateTable content in packet with size " << pkt->GetSize()
            << " bytes from " << sender_address <<std::endl;
  std::map<Ipv4Address, std::pair<int,Time>> uTable;
  if(pkt->GetSize()){
    uint8_t *buf = new uint8_t[pkt->GetSize ()];
    pkt->CopyData(buf, pkt->GetSize ());
    std::string s = std::string((char*)buf);

    std::istringstream ss(s);
    std::vector<std::string> tokens;
    std::string token;
    while(std::getline(ss,token)){
      tokens.push_back(token);
    }

    std::vector<std::string> line;
    for(uint32_t i = 0; i < tokens.size(); i++){
      line = split(tokens[i], ',');
      char temp[128];
      int j=0;
      for(char c:line[0]){
        temp[j++] = c;
      }
      temp[j]='\0';
      Ipv4Address addr = Ipv4Address(temp);
      uTable[addr] = std::make_pair(std::stoi(line[1]), Time(line[2]));
    }
  }
  return uTable;
}

void printNeighborProb( std::vector<std::pair<Ipv4Address, double>> neighborProb, Ipv4Address m_address ) {
  std::cout << m_address << " current target size " << neighborProb.size() << std::endl;
  for (int i = 0; i < (int) neighborProb.size(); ++i) {
    std::cout << m_address << " with neighbor "
              << neighborProb[i].first
              << " with distance: " << neighborProb[i].second <<std::endl;
  }
}

Ptr<EmpiricalRandomVariable>
getEmpiricalRandomVariable (std::vector<std::pair<Ipv4Address, double>> targetProb){
  double sum_prob = 0.0, cum_prob = 0.0;
  Ptr<EmpiricalRandomVariable> rndvar = CreateObject<EmpiricalRandomVariable> ();
  for (int i = 0; i < (int) targetProb.size(); ++i ) {
    sum_prob += targetProb[i].second;
  }
  for (int i = 0; i < (int) targetProb.size() - 1; ++i) {
    cum_prob += targetProb[i].second;
    rndvar -> CDF(double(i), cum_prob/sum_prob);
  }
  rndvar -> CDF(targetProb.size() - 1, 1.0);
  return rndvar;
};
void SWIM::InitializeMembership() {
  for (unsigned int i = 0; i < neighborDistance.size(); ++i ) {
    MembershipTable[neighborDistance[i].first] = std::make_pair(STATUS_ACTIVE, Simulator::Now());
  }
}

void SWIM::UpdateUniformNeighborProb() {
  double sum_distance = 0.0;

  for (int i = 0; i < (int) neighborDistance.size(); ++i) {
    sum_distance += neighborDistance[i].second * 1.0;
  }
  
  for (int i = 0; i < (int) neighborDistance.size(); ++i) {
    neighborProb.push_back(std::make_pair(neighborDistance[i].first,
                                          neighborDistance[i].second * 1.0 / sum_distance));
  }
  return;
}

void CheckProbabilityValidity(std::vector<std::pair<Ipv4Address, double>> prob_vec) {
  double cum_prob = 0.0;
  for (int i = 0; i < (int) prob_vec.size(); ++i) {
    std::cout << prob_vec[i].first << " with prob " << prob_vec[i].second << std::endl;
    cum_prob += prob_vec[i].second;
  }
  std::cout << "Total cumulative: " << cum_prob << std::endl;
  return;
}

void PrintTable(Ipv4Address m_address, std::map<Ipv4Address, std::pair<int,Time>> table) {
  for(auto it = table.begin();it != table.end(); it ++) {
    std::cout << m_address << "---" << it->first << " " << it->second.first << " " << it->second.second.GetSeconds() << std::endl;
  }
}

void SWIM::UpdateSpatialNeighborProb(double k) {
  double sum_distance = 0.0;

  for (int i = 0; i < (int) neighborDistance.size(); ++i) {
    if (neighborSet.find(neighborDistance[i].first) == neighborSet.end()) { continue; }
    sum_distance += 1.0 / pow(neighborDistance[i].second, k);
  }
  for (int i = 0; i < (int) neighborDistance.size(); ++i) {
    if (neighborSet.find(neighborDistance[i].first) == neighborSet.end()) { continue; }
    neighborProb.push_back(std::make_pair(neighborDistance[i].first,
                                          1.0 / pow(neighborDistance[i].second, k) / sum_distance));
  }
  // CheckProbabilityValidity(neighborProb);
  return;
}

std::vector<Ipv4Address>
SWIM::ChooseRandomNeighbor() {
  Ptr<UniformRandomVariable> rndvar = CreateObject<UniformRandomVariable>();

  std::vector<Ipv4Address> vec;
  for(auto it = neighborSet.begin(); it != neighborSet.end(); it++){
    vec.push_back(*it);
  }

  for (int i= (int) neighborSet.size()-1; i>0; --i) {
    std::swap(vec[i],vec[rndvar->GetInteger(0, neighborSet.size()-1)]);
  }

  return vec;
}

std::pair<Ipv4Address, double>
ChooseSpatialNeighbor(std::vector<std::pair<Ipv4Address, double>> neighborProb) {
  double cum_prob = 0.0;
  Ptr<EmpiricalRandomVariable> rndvar = CreateObject<EmpiricalRandomVariable> ();

  /** Needs to add this manually <-- The cummulative prob will have a tiny diff to 1.0
      The difference is not seen by std::cout, but when != in random-variable-stream
      validation function, it will always judge as different number.
 **/
  for (int i = 0; i < (int) neighborProb.size() - 1; ++i) {
    cum_prob += neighborProb[i].second;
    rndvar -> CDF(double(i), cum_prob);
  }
  rndvar -> CDF(neighborProb.size() - 1, 1.0);
//  std::cout << "Random seed is: " << RngSeedManager::GetSeed() << std::endl;
  return neighborProb[rndvar->GetInteger()];
}


std::vector<Ipv4Address>
ChooseNSpatialNeighbor(int K, std::vector<std::pair<Ipv4Address, double>> neighborProb, Ipv4Address suspected_node, Ipv4Address m_address){
  std::vector<Ipv4Address> res;

  std::vector<std::pair<Ipv4Address, double>> targetProb = neighborProb;
  auto it = std::find_if( targetProb.begin(), targetProb.end(),
                          [&](const std::pair<Ipv4Address, double>& element){ return element.first == suspected_node;} );

  if (it != targetProb.end()) { targetProb.erase(it); }

  // Find reasonable K
  K = std::min(K, (int) targetProb.size());
/*  std::cout << m_address << " min of targetProb and K = " << K << std::endl;  */

  while ( (int) res.size() < K) {
    Ptr<EmpiricalRandomVariable> rndvar = getEmpiricalRandomVariable(targetProb);
    Ipv4Address dest_addr = targetProb[rndvar->GetInteger()].first;
    res.push_back(dest_addr);

/*
    std::cout << m_address << " chooses " << dest_addr << " suspecting " << suspected_node
              << " with destSet size " << res.size() << " and K = " << K << std::endl;
*/

    it = std::find_if( targetProb.begin(), targetProb.end(),
                       [&](const std::pair<Ipv4Address, double>& element){ return element.first == dest_addr;} );
    targetProb.erase(it);
/*    printNeighborProb(targetProb, m_address);*/
  }

  return res;
}

std::vector<std::pair<Ipv4Address, double>>
InitializeTBNeighborProb(std::vector<std::pair<Ipv4Address, double>> neighborProb) {
  // std::cout << m_address << " initializes Timebounded neighbor vector." << std::endl;
  std::vector<std::pair<Ipv4Address, double>> vecProb = neighborProb, res;
  Ptr<UniformRandomVariable> rndvar = CreateObject<UniformRandomVariable>();

  for (int i = 0; i < (int) neighborProb.size(); ++i) {
    int temp_rnd = rndvar->GetInteger(0, (uint32_t) vecProb.size());
    res.push_back(vecProb[temp_rnd]);
    vecProb.erase(vecProb.begin() + temp_rnd);
  }

  return res;
}

bool cmp(std::pair<Ipv4Address, double>& left, std::pair<Ipv4Address, double> right) {
  return left.second < right.second;
}

double FindMinNeighborProb(std::vector<std::pair<Ipv4Address, double>> neighborProb) {
  auto min_it = std::min_element(neighborProb.begin(), neighborProb.end(), cmp);
  return min_it->second;
}

Ipv4Address
SWIM::ChooseTimeboundedSpatialNeighbor() {
  if ( (int) neighborProbTimebounded.size() == 0) {
    neighborProbTimebounded = InitializeTBNeighborProb(neighborProb);
  }

  double min_prob = FindMinNeighborProb(neighborProb);

  Ptr<EmpiricalRandomVariable> rndvar = getEmpiricalRandomVariable(neighborProbTimebounded);
  int temp_rnd = rndvar->GetInteger();
  Ipv4Address dest_addr = neighborProbTimebounded[temp_rnd].first;

  // Modify probability vec
  double new_prob = neighborProbTimebounded[temp_rnd].second - min_prob;
  if (new_prob > 0) {
    neighborProbTimebounded[temp_rnd].second = new_prob;
  } else {
    neighborProbTimebounded.erase(neighborProbTimebounded.begin() + temp_rnd);
  }

  // printNeighborProb(neighborProbTimebounded, m_address);

  return dest_addr;
}

void SWIM::CheckOutage() {
  if (m_failed) return;
  // Temporary outage happens
  Ptr<UniformRandomVariable> rndvar = CreateObject<UniformRandomVariable>();
  double temp_rnd = rndvar->GetValue(0.0, 1.0);
  if (temp_rnd < m_tmp_outage_rate) { // The node meets temporary outage
    m_failed = true;
    // TODO: should the integer start from 1?
    uint32_t t_outage = rndvar->GetInteger(1, m_tmp_outage_tmax);
    std::cout << m_address << " is temporarily outage for " << t_outage
              << " seconds at time " << Simulator::Now().GetSeconds() << std::endl;
    Simulator::Schedule(Seconds(t_outage), &SWIM::NodeResume, this);
  }
}

void SWIM::NodeFailure() {
  std::cout << m_address << " stops at time "
            << Simulator::Now().GetSeconds() << std::endl;
  m_failed = true;
}

void SWIM::NodeResume() {
  // To prevent impossible recovery when temp outage interval overlaps actual failure time
  if (Simulator::Now().GetSeconds() >= t_failure) {  return; }
  m_failed = false;
  std::cout << m_address << " resume working at time " << Simulator::Now().GetSeconds() << std::endl;
  Simulator::Schedule(Seconds(0), &SWIM::ScheduleNextTx, this);
  Simulator::Schedule(Seconds(m_check_timeout_intv), &SWIM::TimeoutCheck, this);
}

void
SWIM::ScheduleStartEvent() {

  // Schedule Fail-stop if needed
  if (t_failure < t_running) {
    Simulator::Schedule(Seconds(t_failure), &SWIM::NodeFailure, this);
  }
  
  m_startStopEvent = Simulator::Schedule(Seconds(0), &SWIM::StartSending, this);

  // Create the socket if not already
  if (socket_send == 0) {
    TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
    socket_send = Socket::CreateSocket(GetNode(), tid);
    if (socket_send->Bind () == -1) {
      NS_FATAL_ERROR ("Failed to bind socket");
    }
  }

  Simulator::Schedule(Seconds(m_check_timeout_intv), &SWIM::TimeoutCheck, this);
  std::cout << "ScheduleStartEvent and finished" << std::endl;

}

void SWIM::StartSending () {
  ScheduleNextTx();
  ScheduleStopEvent();
}

void SWIM::ScheduleNextTx() {
  if (m_failed) { return; }
  if (Simulator::Now().GetSeconds() + m_send_interval > t_failure) { return; }

  CheckOutage();
  
  Ptr<UniformRandomVariable> rndvar = CreateObject<UniformRandomVariable>();
  int temp_rnd = rndvar->GetInteger(0, m_send_interval/10);
  m_sendEvent = Simulator::Schedule (Seconds(m_send_interval + temp_rnd), &SWIM::PullGossip, this);

}

void SWIM::PullGossip() {
  if (m_failed) { return; }
  Ipv4Address dest_addr = ChooseSpatialNeighbor(neighborProb).first;
  //  Ipv4Address dest_addr = ChooseTimeboundedSpatialNeighbor();
  Ipv4Address unused_addr;
  sendMessage(dest_addr, unused_addr, unused_addr, PING);
  ackPendingSet.insert(dest_addr);
  // if (MembershipTable.find(dest_addr) != MembershipTable.end() && 
  //     MembershipTable.find(dest_addr)->second.first != STATUS_FAILED) {  
  //   MembershipTable.find(dest_addr)->second = std::make_pair(STATUS_WAIT, Simulator::Now());
  // }
   
  ScheduleNextTx();
}

void SWIM::TimeoutCheck() {
  if (m_failed) {
    return;
  }

  Time current_time = Simulator::Now();
  for(auto it = MembershipTable.begin(), next_it = it; it!=MembershipTable.end(); it = next_it){
    next_it = it; ++next_it;
    std::pair<int,Time> info = it->second;

    if (ackPendingSet.find(it->first) == ackPendingSet.end()) { continue; }

    if (info.first == STATUS_ACTIVE && current_time.Compare(info.second + Seconds(m_rtt)) > 0){

      it->second = std::make_pair(STATUS_IND, current_time);
      std::cout << m_address << " send indirect ping-req due to " << it->first << " at time " << (Simulator::Now()).GetSeconds() << std::endl;
      
      if(neighborSet.find(it->first) != neighborSet.end()){
        sendINDPING(it->first, n_ind_ping);
      }
      continue;
    }

    if(info.first == STATUS_IND && current_time.Compare(info.second + Seconds(m_send_interval - m_rtt)) > 0){
      std::cout<< m_address << " didn't get ping & ind-ping (suspect) from " << it->first << " at "<< Simulator::Now().GetSeconds() << std::endl;
      UpdateTable[it->first] = std::make_pair(STATUS_SUSPECT, current_time);
      it->second = std::make_pair(STATUS_SUSPECT, current_time);
    }

    if(info.first == STATUS_SUSPECT && current_time.Compare(info.second + Seconds(m_timeout)) > 0){
      MembershipTable.erase(it->first);
      UpdateTable[it->first]= std::make_pair(STATUS_FAILED, current_time);
      std::cout << m_address << " consider " << it->first << " is failed at "<< Simulator::Now().GetSeconds() << std::endl;
      ackPendingSet.erase(it->first);
      neighborSet.erase(it->first);
      auto itt = std::find_if( neighborProbTimebounded.begin(), neighborProbTimebounded.end(),
                          [&](const std::pair<Ipv4Address, double>& element){ return element.first == it->first;} );
      if (itt != neighborProbTimebounded.end()) { neighborProbTimebounded.erase(itt); }
    }

  }
  // std::cout << m_address << "'s Membership Table at " << Simulator::Now().GetSeconds() << std::endl;
  // PrintTable(m_address, MembershipTable);

  Simulator::Schedule(Seconds(m_check_timeout_intv), &SWIM::TimeoutCheck, this);
}

void SWIM::UpdateMembership(std::map<Ipv4Address, std::pair<int,Time>> uTable){
  // std::cout << m_address << "'s uTable at " << Simulator::Now().GetSeconds()<< std::endl;
  // PrintTable(m_address, uTable);
  Time current_time = Simulator::Now();
  
  for(auto it = uTable.begin(), next_it = it; it != uTable.end(); it = next_it){
    next_it = it; ++next_it;
    // No such entry in the Membership Table, only when the msg is ACTIVE and new enough
    if ( MembershipTable.find(it->first) == MembershipTable.end() ) {
      if ( it->second.first != STATUS_ACTIVE || it->first == m_address) { continue; }
      // Is active, but the news is too old, skip
      if ( current_time.Compare(it->second.second + Seconds(m_send_interval + m_timeout)) > 0) { continue; }
      if ( UpdateTable[it->first].second.Compare(it->second.second) > 0) { continue; }
      MembershipTable[it->first] = it->second;
      UpdateTable[it->first] = it->second;
      std::cout << m_address << " add " << it->first << " back to neighbor." << std::endl;
      neighborSet.insert(it->first);
      UpdateSpatialNeighborProb(m_param_k);
      InitializeTBNeighborProb(neighborProb);
      continue;
    }

    //There is already an entry in the membership table
    if ( it->second.first == STATUS_FAILED && MembershipTable[it->first].first == STATUS_SUSPECT){
      std::cout << m_address << " consider " << it->first << " is failed at "<< Simulator::Now().GetSeconds() << std::endl;
      MembershipTable.erase(it->first); 
      UpdateTable[it->first] = it->second;
      ackPendingSet.erase(it->first);
      neighborSet.erase(it->first);
      auto itt = std::find_if( neighborProbTimebounded.begin(), neighborProbTimebounded.end(),
                          [&](const std::pair<Ipv4Address, double>& element){ return element.first == it->first;} );
      if (itt != neighborProbTimebounded.end()) { neighborProbTimebounded.erase(itt); }
      continue;
    }

    // If the same suspect status with different timestamp, keep the local one
    if ( (it->second.first == STATUS_SUSPECT && MembershipTable[it->first].first == STATUS_SUSPECT)) {
      continue;
    }

    Time uTableTime = it->second.second;
    if (uTableTime.Compare(MembershipTable[it->first].second) > 0 ){
      MembershipTable[it->first] = it->second;
      UpdateTable[it->first] = it->second;
      if (it->second.first == STATUS_FAILED) {
        std::cout << m_address << " consider " << it->first << " is failed at "<< Simulator::Now().GetSeconds() << std::endl;
        MembershipTable.erase(it->first); 
        ackPendingSet.erase(it->first);
        neighborSet.erase(it->first);
        auto itt = std::find_if( neighborProbTimebounded.begin(), neighborProbTimebounded.end(),
                          [&](const std::pair<Ipv4Address, double>& element){ return element.first == it->first;} );
        if (itt != neighborProbTimebounded.end()) { neighborProbTimebounded.erase(itt); }
      }
    }
  }
  // std::cout << m_address << "'s UpdateTable at " << Simulator::Now().GetSeconds()<< std::endl;
  // PrintTable(m_address, UpdateTable);
  // std::cout << m_address << "'s MembershipTable at " << Simulator::Now().GetSeconds()<< std::endl;
  // PrintTable(m_address, MembershipTable);
}

void SWIM::sendMessage(Ipv4Address dest, Ipv4Address origin_sender, Ipv4Address suspect, uint32_t type){
  if (m_failed) { return; }
  // std::cout << "In sendMessage function for " << m_address << std::endl;
  if (Ipv4Address::IsMatchingType(dest)) {
    socket_send->Connect(InetSocketAddress(Ipv4Address::ConvertFrom(dest), port_recv));
    // if (res == 0) { std::cout << "Socket connected to " << dest << std::endl; }
    // else {std::cout << "Connection failes" << std::endl; }
  } else {
    NS_FATAL_ERROR ("Invalid neighbour address type");
  }
  
  /*construct packet*/
  PktTypeHeader pktTs;
  pktTs.setType(type);
  pktTs.setOriginSender(origin_sender);
  pktTs.setSuspect(suspect);
  Ptr<Packet> p;
  std::ostringstream updatetable_ostream;
  if(UpdateTable.size() > 0){
    for (auto it = UpdateTable.begin(); it != UpdateTable.end(); it++){
      updatetable_ostream << it->first << "," << it->second.first << "," << it->second.second << '\n';
    }

    updatetable_ostream << '\0';

    p = Create<Packet> ((uint8_t*) updatetable_ostream.str().c_str(), updatetable_ostream.str().length());
  }
  else{
    p = Create<Packet> ();
  }
  p->AddHeader(pktTs);

  if ((socket_send->Send (p)) >= 0) {
    std::cout << m_address << " send msg type " << type << " to " << dest << " at "<< Simulator::Now().GetSeconds()<< std::endl;
  } else {
    std::cout << m_address << " Error while sending ping to " << Ipv4Address::ConvertFrom (dest) <<std::endl;
  }
}

void SWIM::sendINDPING(Ipv4Address suspected_node, int num_ind_revr){
  std::vector<Ipv4Address> dest_vec = ChooseNSpatialNeighbor(num_ind_revr, neighborProb, suspected_node, m_address);
//  std::vector<Ipv4Address> dest_vec = ChooseTimeboundedSpatialNeighbor(num_ind_revr, suspected_node);
  for(int i = 0; i < (int) dest_vec.size(); i++){
    sendMessage(dest_vec[i], m_address, suspected_node, IND_PING);
  }
}

void SWIM::handlePING(Ipv4Address sender_address){
  Ipv4Address unused_addr;
  std::cout << m_address << " receive PING from " <<  sender_address << " at time "<< Simulator::Now().GetSeconds() << std::endl;
  sendMessage(sender_address, m_address, unused_addr, ACKSIG);
}

void SWIM::handleACK(Ipv4Address sender_address) {
  ackPendingSet.erase(sender_address);
  std::cout << m_address << " receive ACK from " <<  sender_address << " at time "<< Simulator::Now().GetSeconds() << std::endl;
}

void SWIM::handleINDPING(Ipv4Address sender_address, Ipv4Address origin_sender, Ipv4Address suspected_node){
  std::cout << m_address << " receive IND_PING from " <<  sender_address << " at time "<< Simulator::Now().GetSeconds() << std::endl;
  if(neighborSet.find(suspected_node) == neighborSet.end()) {
    std::cout << m_address << " could not send indirect PING to " << suspected_node << " (not neighbor)." << std::endl;
    return;
  }
  sendMessage(suspected_node, origin_sender, suspected_node, PING_H);
}

void SWIM::handlePINGH(Ipv4Address sender_address, Ipv4Address origin_sender, Ipv4Address suspected_node){
  std::cout << m_address << " receive PING_H from " <<  sender_address << " at time "<< Simulator::Now().GetSeconds() << std::endl;
  sendMessage(sender_address, origin_sender, suspected_node, PING_H_ACK);
}

void SWIM::handlePINGHACK(Ipv4Address sender_address, Ipv4Address origin_sender, Ipv4Address suspected_node){
  std::cout << m_address << " receive PING_H_ACK from " <<  sender_address << " at time "<< Simulator::Now().GetSeconds() << std::endl;
  sendMessage(origin_sender, origin_sender, suspected_node, IND_PING_ACK);
}

void SWIM::handleINDPINGACK(Ipv4Address sender_address, Ipv4Address suspected_node){
  std::cout << m_address << " receive IND_PING_ACK from " <<  sender_address
            << " suspecting " << suspected_node << " at time "<< Simulator::Now().GetSeconds() << std::endl;
}

void SWIM::HandleRead(Ptr<Socket> socket) {
  if (m_failed) { return; }
  
  Ptr<UniformRandomVariable> rndvar = CreateObject<UniformRandomVariable>();
  uint32_t prob = rndvar->GetInteger(0, 100);

  Ptr<Packet> pkt;
  Address from;
  // NS_LOG_INFO("Packet loss rate: " << prob <<"/" << (100 - (int)m_packetLoss));
  while ((pkt = socket->RecvFrom (from)) && prob <= (100 - m_packetLoss)) {
    
    PktTypeHeader pktTs;
    pkt->RemoveHeader(pktTs);
    Time sender_ts = pktTs.getTs();
    Ipv4Address origin_sender = pktTs.getOriginSender();
    Ipv4Address suspected_node = pktTs.getSuspect();
    Ipv4Address sender_address = InetSocketAddress::ConvertFrom(from).GetIpv4();

    UpdateTable[sender_address] = std::make_pair(STATUS_ACTIVE, sender_ts);
    MembershipTable[sender_address] = std::make_pair(STATUS_ACTIVE, sender_ts);
    
    std::map<Ipv4Address, std::pair<int,Time>> uTable= deserialize(pkt, sender_address, m_address);
    UpdateMembership(uTable);

    switch (pktTs.getType()) {
      case PING:
        handlePING(sender_address);
        break;
      case ACKSIG:
        handleACK(sender_address);
        break;
      case IND_PING:
        handleINDPING(sender_address, origin_sender, suspected_node);
        break;
      case PING_H:
        handlePINGH(sender_address, origin_sender, suspected_node);
        break;
      case PING_H_ACK:
        handlePINGHACK(sender_address, origin_sender, suspected_node);
        break;
      case IND_PING_ACK:
        handleINDPINGACK(sender_address, suspected_node);
        break;
      default:
        NS_LOG_ERROR("Wrong received message type: " << pktTs.getType());
        break;
    }
  }
}

void SWIM::ScheduleStopEvent() {
  m_startStopEvent = Simulator::Schedule (Seconds(t_running), &SWIM::StopSending, this);
}

void SWIM::StopSending() {
  CancelEvents();
}

void SWIM::CancelEvents () {
  Simulator::Stop();
}

void
SWIM::printDistance() {
  for (int i = 0; i < (int) neighborDistance.size(); ++i) {
    std::cout << "Node " << m_address << " with neighbor "
                         << neighborDistance[i].first
                         << " with distance: " << neighborDistance[i].second <<std::endl;
  }
  std::cout << m_address 
            << " has final stop time of " << std::min(t_failure, t_running)
            << " with power k " << m_param_k 
            << " time period " << m_send_interval 
            << " suspect timeout " << m_timeout
            << " pkt no loss level " << m_packetLoss 
            << std::endl;
}

void
SWIM::StartApplication() {
  // Establish listen port
  if (socket_recv == 0) {
    // TODO: Concurrent map? or mutex?
    TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
    socket_recv = Socket::CreateSocket (GetNode (), tid);
    InetSocketAddress local = InetSocketAddress (m_address, port_recv);
    if (socket_recv->Bind (local) == -1)
    {
      NS_FATAL_ERROR ("Failed to bind socket");
    }
  }

  printDistance();

//UpdateUniformNeighborProb();
  SWIM::UpdateSpatialNeighborProb(m_param_k);

  socket_recv->SetRecvCallback (MakeCallback (&SWIM::HandleRead, this));

  InitializeMembership();

  ScheduleStartEvent();
}

void
SWIM::StopApplication() {
  CancelEvents();
  if(socket_recv != 0) {
    socket_recv->Close();
  } else {
    NS_LOG_WARN ("SWIM found null socket_recv to close in StopApplication");
  }

  if(socket_send != 0) {
    socket_recv->Close();
  } else {
    NS_LOG_WARN ("SWIM found null socket_send to close in StopApplication");
  }
}

void
SWIM::DoDispose (void) {
  socket_recv = 0;
  socket_send = 0;
  Application::DoDispose();
}
}

