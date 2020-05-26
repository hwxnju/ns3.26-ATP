#ifndef ATP_SOCKET_H
#define ATP_SOCKET_H

#include "tcp-socket-base.h"
#include "tcp-congestion-ops.h"
#include "tcp-l4-protocol.h"

namespace ns3 {

class AtpSocket : public TcpSocketBase
{
public:
  /**
   * Get the type ID.
   * \brief Get the type ID.
   * \return the object TypeId
   */

  static TypeId GetTypeId (void);

  /**
   * \brief Get the instance TypeId
   * \return the instance TypeId
   */
  virtual TypeId GetInstanceTypeId () const;

  /**
   * Create an unbound TCP socket
   */
  AtpSocket (void);

  /**
   * Clone a TCP socket, for use upon receiving a connection request in LISTEN state
   *
   * \param sock the original Tcp Socket
   */
  AtpSocket (const AtpSocket& sock);

protected:

  // inherited from TcpSocketBase
  virtual void SendACK (void);
  virtual void EstimateRtt (const TcpHeader& tcpHeader);
  virtual void UpdateRttHistory (const SequenceNumber32 &seq, uint32_t sz,
                                 bool isRetransmission);
  virtual void DoRetransmit (void);
  virtual void NewAck (SequenceNumber32 const& ack, bool resetRTO); // 重载TCP的收到ack函数
  virtual int Send (Ptr<Packet> p, uint32_t flags);                 // 重载TCP的send函数
  virtual Ptr<TcpSocketBase> Fork (void);
  virtual void UpdateEcnState (const TcpHeader &tcpHeader);
  virtual uint32_t GetSsThresh (void);
  virtual bool MarkEmptyPacket (void) const;

  void UpdateAlpha (const TcpHeader &tcpHeader);

  // ATP related params
  double m_g;   //!< atp g param
  TracedValue<double> m_alpha;   //!< atp alpha param
  uint32_t m_ackedBytesEcn; //!< acked bytes with ecn
  uint32_t m_ackedBytesTotal;   //!< acked bytes total
  SequenceNumber32 m_alphaUpdateSeq;
  SequenceNumber32 m_atpMaxSeq;
  bool m_ecnTransition; //!< ce state machine to support delayed ACK

  double maxLossRate; //maximum loss rate set by application
  double curAckRate; 
  double curLossRate;//current loss rate
  int sentPackageNum; //package num had sent
  int ackPackageNum;  //package num had ack
  int lostPackageNum;
};

} // namespace ns3

#endif // ATP_SOCKET_H
