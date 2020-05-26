#ifndef ATP_SOCKET_FACTORY_H
#define ATP_SOCKET_FACTORY_H

#include "atp-socket-factory-base.h"

namespace ns3 {

class AtpSocketFactory : public AtpSocketFactoryBase
{
public:
  /**
   * Get the type ID.
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  virtual Ptr<Socket> CreateSocket (void);

};

} // namespace ns3

#endif // ATP_SOCKET_FACTORY_H
