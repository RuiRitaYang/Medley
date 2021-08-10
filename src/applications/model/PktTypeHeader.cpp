//
// Created by ruiyang on 10/31/17.
//

#include "ns3/assert.h"
#include "ns3/log.h"
#include "ns3/header.h"
#include "ns3/simulator.h"
//#include "../../../build/ns3/assert.h"
//#include "../../../build/ns3/log.h"
//#include "../../../build/ns3/header.h"
//#include "../../../build/ns3/simulator.h"
#include "PktTypeHeader.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("PktTypeHeader");

NS_OBJECT_ENSURE_REGISTERED (PktTypeHeader);

PktTypeHeader::PktTypeHeader ()
    : m_pkt_type (0),
      m_ts (Simulator::Now ().GetTimeStep ())
{
  NS_LOG_FUNCTION (this);
}

void
PktTypeHeader::setType (uint32_t pkt_type)
{
  NS_LOG_FUNCTION (this << pkt_type);
  m_pkt_type = pkt_type;
}

uint32_t
PktTypeHeader::getType (void) const
{
  NS_LOG_FUNCTION (this);
  return m_pkt_type;
}

Ipv4Address 
PktTypeHeader::getOriginSender(){
  NS_LOG_FUNCTION(this);
  return Ipv4Address::Deserialize(m_origin_sender);
}

void 
PktTypeHeader::setOriginSender(Ipv4Address addr){
  NS_LOG_FUNCTION(this);
  addr.Serialize(m_origin_sender);
}

void 
PktTypeHeader::setSuspect(Ipv4Address addr){
  NS_LOG_FUNCTION(this);
  addr.Serialize(m_origin_receiver);
}

Ipv4Address 
PktTypeHeader::getSuspect(){
  NS_LOG_FUNCTION(this);
  return Ipv4Address::Deserialize(m_origin_receiver);
}


Time
PktTypeHeader::getTs (void) const
{
  NS_LOG_FUNCTION (this);
  return TimeStep (m_ts);
}

TypeId
PktTypeHeader::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::PktTypeHeader")
      .SetParent<Header> ()
      .SetGroupName("Applications")
      .AddConstructor<PktTypeHeader> ()
  ;
  return tid;
}
TypeId
PktTypeHeader::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}
void
PktTypeHeader::Print (std::ostream &os) const
{
  NS_LOG_FUNCTION (this << &os);
  os << "(type=" << m_pkt_type << " time=" << TimeStep (m_ts).GetSeconds () << ")";
}
uint32_t
PktTypeHeader::GetSerializedSize (void) const
{
  NS_LOG_FUNCTION (this);
  return 4+8+4+4;
}

void
PktTypeHeader::Serialize (Buffer::Iterator start) const
{
  NS_LOG_FUNCTION (this << &start);
  Buffer::Iterator it = start;
  it.WriteHtonU32 (m_pkt_type);
  it.WriteHtonU64 (m_ts);
  for(int i=0;i<4;i++){
    it.WriteU8(m_origin_sender[i]);
  }
  for(int i=0;i<4;i++){
    it.WriteU8(m_origin_receiver[i]);
  }
}
uint32_t
PktTypeHeader::Deserialize (Buffer::Iterator start)
{
  NS_LOG_FUNCTION (this << &start);
  Buffer::Iterator it = start;
  m_pkt_type = it.ReadNtohU32 ();
  m_ts = it.ReadNtohU64 ();
  for(int i=0;i<4;i++){
    m_origin_sender[i] = it.ReadU8();
  }
  for(int i=0;i<4;i++){
    m_origin_receiver[i] = it.ReadU8();
  }
  return GetSerializedSize ();
}

} // namespace ns3
