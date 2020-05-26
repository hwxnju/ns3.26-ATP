#ifndef ATP_SOCKET_FACTORY_HELPER_H
#define ATP_SOCKET_FACTORY_HELPER_H

#include <ns3/node-container.h>
#include <ns3/node.h>
#include <ns3/ptr.h>
#include <ns3/type-id.h>

#include <string>
#include <vector>

namespace ns3 {

class AtpSocketFactoryHelper
{
public:
  virtual ~AtpSocketFactoryHelper ();

  /**
   * @brief AddSocketFactory
   * @param tid typeId of socket factory
   */
  void AddSocketFactory (const std::string &tid);

  /**
   * @brief Install socket factory to nodes
   * @param nodes
   */
  void Install (NodeContainer nodes);

  void Install (Ptr<Node> node);

private:
  /**
   * \brief Assignment operator declared private and not implemented to disallow
   * assignment and prevent the compiler from happily inserting its own.
   */
  AtpSocketFactoryHelper & operator = (const AtpSocketFactoryHelper &o);

  std::vector <TypeId> m_socketFactorys;
};

}

#endif // ATP_SOCKET_FACTORY_HELPER_H
