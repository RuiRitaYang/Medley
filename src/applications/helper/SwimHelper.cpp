/**
 * Medley protocol helper
 */

#include "SwimHelper.h"

namespace ns3 {

SwimHelper::SwimHelper() {
  m_factory.SetTypeId(SWIM::GetTypeId());
}

SwimHelper::SwimHelper(uint64_t running_time) {
  m_factory.SetTypeId(SWIM::GetTypeId());
  SetAttribute("MaxRunningTime", UintegerValue(running_time));
}

SwimHelper::SwimHelper(uint64_t running_time, uint32_t send_intv,
                       uint32_t to_intv, uint32_t to_check_intv) {
  m_factory.SetTypeId(SWIM::GetTypeId());
  SetAttribute("MaxRunningTIme", UintegerValue(running_time));
  SetAttribute("SendInterval", UintegerValue(send_intv));
  SetAttribute("TimeoutInterval", UintegerValue(to_intv));
  SetAttribute("TimeoutCheckInterval", UintegerValue(to_check_intv));
}

void SwimHelper::SetAttribute(std::string name, const AttributeValue &value) {
  m_factory.Set (name, value);
}

ApplicationContainer
SwimHelper::Install(NodeContainer c) {
  ApplicationContainer apps;
  for (NodeContainer::Iterator i = c.Begin(); i != c.End(); ++i) {
    Ptr<Node> node = *i;
    swim_node = m_factory.Create<SWIM> ();
    node->AddApplication(swim_node);
    apps.Add(swim_node);
  }

  return apps;
}

Ptr<SWIM> SwimHelper::GetSwim() {
  return swim_node;
}

} // namespace ns3
