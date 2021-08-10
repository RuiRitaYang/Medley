//
// Created by ruiyang on 10/31/17.
//

#ifndef PKTTYPEHEADER_H
#define PKTTYPEHEADER_H

#include "ns3/header.h"
#include "ns3/nstime.h"
#include "ns3/ipv4-address.h"


namespace ns3 {
/**
 * \ingroup swim
 *
 * \brief Packet header for swim application.
 *
 * The header is made of a 32bits type number (0 - PING, 1 - ACK,
 * 2 - INDIRECT PING, 3 - PING HELP, 4- PING HELP ACK, 5 - IND PING ACK)
 * followed by a 64-bits time stamp
 */
class PktTypeHeader : public Header {
public:
  PktTypeHeader();
  void setType(uint32_t seq);
  uint32_t getType (void) const;
  void setOriginSender(Ipv4Address addr);
  Ipv4Address getOriginSender();
  void setSuspect(Ipv4Address addr);
  Ipv4Address getSuspect();

  Time getTs (void) const;
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const;
  virtual void Print (std::ostream &os) const;
  virtual uint32_t GetSerializedSize (void) const;
  virtual void Serialize (Buffer::Iterator start) const;
  virtual uint32_t Deserialize (Buffer::Iterator start);

private:
  uint32_t m_pkt_type;  //!< packet type
  uint64_t m_ts;        //!< Timestamp
  uint8_t m_origin_sender[4];
  uint8_t m_origin_receiver[4];
};

} // ns3 namespace

#endif //PKTTYPEHEADER_H
