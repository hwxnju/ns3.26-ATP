#include "atp-socket-factory-base.h"

#include "ns3/assert.h"

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (AtpSocketFactoryBase);

TypeId
AtpSocketFactoryBase::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::AtpSocketFactoryBase")
      .SetParent<SocketFactory> ()
      .SetGroupName ("Internet");
  return tid;
}

AtpSocketFactoryBase::AtpSocketFactoryBase ()
  : m_tcp (0)
{
}

AtpSocketFactoryBase::~AtpSocketFactoryBase ()
{
  NS_ASSERT (m_tcp == 0);
}

void
AtpSocketFactoryBase::SetTcp (Ptr<TcpL4Protocol> tcp)
{
  m_tcp = tcp;
}

Ptr<TcpL4Protocol>
AtpSocketFactoryBase::GetTcp (void)
{
  return m_tcp;
}

void
AtpSocketFactoryBase::DoDispose (void)
{
  m_tcp = 0;
  SocketFactory::DoDispose ();
}

} // namespace ns3
