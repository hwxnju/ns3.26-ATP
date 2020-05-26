#include "atp-socket-factory-helper.h"

#include "ns3/atp-socket-factory-base.h"
#include "ns3/object-factory.h"
#include "ns3/tcp-l4-protocol.h"

namespace ns3 {

AtpSocketFactoryHelper::~AtpSocketFactoryHelper ()
{
  m_socketFactorys.clear ();
}

void
AtpSocketFactoryHelper::AddSocketFactory (const std::string &tid)
{
  m_socketFactorys.push_back (TypeId::LookupByName (tid));
}

void
AtpSocketFactoryHelper::Install (NodeContainer nodes)
{
  for (NodeContainer::Iterator it = nodes.Begin ();
       it != nodes.End (); ++it)
    {
      Install (*it);
    }
}

void
AtpSocketFactoryHelper::Install (Ptr<Node> node)
{
  Ptr<TcpL4Protocol> tcp = node->GetObject<TcpL4Protocol> ();
  NS_ASSERT (tcp);
  ObjectFactory factory;
  for (auto &tid : m_socketFactorys)
    {
      factory.SetTypeId (tid);
      Ptr<AtpSocketFactoryBase> socketFactory = factory.Create<AtpSocketFactoryBase> ();
      NS_ASSERT (socketFactory);
      socketFactory->SetTcp (tcp);
      node->AggregateObject (socketFactory);
    }
}

} // namespace ns3
