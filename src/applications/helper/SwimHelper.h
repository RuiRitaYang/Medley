/**
 * Medley protocol helper
 */

#ifndef SWIMHELPER_H
#define SWIMHELPER_H

#include <stdint.h>
#include <string>
#include "ns3/object-factory.h"
#include "ns3/address.h"
#include "ns3/attribute.h"
#include "ns3/net-device.h"
#include "ns3/node-container.h"
#include "ns3/application-container.h"
#include "ns3/onoff-application.h"
#include "ns3/swim.h"

namespace ns3 {

class SwimHelper {
public:
  SwimHelper();
  SwimHelper(uint64_t running_time);
  SwimHelper(uint64_t running_time, uint32_t send_intv, uint32_t to_intv, uint32_t to_check_intv);
  void SetAttribute (std::string name, const AttributeValue &value);
  ApplicationContainer Install (NodeContainer c);
  Ptr<SWIM> GetSwim (void);


private:
  ObjectFactory m_factory; //!<Object factory
  Ptr<SWIM> swim_node; // !< The last created swim application
};

}


#endif //NS_3_27_SWIMHELPER_H
