#include "atp-socket.h"

#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("AtpSocket");

NS_OBJECT_ENSURE_REGISTERED (AtpSocket);

TypeId
AtpSocket::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::AtpSocket")
      .SetParent<TcpSocketBase> ()
      .SetGroupName ("Internet")
      .AddConstructor<AtpSocket> ()
      .AddAttribute ("AtpWeight",
                     "Weigt for calculating ATP's alpha parameter",
                     DoubleValue (1.0 / 16.0),
                     MakeDoubleAccessor (&AtpSocket::m_g),
                     MakeDoubleChecker<double> (0.0, 1.0))
      .AddTraceSource ("AtpAlpha",
                       "Alpha parameter stands for the congestion status",
                       MakeTraceSourceAccessor (&AtpSocket::m_alpha),
                       "ns3::TracedValueCallback::Double");
  return tid;
}

TypeId
AtpSocket::GetInstanceTypeId () const
{
  return AtpSocket::GetTypeId ();
}

AtpSocket::AtpSocket (void)
  : TcpSocketBase (),
    m_g (1.0 / 16.0),
    m_alpha (1.0),
    m_ackedBytesEcn (0),
    m_ackedBytesTotal (0),
    m_alphaUpdateSeq (0),
    m_atpMaxSeq (0),
    m_ecnTransition (false),
    maxLossRate (0.0001),
    curAckRate (0.0),
    curLossRate (0.0),
    sentPackageNum (0),
    ackPackageNum (0),
    lostPackageNum (0)
{
}

AtpSocket::AtpSocket (const AtpSocket &sock)
  : TcpSocketBase (sock),
    m_g (sock.m_g),
    m_alpha (sock.m_alpha),
    m_ackedBytesEcn (sock.m_ackedBytesEcn),
    m_ackedBytesTotal (sock.m_ackedBytesTotal),
    m_alphaUpdateSeq (sock.m_alphaUpdateSeq),
    m_atpMaxSeq (sock.m_atpMaxSeq),
    m_ecnTransition (sock.m_ecnTransition),
    maxLossRate (0.0001),
    curAckRate (0.0),
    curLossRate (0.0),
    sentPackageNum (0),
    ackPackageNum (0),
    lostPackageNum (0)
{
}

void
AtpSocket::SendACK (void)
{
  NS_LOG_FUNCTION (this);

  uint8_t flag = TcpHeader::ACK;

  if (m_ecnState & ECN_CONN)
    {
      if (m_ecnTransition)
        {
          if (!(m_ecnState & ECN_TX_ECHO))
            {
              NS_LOG_DEBUG ("Sending ECN Echo.");
              flag |= TcpHeader::ECE;
            }
          m_ecnTransition = false;
        }
      else if (m_ecnState & ECN_TX_ECHO)
        {
          NS_LOG_DEBUG ("Sending ECN Echo.");
          flag |= TcpHeader::ECE;
        }
    }
  SendEmptyPacket (flag);
}

void
AtpSocket::EstimateRtt (const TcpHeader &tcpHeader)
{
  if (!(tcpHeader.GetFlags () & TcpHeader::SYN))
    {
      UpdateAlpha (tcpHeader);
    }
  TcpSocketBase::EstimateRtt (tcpHeader);
}

void
AtpSocket::UpdateRttHistory (const SequenceNumber32 &seq, uint32_t sz,
                               bool isRetransmission)
{
  // set atp max seq to highTxMark
  m_atpMaxSeq =std::max (std::max (seq + sz, m_tcb->m_highTxMark.Get ()), m_atpMaxSeq);
  TcpSocketBase::UpdateRttHistory (seq, sz, isRetransmission);
}

void
AtpSocket::UpdateAlpha (const TcpHeader &tcpHeader)
{
  NS_LOG_FUNCTION (this);
  int32_t ackedBytes = tcpHeader.GetAckNumber () - m_highRxAckMark.Get ();
  if (ackedBytes > 0)
    {
      m_ackedBytesTotal += ackedBytes;
      if (tcpHeader.GetFlags () & TcpHeader::ECE)
        {
          m_ackedBytesEcn += ackedBytes;
        }
    }
  /*
   * check for barrier indicating its time to recalculate alpha.
   * this code basically updated alpha roughly once per RTT.
   */
  if (tcpHeader.GetAckNumber () > m_alphaUpdateSeq)
    {
      m_alphaUpdateSeq = m_atpMaxSeq;
      // NS_LOG_DEBUG ("Before alpha update: " << m_alpha.Get ());
      m_ackedBytesTotal = m_ackedBytesTotal ? m_ackedBytesTotal : 1;
      m_alpha = (1 - m_g) * m_alpha + m_g * m_ackedBytesEcn / m_ackedBytesTotal;
      // NS_LOG_DEBUG ("After alpha update: " << m_alpha.Get ());
      // NS_LOG_DEBUG ("[ALPHA] " << Simulator::Now ().GetSeconds () << " " << m_alpha.Get ());
      m_ackedBytesEcn = m_ackedBytesTotal = 0;
    }
}

void
AtpSocket::DoRetransmit (void)
{
  NS_LOG_FUNCTION (this);
  // reset atp seq value to  if retransmit (why?)
  m_alphaUpdateSeq = m_atpMaxSeq = m_tcb->m_nextTxSequence;
  lostPackageNum++;
  curLossRate = (double)lostPackageNum/sentPackageNum;
  if(curLossRate >= maxLossRate){
    TcpSocketBase::DoRetransmit ();
  }
}

int
AtpSocket::Send (Ptr<Packet> p, uint32_t flags)
{
  NS_LOG_FUNCTION(this);
  sentPackageNum++;
  curLossRate = (double)lostPackageNum/sentPackageNum;
  return TcpSocketBase::Send (p, flags);
}

void
AtpSocket::NewAck (SequenceNumber32 const& ack, bool resetRTO){
  NS_LOG_FUNCTION(this);
  ackPackageNum++;
  if(sentPackageNum != 0)
    curAckRate = (double)ackPackageNum/sentPackageNum;
    NS_LOG_DEBUG("--------at NewAck r_wnd" << m_rWnd << ", lostPackageNum = " << lostPackageNum);
  TcpSocketBase::NewAck(ack, resetRTO);
}

Ptr<TcpSocketBase>
AtpSocket::Fork (void)
{
  return CopyObject<AtpSocket> (this);
}

void
AtpSocket::UpdateEcnState (const TcpHeader &tcpHeader)
{
  NS_LOG_FUNCTION (this << tcpHeader);

  if (m_ceReceived && !(m_ecnState & ECN_TX_ECHO))
    {
      // 感知到拥塞
      NS_LOG_INFO ("Congestion was experienced. Start sending ECN Echo.");
      m_ecnState |= ECN_TX_ECHO;
      m_ecnTransition = true;
      m_delAckCount = m_delAckMaxCount;
    }
  else if (!m_ceReceived && (m_ecnState & ECN_TX_ECHO))
    {
      m_ecnState &= ~ECN_TX_ECHO;
      m_ecnTransition = true;
      m_delAckCount = m_delAckMaxCount;
    }
}

uint32_t
AtpSocket::GetSsThresh (void)
{
  NS_LOG_FUNCTION (this);
  uint32_t newWnd = (1 - m_alpha / 2.0) * m_tcb->m_cWnd;
  return std::max (newWnd, 2 * m_tcb->m_segmentSize);
}

bool
AtpSocket::MarkEmptyPacket (void) const
{
  NS_LOG_FUNCTION (this);
  // mark empty packet if we use ATP && ECN is enabled
  return m_ecn;
}

} // namespace ns3
