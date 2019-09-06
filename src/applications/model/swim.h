/**
 * Medley protocol
 */

#ifndef SWIM_H
#define SWIM_H

#include "ns3/application.h"
#include "ns3/socket.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/nstime.h"
#include "ns3/simulator.h"
#include "ns3/packet.h"
#include "ns3/ptr.h"
#include "ns3/system-mutex.h"
#include "ns3/ipv4-address.h"
#include "ns3/random-variable-stream.h"
#include <sstream>
#include <map>
#include <time.h>
#include <vector>
#include <utility>
#include <numeric>
#include <cmath>

#define STATUS_ACTIVE   1
#define STATUS_SUSPECT  3
#define STATUS_FAILED   4
#define STATUS_IND      2

#define PING 1
#define ACKSIG 2
#define IND_PING 3
#define PING_H 4
#define PING_H_ACK 5
#define IND_PING_ACK 6

namespace ns3 {

class SWIM : public Application
{
public:
  static TypeId GetTypeId (void);
  SWIM ();
  virtual ~SWIM ();

  void AddNeighbor(Ipv4Address neighbor, double distance);
  void SetMyAddress(Ipv4Address own);
  void SetFailureTime(uint64_t t_failed);
  void SetMaxRunningTime(uint64_t t);
  void SetTimeout(uint32_t multi, uint32_t timeout);
  void SetPowerk(double k);
  void SetTempOutagePara(double r, uint32_t t);
  void SetNumDirectPing(uint32_t n);
  void SetNumIndirectPing(uint32_t n);
  void SetPacketLoss(uint32_t packetLoss);

protected:
  virtual void DoDispose (void);

private:
  void InitializeMembership();
  void UpdateUniformNeighborProb();
  void UpdateSpatialNeighborProb(double k);
  void CheckOutage();
  void NodeFailure();
  void NodeResume();
  void ScheduleStartEvent();
  void StartSending ();
  void ScheduleNextTx();
  void PullGossip();
  void TimeoutCheck();
  void HandleRead(Ptr<Socket> socket);
  void handlePING(Ipv4Address sender_address);
  void handleACK(Ipv4Address sender_address);
  void handleINDPING(Ipv4Address sender_address, Ipv4Address original_sender, Ipv4Address suspected_node);
  void handlePINGH(Ipv4Address sender_address, Ipv4Address original_sender, Ipv4Address suspected_node);
  void handlePINGHACK(Ipv4Address sender_address, Ipv4Address origin_sender, Ipv4Address suspected_node);
  void handleINDPINGACK(Ipv4Address sender_address, Ipv4Address suspected_node);
  void sendINDPING(Ipv4Address suspected_node, int num_ind_revr);
  Ipv4Address ChooseTimeboundedSpatialNeighbor();


  void UpdateMembership(std::map<Ipv4Address, std::pair<int,Time>> uTable);
  void sendMessage(Ipv4Address dest, Ipv4Address origin_sender, Ipv4Address suspect, uint32_t type);
  
  void ScheduleStopEvent();
  void StopSending();
  void CancelEvents();

  std::vector<Ipv4Address> ChooseRandomNeighbor();

  Ptr<Socket>     socket_send;       //!< Associated socket to send message
  Ptr<Socket>     socket_recv;       //!< Associated socket to listen message
  bool            m_connected;      //!< True if connected
  bool            m_failed;         /*!< True if this node is failed.
                                     *   Failure means no sending and no reaction to received pkts (still use bw)
                                     */
  uint16_t        port_recv;        //!< Port on which we listen for incoming packets.
  uint16_t        port_send;        //!< Port on which we send packets.
  Ipv4Address     m_address;
  Ipv4Address     p_address;
  uint32_t        m_send_interval;
  double          m_rtt;
  uint32_t        m_timeout;
  uint32_t        m_check_timeout_intv;
  EventId         m_startStopEvent;     //!< Event id for next start or stop event
  EventId         m_sendEvent;    //!< Event id of pending "send packet" event

  // About failure
  double          m_tmp_outage_rate;
  uint32_t        m_tmp_outage_tmax;
  uint64_t        t_failure;

  uint64_t        t_start;   //!< Program starting time
  uint64_t        t_running; //!< Maximal running time

  double        m_param_k;  //!< Probability parameter k (1/(r^k))

  uint32_t        n_direct_ping;   //!< number of direct ping per time
  uint32_t        n_ind_ping;      //!< number of indirect ping per time
  uint32_t        m_packetLoss;  //packet loss rate


  /* Prepare for more complicated implementation */
  std::set<Ipv4Address> neighborSet;
  std::set<Ipv4Address> ackPendingSet;
  std::vector<std::pair<Ipv4Address, double>> neighborDistance;
  std::vector<std::pair<Ipv4Address, double>> neighborProb;
  std::vector<std::pair<Ipv4Address, double>> neighborProbTimebounded;
  std::map<Ipv4Address, std::pair<int,Time>> MembershipTable;
  std::map<Ipv4Address, std::pair<int,Time>> UpdateTable;
  
  std::map<Ipv4Address, uint32_t> bandwidthTable;

  virtual void StartApplication (void);
  virtual void StopApplication (void);


private:
  void printDistance();
};
}

#endif //SWIM_H
