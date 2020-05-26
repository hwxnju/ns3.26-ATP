#include "atp-socket-factory.h"

#include "atp-socket.h"

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (AtpSocketFactory);

TypeId
AtpSocketFactory::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::AtpSocketFactory")
      .SetParent<AtpSocketFactoryBase> ()
      .SetGroupName ("Internet")
      .AddConstructor<AtpSocketFactory> ();
  return tid;
}

Ptr<Socket>
AtpSocketFactory::CreateSocket (void)
{
  TypeIdValue congestionTypeId;
  GetTcp ()->GetAttribute ("SocketType", congestionTypeId);
  Ptr<Socket> socket = GetTcp ()->CreateSocket (congestionTypeId.Get (), AtpSocket::GetTypeId ());
  socket->SetAttribute ("UseEcn", BooleanValue (true));
  return socket;
}

} // namespace ns3
