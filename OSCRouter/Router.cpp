// Copyright (c) 2018 Electronic Theatre Controls, Inc., http://www.etcconnect.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include "Router.h"
#include "EosTimer.h"
#include "EosUdp.h"
#include "EosTcp.h"
#include "Version.h"
#include "artnet/packets.h"
#include "streamcommon.h"
#include "psn_lib.hpp"

#ifndef WIN32
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include <sstream>
#include <iomanip>

// must be last include
#include "LeakWatcher.h"

////////////////////////////////////////////////////////////////////////////////

#define EPSILLON 0.00001f

uint16_t Router::GetDefaultPSNPort()
{
  return psn::DEFAULT_UDP_PORT;
}

QString Router::GetDefaultPSNIP()
{
  return QString::fromStdString(psn::DEFAULT_UDP_MULTICAST_ADDR);
}

////////////////////////////////////////////////////////////////////////////////

void PacketLogger::OSCParserClient_Log(const std::string &message)
{
  m_LogMsg = (m_Prefix + message);
  m_pLog->Add(m_LogType, m_LogMsg);
}

////////////////////////////////////////////////////////////////////////////////

void PacketLogger::PrintPacket(OSCParser &oscParser, const char *packet, size_t size)
{
  if (packet == nullptr || size == 0)
    return;

  if (OSCParser::IsOSCPacket(packet, size) && oscParser.PrintPacket(*this, packet, size))
    return;

  // not printed as an OSC packet, so print the raw hex contents
  const size_t MaxPrintSize = 32;
  size_t printSize = qMin(size, MaxPrintSize);

  std::stringstream ss;
  ss << std::setfill('0') << std::setw(2) << std::hex;
  for (size_t i = 0; i < printSize; ++i)
  {
    if (i != 0)
      ss << ' ';
    ss << static_cast<int>(packet[i]);
  }

  if (size > printSize)
    ss << "...";

  if (!ss.str().empty())
    OSCParserClient_Log(ss.str());
}

////////////////////////////////////////////////////////////////////////////////

EosUdpInThread::EosUdpInThread()
  : m_Run(false)
  , m_Mutex()
  , m_ItemStateTableId(ItemStateTable::sm_Invalid_Id)
  , m_State(ItemState::STATE_UNINITIALIZED)
  , m_ReconnectDelay(0)
  , m_Mute(false)
{
}

////////////////////////////////////////////////////////////////////////////////

EosUdpInThread::~EosUdpInThread()
{
  Stop();
}

////////////////////////////////////////////////////////////////////////////////

void EosUdpInThread::Start(const EosAddr &addr, QString multicastInterfaceIP, Protocol protocol, ItemStateTable::ID itemStateTableId, unsigned int reconnectDelayMS, bool mute)
{
  Stop();

  m_Addr = addr;
  if (m_Addr.ip.isEmpty())
    m_Addr.ip = QLatin1String("0.0.0.0");
  m_MulticastInterfaceIP = multicastInterfaceIP;
  if (m_MulticastInterfaceIP.isEmpty() && QHostAddress(m_Addr.ip).isMulticast())
    m_MulticastInterfaceIP = QLatin1String("0.0.0.0");
  m_Protocol = protocol;
  m_ItemStateTableId = itemStateTableId;
  m_ReconnectDelay = reconnectDelayMS;
  m_Mute = mute;
  m_Run = true;
  start();
}

////////////////////////////////////////////////////////////////////////////////

void EosUdpInThread::Stop()
{
  m_Run = false;
  wait();
}

////////////////////////////////////////////////////////////////////////////////

void EosUdpInThread::Flush(EosLog::LOG_Q &logQ, RECV_Q &recvQ)
{
  recvQ.clear();

  m_Mutex.lock();
  m_Log.Flush(logQ);
  m_Q.swap(recvQ);
  m_Mutex.unlock();
}

////////////////////////////////////////////////////////////////////////////////

ItemState::EnumState EosUdpInThread::GetState()
{
  ItemState::EnumState state;
  m_Mutex.lock();
  state = m_State;
  m_Mutex.unlock();
  return state;
}

////////////////////////////////////////////////////////////////////////////////

void EosUdpInThread::SetState(ItemState::EnumState state)
{
  m_Mutex.lock();
  m_State = state;
  m_Mutex.unlock();
}

////////////////////////////////////////////////////////////////////////////////

void EosUdpInThread::RecvPacket(const QHostAddress &host, const char *data, int len, OSCParser &logParser, PacketLogger &packetLogger)
{
  switch (m_Protocol)
  {
    case Protocol::kPSN: RecvPacketPSN(host, data, len, logParser, packetLogger); break;
    default: QueuePacket(host, data, len, logParser, packetLogger); break;
  }
}

////////////////////////////////////////////////////////////////////////////////

void EosUdpInThread::RecvPacketPSN(const QHostAddress &host, const char *data, int len, OSCParser &logParser, PacketLogger &packetLogger)
{
  if (!m_PSNDecoder->decode(data, static_cast<size_t>(len)))
    return;  // could not decode psn packet

  if (m_PSNFrame.has_value() && m_PSNFrame.value() == m_PSNDecoder->get_data().header.frame_id)
    return;  // already recevied this psn frame

  m_PSNFrame = m_PSNDecoder->get_data().header.frame_id;

  const psn::tracker_map &trackers = m_PSNDecoder->get_data().trackers;
  for (psn::tracker_map::const_iterator trackerIter = trackers.begin(); trackerIter != trackers.end(); ++trackerIter)
  {
    const psn::tracker &tracker = trackerIter->second;
    std::string path = "/psn/" + std::to_string(tracker.get_id());

    std::string completePath = path;
    OSCPacketWriter completeOSC;
    int count = 0;

    if (tracker.is_pos_set())
    {
      OSCPacketWriter osc(path + "/pos");
      completePath += "/pos";
      osc.AddFloat32(tracker.get_pos().x);
      osc.AddFloat32(tracker.get_pos().y);
      osc.AddFloat32(tracker.get_pos().z);
      completeOSC.AddFloat32(tracker.get_pos().x);
      completeOSC.AddFloat32(tracker.get_pos().y);
      completeOSC.AddFloat32(tracker.get_pos().z);
      size_t size = 0;
      char *packet = osc.Create(size);
      if (packet)
      {
        if (size > 0)
        {
          QueuePacket(host, packet, static_cast<int>(size), logParser, packetLogger);
          ++count;
        }
        delete[] packet;
      }
    }

    if (tracker.is_speed_set())
    {
      OSCPacketWriter osc(path + "/speed");
      completePath += "/speed";
      osc.AddFloat32(tracker.get_speed().x);
      osc.AddFloat32(tracker.get_speed().y);
      osc.AddFloat32(tracker.get_speed().z);
      completeOSC.AddFloat32(tracker.get_speed().x);
      completeOSC.AddFloat32(tracker.get_speed().y);
      completeOSC.AddFloat32(tracker.get_speed().z);
      size_t size = 0;
      char *packet = osc.Create(size);
      if (packet)
      {
        if (size > 0)
        {
          QueuePacket(host, packet, static_cast<int>(size), logParser, packetLogger);
          ++count;
        }
        delete[] packet;
      }
    }

    if (tracker.is_ori_set())
    {
      OSCPacketWriter osc(path + "/orientation");
      completePath += "/orientation";
      osc.AddFloat32(tracker.get_ori().x);
      osc.AddFloat32(tracker.get_ori().y);
      osc.AddFloat32(tracker.get_ori().z);
      completeOSC.AddFloat32(tracker.get_ori().x);
      completeOSC.AddFloat32(tracker.get_ori().y);
      completeOSC.AddFloat32(tracker.get_ori().z);
      size_t size = 0;
      char *packet = osc.Create(size);
      if (packet)
      {
        if (size > 0)
        {
          QueuePacket(host, packet, static_cast<int>(size), logParser, packetLogger);
          ++count;
        }
        delete[] packet;
      }
    }

    if (tracker.is_accel_set())
    {
      OSCPacketWriter osc(path + "/acceleration");
      completePath += "/acceleration";
      osc.AddFloat32(tracker.get_accel().x);
      osc.AddFloat32(tracker.get_accel().y);
      osc.AddFloat32(tracker.get_accel().z);
      completeOSC.AddFloat32(tracker.get_accel().x);
      completeOSC.AddFloat32(tracker.get_accel().y);
      completeOSC.AddFloat32(tracker.get_accel().z);
      size_t size = 0;
      char *packet = osc.Create(size);
      if (packet)
      {
        if (size > 0)
        {
          QueuePacket(host, packet, static_cast<int>(size), logParser, packetLogger);
          ++count;
        }
        delete[] packet;
      }
    }

    if (tracker.is_target_pos_set())
    {
      OSCPacketWriter osc(path + "/target");
      completePath += "/target";
      osc.AddFloat32(tracker.get_target_pos().x);
      osc.AddFloat32(tracker.get_target_pos().y);
      osc.AddFloat32(tracker.get_target_pos().z);
      completeOSC.AddFloat32(tracker.get_target_pos().x);
      completeOSC.AddFloat32(tracker.get_target_pos().y);
      completeOSC.AddFloat32(tracker.get_target_pos().z);
      size_t size = 0;
      char *packet = osc.Create(size);
      if (packet)
      {
        if (size > 0)
        {
          QueuePacket(host, packet, static_cast<int>(size), logParser, packetLogger);
          ++count;
        }
        delete[] packet;
      }
    }

    if (tracker.is_status_set())
    {
      OSCPacketWriter osc(path + "/status");
      completePath += "/status";
      osc.AddFloat32(tracker.get_status());
      completeOSC.AddFloat32(tracker.get_status());
      size_t size = 0;
      char *packet = osc.Create(size);
      if (packet)
      {
        if (size > 0)
        {
          QueuePacket(host, packet, static_cast<int>(size), logParser, packetLogger);
          ++count;
        }
        delete[] packet;
      }
    }

    if (tracker.is_status_set())
    {
      OSCPacketWriter osc(path + "/timestamp");
      completePath += "/timestamp";
      osc.AddUInt64(tracker.get_timestamp());
      completeOSC.AddUInt64(tracker.get_timestamp());
      size_t size = 0;
      char *packet = osc.Create(size);
      if (packet)
      {
        if (size > 0)
        {
          QueuePacket(host, packet, static_cast<int>(size), logParser, packetLogger);
          ++count;
        }
        delete[] packet;
      }
    }

    if (count > 1 && !completeOSC.empty())
    {
      completeOSC.SetPath(completePath);
      size_t size = 0;
      char *packet = completeOSC.Create(size);
      if (packet)
      {
        if (size > 0)
          QueuePacket(host, packet, static_cast<int>(size), logParser, packetLogger);
        delete[] packet;
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////

void EosUdpInThread::QueuePacket(const QHostAddress &host, const char *data, int len, OSCParser &logParser, PacketLogger &packetLogger)
{
  std::string logPrefix = QString("UDP IN   [%1:%2] ").arg(host.toString()).arg(m_Addr.port).toUtf8().constData();
  packetLogger.SetPrefix(logPrefix);
  packetLogger.PrintPacket(logParser, data, static_cast<size_t>(len));
  unsigned int ip = static_cast<unsigned int>(host.toIPv4Address());
  m_Mutex.lock();
  m_Q.push_back(sRecvPacket(data, len, ip));
  m_Mutex.unlock();
}

////////////////////////////////////////////////////////////////////////////////

void EosUdpInThread::run()
{
  QString msg = QString("udp in %1:%2 thread started").arg(m_Addr.ip).arg(m_Addr.port);
  m_PrivateLog.AddInfo(msg.toUtf8().constData());
  UpdateLog();

  m_PSNDecoder = new psn::psn_decoder();
  m_PSNFrame.reset();

  EosTimer reconnectTimer;

  // outer loop for auto-reconnect
  while (m_Run)
  {
    SetState(ItemState::STATE_CONNECTING);

    EosUdpIn *udpIn = EosUdpIn::Create();
    if (udpIn->Initialize(m_PrivateLog, m_Addr.ip.toUtf8().constData(), m_Addr.port, m_MulticastInterfaceIP.isEmpty() ? nullptr : m_MulticastInterfaceIP.toUtf8().constData()))
    {
      SetState(ItemState::STATE_CONNECTED);

      OSCParser logParser;
      logParser.SetRoot(new OSCMethod());
      PacketLogger packetLogger(EosLog::LOG_MSG_TYPE_RECV, m_PrivateLog);
      sockaddr_in addr;

      // run
      while (m_Run)
      {
        int len = 0;
        int addrSize = static_cast<int>(sizeof(addr));
        const char *data = udpIn->RecvPacket(m_PrivateLog, 100, 0, len, &addr, &addrSize);
        if (!m_Mute && data && len > 0)
        {
          if (m_Addr.ip.isEmpty() || m_MulticastInterfaceIP.isEmpty())
            RecvPacket(QHostAddress(reinterpret_cast<const sockaddr *>(&addr)), data, len, logParser, packetLogger);
          else
            RecvPacket(QHostAddress(m_Addr.ip), data, len, logParser, packetLogger);  // route as if we received this from the multicast IP directly
        }

        UpdateLog();

        msleep(1);
      }
    }

    delete udpIn;

    SetState(ItemState::STATE_NOT_CONNECTED);

    if (m_ReconnectDelay == 0)
      break;

    msg = QString("udp in %1:%2 reconnecting in %3...").arg(m_Addr.ip).arg(m_Addr.port).arg(m_ReconnectDelay / 1000);
    m_PrivateLog.AddInfo(msg.toUtf8().constData());
    UpdateLog();

    reconnectTimer.Start();
    while (m_Run && !reconnectTimer.GetExpired(m_ReconnectDelay))
      msleep(10);
  }

  delete m_PSNDecoder;

  msg = QString("udp in %1:%2 thread ended").arg(m_Addr.ip).arg(m_Addr.port);
  m_PrivateLog.AddInfo(msg.toUtf8().constData());
  UpdateLog();
}

////////////////////////////////////////////////////////////////////////////////

void EosUdpInThread::UpdateLog()
{
  m_Mutex.lock();
  m_Log.AddLog(m_PrivateLog);
  m_Mutex.unlock();

  m_PrivateLog.Clear();
}

////////////////////////////////////////////////////////////////////////////////

EosUdpOutThread::EosUdpOutThread()
  : m_Run(false)
  , m_ItemStateTableId(ItemStateTable::sm_Invalid_Id)
  , m_State(ItemState::STATE_UNINITIALIZED)
  , m_ReconnectDelay(0)
  , m_QEnabled(false)
{
}

////////////////////////////////////////////////////////////////////////////////

EosUdpOutThread::~EosUdpOutThread()
{
  Stop();
}

////////////////////////////////////////////////////////////////////////////////

void EosUdpOutThread::Start(const EosAddr &addr, QString multicastInterfaceIP, ItemStateTable::ID itemStateTableId, unsigned int reconnectDelayMS)
{
  Stop();

  m_Addr = addr;
  if (m_Addr.ip.isEmpty())
    m_Addr.ip = QLatin1String("0.0.0.0");
  m_MulticastInterfaceIP = multicastInterfaceIP;
  if (m_MulticastInterfaceIP.isEmpty() && QHostAddress(m_Addr.ip).isMulticast())
    m_MulticastInterfaceIP = QLatin1String("0.0.0.0");
  m_ItemStateTableId = itemStateTableId;
  m_ReconnectDelay = reconnectDelayMS;
  m_Run = true;
  m_QEnabled = true;  // q commands while on-demand thread is first starting
  start();
}

////////////////////////////////////////////////////////////////////////////////

void EosUdpOutThread::Stop()
{
  m_Run = false;
  wait();
}

////////////////////////////////////////////////////////////////////////////////

bool EosUdpOutThread::Send(const EosPacket &packet)
{
  m_Mutex.lock();
  if (m_QEnabled)
  {
    m_Q.push_back(packet);
    m_Mutex.unlock();
    return true;
  }
  m_Mutex.unlock();
  return false;
}

////////////////////////////////////////////////////////////////////////////////

void EosUdpOutThread::Flush(EosLog::LOG_Q &logQ)
{
  m_Mutex.lock();
  m_Log.Flush(logQ);
  m_Mutex.unlock();
}

////////////////////////////////////////////////////////////////////////////////

ItemState::EnumState EosUdpOutThread::GetState()
{
  ItemState::EnumState state;
  m_Mutex.lock();
  state = m_State;
  m_Mutex.unlock();
  return state;
}

////////////////////////////////////////////////////////////////////////////////

void EosUdpOutThread::SetState(ItemState::EnumState state)
{
  m_Mutex.lock();
  if (m_State != state)
  {
    m_State = state;

    switch (m_State)
    {
      case ItemState::STATE_CONNECTED: m_QEnabled = true; break;
      case ItemState::STATE_NOT_CONNECTED: m_QEnabled = false; break;
    }
  }
  m_Mutex.unlock();
}

////////////////////////////////////////////////////////////////////////////////

void EosUdpOutThread::run()
{
  QString msg = QString("udp out %1:%2 thread started").arg(m_Addr.ip).arg(m_Addr.port);
  m_PrivateLog.AddInfo(msg.toUtf8().constData());
  UpdateLog();

  EosTimer reconnectTimer;

  // outer loop for auto-reconnect
  do
  {
    SetState(ItemState::STATE_CONNECTING);

    EosUdpOut *udpOut = EosUdpOut::Create();
    if (udpOut->Initialize(m_PrivateLog, m_Addr.ip.toUtf8().constData(), m_Addr.port, m_MulticastInterfaceIP.isEmpty() ? nullptr : m_MulticastInterfaceIP.toUtf8().constData()))
    {
      SetState(ItemState::STATE_CONNECTED);

      OSCParser logParser;
      logParser.SetRoot(new OSCMethod());
      PacketLogger packetLogger(EosLog::LOG_MSG_TYPE_SEND, m_PrivateLog);
      packetLogger.SetPrefix(QString("UDP OUT  [%1:%2] ").arg(m_Addr.ip).arg(m_Addr.port).toUtf8().constData());

      // run
      EosPacket::Q q;
      while (m_Run)
      {
        m_Mutex.lock();
        m_Q.swap(q);
        m_Mutex.unlock();

        for (EosPacket::Q::iterator i = q.begin(); m_Run && i != q.end(); i++)
        {
          const char *buf = i->GetData();
          int len = i->GetSize();
          if (udpOut->SendPacket(m_PrivateLog, buf, len))
            packetLogger.PrintPacket(logParser, buf, static_cast<size_t>(len));
        }
        q.clear();

        UpdateLog();

        msleep(1);
      }
    }

    delete udpOut;

    SetState(ItemState::STATE_NOT_CONNECTED);

    if (m_ReconnectDelay == 0)
      break;

    msg = QString("udp out %1:%2 reconnecting in %3...").arg(m_Addr.ip).arg(m_Addr.port).arg(m_ReconnectDelay / 1000);
    m_PrivateLog.AddInfo(msg.toUtf8().constData());
    UpdateLog();

    reconnectTimer.Start();
    while (m_Run && !reconnectTimer.GetExpired(m_ReconnectDelay))
      msleep(10);
  } while (m_Run);

  msg = QString("udp out %1:%2 thread ended").arg(m_Addr.ip).arg(m_Addr.port);
  m_PrivateLog.AddInfo(msg.toUtf8().constData());
  UpdateLog();
}

////////////////////////////////////////////////////////////////////////////////

void EosUdpOutThread::UpdateLog()
{
  m_Mutex.lock();
  m_Log.AddLog(m_PrivateLog);
  m_Mutex.unlock();

  m_PrivateLog.Clear();
}

////////////////////////////////////////////////////////////////////////////////

EosTcpClientThread::EosTcpClientThread()
  : m_AcceptedTcp(0)
  , m_Run(false)
  , m_ItemStateTableId(ItemStateTable::sm_Invalid_Id)
  , m_FrameMode(OSCStream::FRAME_MODE_INVALID)
  , m_State(ItemState::STATE_UNINITIALIZED)
  , m_ReconnectDelay(0)
  , m_Mute(false)
{
}

////////////////////////////////////////////////////////////////////////////////

EosTcpClientThread::~EosTcpClientThread()
{
  Stop();
}

////////////////////////////////////////////////////////////////////////////////

void EosTcpClientThread::Start(const EosAddr &addr, ItemStateTable::ID itemStateTableId, OSCStream::EnumFrameMode frameMode, unsigned int reconnectDelayMS, bool mute)
{
  Start(0, addr, itemStateTableId, frameMode, reconnectDelayMS, mute);
}

////////////////////////////////////////////////////////////////////////////////

void EosTcpClientThread::Start(EosTcp *tcp, const EosAddr &addr, ItemStateTable::ID itemStateTableId, OSCStream::EnumFrameMode frameMode, unsigned int reconnectDelayMS, bool mute)
{
  Stop();

  m_AcceptedTcp = tcp;
  m_Addr = addr;
  m_ItemStateTableId = itemStateTableId;
  m_FrameMode = frameMode;
  m_ReconnectDelay = reconnectDelayMS;
  m_Mute = mute;
  m_Run = true;
  start();
}

////////////////////////////////////////////////////////////////////////////////

void EosTcpClientThread::Stop()
{
  m_Run = false;
  wait();

  if (m_AcceptedTcp)
  {
    delete m_AcceptedTcp;
    m_AcceptedTcp = 0;
  }
}

////////////////////////////////////////////////////////////////////////////////

bool EosTcpClientThread::Send(const EosPacket &packet)
{
  m_Mutex.lock();
  if (GetState() == ItemState::STATE_CONNECTED)
  {
    m_SendQ.push_back(packet);
    m_Mutex.unlock();
    return true;
  }
  m_Mutex.unlock();
  return false;
}

////////////////////////////////////////////////////////////////////////////////

bool EosTcpClientThread::SendFramed(const EosPacket &packet)
{
  m_Mutex.lock();
  if (GetState() == ItemState::STATE_CONNECTED)
  {
    size_t frameSize = packet.GetSize();
    char *frame = OSCStream::CreateFrame(m_FrameMode, packet.GetDataConst(), frameSize);
    if (frame)
    {
      m_SendQ.push_back(EosPacket(frame, static_cast<int>(frameSize)));
      m_Mutex.unlock();
      delete[] frame;
      return true;
    }
  }
  m_Mutex.unlock();
  return false;
}

////////////////////////////////////////////////////////////////////////////////

void EosTcpClientThread::Flush(EosLog::LOG_Q &logQ, EosUdpInThread::RECV_Q &recvQ)
{
  recvQ.clear();

  m_Mutex.lock();
  m_Log.Flush(logQ);
  m_RecvQ.swap(recvQ);
  m_Mutex.unlock();
}

////////////////////////////////////////////////////////////////////////////////

ItemState::EnumState EosTcpClientThread::GetState()
{
  ItemState::EnumState state;
  m_Mutex.lock();
  state = m_State;
  m_Mutex.unlock();
  return state;
}

////////////////////////////////////////////////////////////////////////////////

void EosTcpClientThread::SetState(ItemState::EnumState state)
{
  m_Mutex.lock();
  m_State = state;
  m_Mutex.unlock();
}

////////////////////////////////////////////////////////////////////////////////

void EosTcpClientThread::run()
{
  QString msg = QString("tcp client %1:%2 thread started").arg(m_Addr.ip).arg(m_Addr.port);
  m_PrivateLog.AddInfo(msg.toUtf8().constData());
  UpdateLog();

  EosTimer reconnectTimer;

  // outer loop for auto-reconnect
  while (m_Run)
  {
    SetState(ItemState::STATE_CONNECTING);

    EosTcp *tcp = (m_AcceptedTcp ? m_AcceptedTcp : EosTcp::Create());
    m_AcceptedTcp = 0;
    if (tcp->Initialize(m_PrivateLog, m_Addr.ip.toUtf8().constData(), m_Addr.port))
    {
      OSCParser logParser;
      logParser.SetRoot(new OSCMethod());
      PacketLogger inPacketLogger(EosLog::LOG_MSG_TYPE_RECV, m_PrivateLog);
      inPacketLogger.SetPrefix(QString("TCP IN  [%1:%2] ").arg(m_Addr.ip).arg(m_Addr.port).toUtf8().constData());
      PacketLogger outPacketLogger(EosLog::LOG_MSG_TYPE_SEND, m_PrivateLog);
      outPacketLogger.SetPrefix(QString("TCP OUT [%1:%2] ").arg(m_Addr.ip).arg(m_Addr.port).toUtf8().constData());

      // connect
      if (m_Run && tcp->GetConnectState() == EosTcp::CONNECT_IN_PROGRESS)
      {
        reconnectTimer.Start();
        for (;;)
        {
          tcp->Tick(m_PrivateLog);
          UpdateLog();

          if (!m_Run || tcp->GetConnectState() != EosTcp::CONNECT_IN_PROGRESS || reconnectTimer.GetExpired(m_ReconnectDelay))
            break;

          msleep(10);
        }
      }

      if (tcp->GetConnectState() == EosTcp::CONNECT_CONNECTED)
        SetState(ItemState::STATE_CONNECTED);

      UpdateLog();

      // send/recv while connected
      EosPacket::Q sendQ;
      unsigned int ip = m_Addr.toUInt();
      OSCStream recvStream(m_FrameMode);
      OSCStream sendStream(m_FrameMode);
      while (m_Run && tcp->GetConnectState() == EosTcp::CONNECT_CONNECTED)
      {
        size_t len = 0;
        const char *data = tcp->Recv(m_PrivateLog, 100, len);

        recvStream.Add(data, len);

        while (m_Run)
        {
          size_t frameSize = 0;
          char *frame = recvStream.GetNextFrame(frameSize);
          if (frame)
          {
            if (!m_Mute && frameSize != 0)
            {
              inPacketLogger.PrintPacket(logParser, frame, frameSize);
              m_Mutex.lock();
              m_RecvQ.push_back(EosUdpInThread::sRecvPacket(frame, static_cast<int>(frameSize), ip));
              m_Mutex.unlock();
            }

            delete[] frame;
          }
          else
            break;
        }

        msleep(1);

        m_Mutex.lock();
        m_SendQ.swap(sendQ);
        m_Mutex.unlock();

        for (EosPacket::Q::iterator i = sendQ.begin(); m_Run && i != sendQ.end(); i++)
        {
          data = i->GetData();
          len = static_cast<size_t>(i->GetSize());
          if (tcp->Send(m_PrivateLog, data, len))
          {
            sendStream.Reset();
            sendStream.Add(data, len);
            for (;;)
            {
              size_t frameSize = 0;
              char *frame = sendStream.GetNextFrame(frameSize);
              if (frame)
              {
                if (frameSize != 0)
                  outPacketLogger.PrintPacket(logParser, frame, frameSize);
                delete[] frame;
              }
              else
                break;
            }
          }
        }
        sendQ.clear();

        UpdateLog();

        msleep(1);
      }
    }

    delete tcp;

    SetState(ItemState::STATE_NOT_CONNECTED);

    if (m_ReconnectDelay == 0)
      break;

    msg = QString("tcp client %1:%2 reconnecting in %3...").arg(m_Addr.ip).arg(m_Addr.port).arg(m_ReconnectDelay / 1000);
    m_PrivateLog.AddInfo(msg.toUtf8().constData());
    UpdateLog();

    reconnectTimer.Start();
    while (m_Run && !reconnectTimer.GetExpired(m_ReconnectDelay))
      msleep(10);
  }

  msg = QString("tcp client %1:%2 thread ended").arg(m_Addr.ip).arg(m_Addr.port);
  m_PrivateLog.AddInfo(msg.toUtf8().constData());
  UpdateLog();
}

////////////////////////////////////////////////////////////////////////////////

void EosTcpClientThread::UpdateLog()
{
  m_Mutex.lock();
  m_Log.AddLog(m_PrivateLog);
  m_Mutex.unlock();

  m_PrivateLog.Clear();
}

////////////////////////////////////////////////////////////////////////////////

EosTcpServerThread::EosTcpServerThread()
  : m_Run(false)
  , m_ItemStateTableId(ItemStateTable::sm_Invalid_Id)
  , m_State(ItemState::STATE_UNINITIALIZED)
  , m_ReconnectDelay(0)
{
}

////////////////////////////////////////////////////////////////////////////////

EosTcpServerThread::~EosTcpServerThread()
{
  Stop();
}

////////////////////////////////////////////////////////////////////////////////

void EosTcpServerThread::Start(const EosAddr &addr, ItemStateTable::ID itemStateTableId, OSCStream::EnumFrameMode frameMode, unsigned int reconnectDelayMS)
{
  Stop();

  m_Addr = addr;
  if (m_Addr.ip.isEmpty())
    m_Addr.ip = QLatin1String("0.0.0.0");
  m_ItemStateTableId = itemStateTableId;
  m_FrameMode = frameMode;
  m_ReconnectDelay = reconnectDelayMS;
  m_Run = true;
  start();
}

////////////////////////////////////////////////////////////////////////////////

void EosTcpServerThread::Stop()
{
  m_Run = false;
  wait();
}

////////////////////////////////////////////////////////////////////////////////

void EosTcpServerThread::Flush(EosLog::LOG_Q &logQ, CONNECTION_Q &connectionQ)
{
  connectionQ.clear();

  m_Mutex.lock();
  m_Log.Flush(logQ);
  m_Q.swap(connectionQ);
  m_Mutex.unlock();
}

////////////////////////////////////////////////////////////////////////////////

ItemState::EnumState EosTcpServerThread::GetState()
{
  ItemState::EnumState state;
  m_Mutex.lock();
  state = m_State;
  m_Mutex.unlock();
  return state;
}

////////////////////////////////////////////////////////////////////////////////

void EosTcpServerThread::SetState(ItemState::EnumState state)
{
  m_Mutex.lock();
  m_State = state;
  m_Mutex.unlock();
}

////////////////////////////////////////////////////////////////////////////////

void EosTcpServerThread::run()
{
  QString msg = QString("tcp server %1:%2 thread started").arg(m_Addr.ip).arg(m_Addr.port);
  m_PrivateLog.AddInfo(msg.toUtf8().constData());
  UpdateLog();

  EosTimer reconnectTimer;

  // outer loop for auto-reconnect
  while (m_Run)
  {
    SetState(ItemState::STATE_CONNECTING);

    EosTcpServer *tcpServer = EosTcpServer::Create();
    bool initialized = (m_Addr.ip.isEmpty() ? tcpServer->Initialize(m_PrivateLog, m_Addr.port) : tcpServer->Initialize(m_PrivateLog, m_Addr.ip.toUtf8().constData(), m_Addr.port));
    if (initialized)
    {
      if (tcpServer->GetListening())
        SetState(ItemState::STATE_CONNECTED);

      sConnection connection;

      while (m_Run && tcpServer->GetListening())
      {
        sockaddr_in addr;
        int addrSize = static_cast<int>(sizeof(addr));
        connection.tcp = tcpServer->Recv(m_PrivateLog, 100, &addr, &addrSize);
        if (connection.tcp)
        {
          char *ip = inet_ntoa(addr.sin_addr);
          if (ip)
            connection.addr.ip = ip;
          else
            connection.addr.ip.clear();

          connection.addr.port = m_Addr.port;

          m_Mutex.lock();
          m_Q.push_back(connection);
          m_Mutex.unlock();

          UpdateLog();
          msleep(1);
        }
        else
        {
          UpdateLog();
          msleep(100);
        }
      }
    }

    delete tcpServer;

    SetState(ItemState::STATE_NOT_CONNECTED);

    if (m_ReconnectDelay == 0)
      break;

    msg = QString("tcp server %1:%2 reconnecting in %3...").arg(m_Addr.ip).arg(m_Addr.port).arg(m_ReconnectDelay / 1000);
    m_PrivateLog.AddInfo(msg.toUtf8().constData());
    UpdateLog();

    reconnectTimer.Start();
    while (m_Run && !reconnectTimer.GetExpired(m_ReconnectDelay))
      msleep(10);
  }

  msg = QString("tcp server %1:%2 thread ended").arg(m_Addr.ip).arg(m_Addr.port);
  m_PrivateLog.AddInfo(msg.toUtf8().constData());
  UpdateLog();
}

////////////////////////////////////////////////////////////////////////////////

void EosTcpServerThread::UpdateLog()
{
  m_Mutex.lock();
  m_Log.AddLog(m_PrivateLog);
  m_Mutex.unlock();

  m_PrivateLog.Clear();
}

////////////////////////////////////////////////////////////////////////////////

bool OSCBundleMethod::ProcessPacket(OSCParserClient & /*client*/, char *buf, size_t size)
{
  EosUdpInThread::sRecvPacket packet(buf, static_cast<int>(size), m_IP);
  m_Q.push_back(packet);
  return true;
}

////////////////////////////////////////////////////////////////////////////////

void OSCBundleMethod::Flush(EosUdpInThread::RECV_Q &q)
{
  q.clear();
  m_Q.swap(q);
}

////////////////////////////////////////////////////////////////////////////////

RouterThread::RouterThread(const Router::ROUTES &routes, const Router::CONNECTIONS &tcpConnections, const Router::Settings &settings, const ItemStateTable &itemStateTable,
                           unsigned int reconnectDelayMS)
  : m_Routes(routes)
  , m_TcpConnections(tcpConnections)
  , m_Settings(settings)
  , m_ItemStateTable(itemStateTable)
  , m_Run(true)
  , m_ReconnectDelay(reconnectDelayMS)
{
}

////////////////////////////////////////////////////////////////////////////////

RouterThread::~RouterThread()
{
  Stop();
}

////////////////////////////////////////////////////////////////////////////////

void RouterThread::Stop()
{
  m_Run = false;
  wait();
}

////////////////////////////////////////////////////////////////////////////////

void RouterThread::Sync(EosLog::LOG_Q &logQ, ItemStateTable &itemStateTable)
{
  m_Mutex.lock();
  m_Log.Flush(logQ);
  itemStateTable.Sync(m_ItemStateTable);

  m_Mutex.unlock();
}

////////////////////////////////////////////////////////////////////////////////

void RouterThread::BuildRoutes(ROUTES_BY_PORT &routesByPort, ROUTES_BY_PORT &routesBysACNUniverse, ROUTES_BY_PORT &routesByArtNetUniverse, ROUTES_BY_PORT &routesByMIDI, ROUTES_BY_PORT &routesByOTP,
                               UDP_IN_THREADS &udpInThreads, UDP_OUT_THREADS &udpOutThreads, TCP_CLIENT_THREADS &tcpClientThreads, TCP_SERVER_THREADS &tcpServerThreads)
{
  m_PrivateLog.AddInfo("Building Routing Table...");

  bool mute = GetMuteAll().incoming;

  // create TCP threads
  for (Router::CONNECTIONS::const_iterator i = m_TcpConnections.begin(); i != m_TcpConnections.end(); i++)
  {
    const Router::sConnection &tcpConnection = *i;

    if (tcpConnection.server)
    {
      if (tcpServerThreads.find(tcpConnection.addr) == tcpServerThreads.end())
      {
        EosTcpServerThread *thread = new EosTcpServerThread();
        tcpServerThreads[tcpConnection.addr] = thread;
        thread->Start(tcpConnection.addr, tcpConnection.itemStateTableId, tcpConnection.frameMode, m_ReconnectDelay);
      }
    }
    else if (QHostAddress(tcpConnection.addr.ip).toIPv4Address() != 0 && tcpClientThreads.find(tcpConnection.addr) == tcpClientThreads.end())
    {
      EosTcpClientThread *thread = new EosTcpClientThread();
      tcpClientThreads[tcpConnection.addr] = thread;
      thread->Start(tcpConnection.addr, tcpConnection.itemStateTableId, tcpConnection.frameMode, m_ReconnectDelay, mute);
    }
  }

  QHostAddress localHost(QHostAddress::LocalHost);
  for (Router::ROUTES::const_iterator i = m_Routes.begin(); i != m_Routes.end(); i++)
  {
    Router::sRoute route(*i);
    if (!route.enable)
      continue;

    QHostAddress srcAddr(route.src.addr.ip);

    ROUTES_BY_PORT *routes = &routesByPort;

    if (!ValidPort(route.dst.protocol, route.dst.addr.port))
      route.dst.addr.port = route.src.addr.port;  // no valid destination port specified, so assume same port as source

    // create udp in thread on each network interface if necessary
    if (route.src.protocol == Protocol::ksACN)
    {
      routes = &routesBysACNUniverse;
    }
    else if (route.src.protocol == Protocol::kArtNet)
    {
      routes = &routesByArtNetUniverse;
    }
    else if (route.src.protocol == Protocol::kMIDI)
    {
      routes = &routesByMIDI;
    }
    else if (route.src.protocol == Protocol::kOTP)
    {
      routes = &routesByOTP;
    }
    else if (udpInThreads.find(route.src.addr) == udpInThreads.end())
    {
      EosUdpInThread *thread = new EosUdpInThread();
      udpInThreads[route.src.addr] = thread;
      thread->Start(route.src.addr, route.src.multicastInterfaceIP, route.src.protocol, route.srcItemStateTableId, m_ReconnectDelay, mute);
    }

    // create udp out thread if known dst, and not an explicit tcp client
    if (route.dst.protocol != Protocol::ksACN && route.dst.protocol != Protocol::kArtNet && route.dst.protocol != Protocol::kMIDI && route.dst.protocol != Protocol::kOTP &&
        tcpClientThreads.find(route.dst.addr) == tcpClientThreads.end())
      CreateUdpOutThread(route.dst.addr, route.dst.multicastInterfaceIP, route.dstItemStateTableId, udpOutThreads);

    // add entry to main routing table...

    // sorted 1st by port
    ROUTES_BY_PORT::iterator portIter = routes->find(route.src.addr.port);
    if (portIter == routes->end())
    {
      ROUTES_BY_IP empty;
      portIter = routes->insert(ROUTES_BY_PORT_PAIR(route.src.addr.port, empty)).first;
    }

    // sorted 2nd by ip
    unsigned int srcIp = route.src.addr.toUInt();
    ROUTES_BY_IP &routesByIp = portIter->second;
    ROUTES_BY_IP::iterator ipIter = routesByIp.find(srcIp);
    if (ipIter == routesByIp.end())
    {
      sRoutesByIp empty;
      ipIter = routesByIp.insert(ROUTES_BY_IP_PAIR(srcIp, empty)).first;
    }

    // sorted 3rd by path
    ROUTES_BY_PATH &routesByPath = (route.src.path.contains('*') ? ipIter->second.routesByWildcardPath : ipIter->second.routesByPath);
    ROUTES_BY_PATH::iterator pathIter = routesByPath.find(route.src.path);
    if (pathIter == routesByPath.end())
    {
      ROUTE_DESTINATIONS empty;
      pathIter = routesByPath.insert(ROUTES_BY_PATH_PAIR(route.src.path, empty)).first;
    }

    // add destination
    ROUTE_DESTINATIONS &destinations = pathIter->second;
    sRouteDst routeDst;
    routeDst.label = route.label;
    routeDst.dst = route.dst;
    routeDst.srcItemStateTableId = route.srcItemStateTableId;
    routeDst.dstItemStateTableId = route.dstItemStateTableId;
    destinations.push_back(routeDst);
  }
}

////////////////////////////////////////////////////////////////////////////////

bool RouterThread::HasProtocolOutput(const ROUTES_BY_PORT &routesByPort, Protocol protocol)
{
  for (ROUTES_BY_PORT::const_iterator portIter = routesByPort.begin(); portIter != routesByPort.end(); ++portIter)
  {
    const ROUTES_BY_IP &routesByIp = portIter->second;
    for (ROUTES_BY_IP::const_iterator ipIter = routesByIp.begin(); ipIter != routesByIp.end(); ++ipIter)
    {
      if (HasProtocolOutput(ipIter->second.routesByPath, protocol) || HasProtocolOutput(ipIter->second.routesByWildcardPath, protocol))
        return true;
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////

bool RouterThread::HasProtocolOutput(const ROUTES_BY_PATH &routesByPath, Protocol protocol)
{
  for (ROUTES_BY_PATH::const_iterator pathIter = routesByPath.begin(); pathIter != routesByPath.end(); ++pathIter)
  {
    const ROUTE_DESTINATIONS &destinations = pathIter->second;
    for (ROUTE_DESTINATIONS::const_iterator dstIter = destinations.begin(); dstIter != destinations.end(); ++dstIter)
    {
      if (dstIter->dst.protocol == protocol && dstIter->dst.addr.port != 0)
        return true;
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////

void RouterThread::DestroysACN(sACN &sacn)
{
  if (sacn.server)
  {
    IPlatformStreamACNSrv::DestroyInstance(sacn.server);
    sacn.server = nullptr;
    m_PrivateLog.AddInfo(QLatin1String("sACN server destroyed").toUtf8().constData());
  }

  if (sacn.client)
  {
    IPlatformStreamACNCli::DestroyInstance(sacn.client);
    sacn.client = nullptr;
    m_PrivateLog.AddInfo(QLatin1String("sACN client destroyed").toUtf8().constData());
  }

  if (sacn.net)
  {
    IPlatformAsyncSocketServ::DestroyInstance(sacn.net);
    sacn.net = nullptr;
    m_PrivateLog.AddInfo(QLatin1String("sACN networking destroyed").toUtf8().constData());
  }

  sacn.ifaces.clear();
  sacn.output.clear();
}

////////////////////////////////////////////////////////////////////////////////

void RouterThread::DestroyArtNet(ArtNet &artnet)
{
  for (ARTNET_RECV_UNIVERSE_LIST::iterator i = artnet.inputs.begin(); i != artnet.inputs.end(); ++i)
  {
    artnet_stop(i->second.node);
    artnet_destroy(i->second.node);
  }
  artnet.inputs.clear();

  if (artnet.server)
  {
    artnet_stop(artnet.server);
    artnet_destroy(artnet.server);
    artnet.server = nullptr;
  }
}

////////////////////////////////////////////////////////////////////////////////

void RouterThread::BuildsACN(ROUTES_BY_PORT &routesByPort, ROUTES_BY_PORT &routesBysACNUniverse, ROUTES_BY_PORT &routesByArtNetUniverse, ROUTES_BY_PORT &routesByMIDI, ROUTES_BY_PORT &routesByOTP,
                             sACN &sacn)
{
  bool hasInput = !routesBysACNUniverse.empty();
  bool hasOutput = HasProtocolOutput(routesByPort, Protocol::ksACN) || HasProtocolOutput(routesBysACNUniverse, Protocol::ksACN) || HasProtocolOutput(routesByArtNetUniverse, Protocol::ksACN) ||
                   HasProtocolOutput(routesByMIDI, Protocol::ksACN) || HasProtocolOutput(routesByOTP, Protocol::ksACN);
  if (!hasInput && !hasOutput)
    return;

  sacn.net = IPlatformAsyncSocketServ::CreateInstance();
  if (sacn.net)
  {
    m_PrivateLog.AddInfo(QLatin1String("sACN networking created").toUtf8().constData());
  }
  else
  {
    m_PrivateLog.AddError(QLatin1String("sACN networking creation failed").toUtf8().constData());
    return;
  }

  if (sacn.net->Startup())
  {
    m_PrivateLog.AddInfo(QLatin1String("sACN networking started").toUtf8().constData());

    if (!m_Settings.sACNIP.isEmpty())
    {
      quint32 ip = QHostAddress(m_Settings.sACNIP).toIPv4Address();
      if (ip != 0)
      {
        std::vector<IAsyncSocketServ::netintinfo> ifaces;
        ifaces.resize(sacn.net->GetNumInterfaces());
        if (!ifaces.empty())
        {
          sacn.net->CopyInterfaceList(ifaces.data());
          for (size_t i = 0; i < ifaces.size(); ++i)
          {
            const IAsyncSocketServ::netintinfo &iface = ifaces[i];
            if (iface.addr.IsV4Address() && static_cast<quint32>(iface.addr.GetV4Address()) == ip)
              sacn.ifaces.push_back(iface.id);
          }
        }
      }
    }
  }
  else
  {
    m_PrivateLog.AddError(QLatin1String("sACN networking startup failed").toUtf8().constData());
    DestroysACN(sacn);
    return;
  }

  if (hasInput)
  {
    sacn.client = IPlatformStreamACNCli::CreateInstance();
    if (sacn.client)
    {
      m_PrivateLog.AddInfo(QLatin1String("sACN client created").toUtf8().constData());

      if (sacn.client->Startup(sacn.net, this))
      {
        m_PrivateLog.AddInfo(QLatin1String("sACN client started").toUtf8().constData());

        for (ROUTES_BY_PORT::const_iterator universeIter = routesBysACNUniverse.begin(); universeIter != routesBysACNUniverse.end(); ++universeIter)
        {
          uint16_t universeNumber = universeIter->first;
          const ROUTES_BY_IP &routesByIp = universeIter->second;

          if (sacn.client->ListenUniverse(universeNumber, sacn.GetNetIFList(), sacn.GetNetIFListSize()))
          {
            SetItemState(routesByIp, Protocol::kInvalid, ItemState::STATE_CONNECTED);
            m_PrivateLog.AddInfo(QStringLiteral("sACN client listening on universe %1").arg(universeNumber).toUtf8().constData());
          }
          else
          {
            SetItemState(routesByIp, Protocol::kInvalid, ItemState::STATE_NOT_CONNECTED);
            m_PrivateLog.AddError(QStringLiteral("sACN client listen on universe %1 failed").arg(universeNumber).toUtf8().constData());
          }
        }
      }
      else
      {
        IPlatformStreamACNCli::DestroyInstance(sacn.client);
        sacn.client = nullptr;
        m_PrivateLog.AddError(QLatin1String("sACN client startup failed").toUtf8().constData());
      }
    }
    else
      m_PrivateLog.AddError(QLatin1String("sACN client creation failed").toUtf8().constData());
  }

  if (hasOutput)
  {
    sacn.server = IPlatformStreamACNSrv::CreateInstance();
    if (sacn.server)
    {
      m_PrivateLog.AddInfo(QLatin1String("sACN server created").toUtf8().constData());

      if (sacn.server->Startup(sacn.net))
      {
        m_PrivateLog.AddInfo(QLatin1String("sACN server started").toUtf8().constData());
      }
      else
      {
        IPlatformStreamACNSrv::DestroyInstance(sacn.server);
        sacn.server = nullptr;
        m_PrivateLog.AddError(QLatin1String("sACN server startup failed").toUtf8().constData());
      }
    }
    else
      m_PrivateLog.AddError(QLatin1String("sACN server creation failed").toUtf8().constData());
  }

  if (!sacn.client)
  {
    for (ROUTES_BY_PORT::const_iterator universeIter = routesBysACNUniverse.begin(); universeIter != routesBysACNUniverse.end(); ++universeIter)
    {
      const ROUTES_BY_IP &routesByIp = universeIter->second;
      SetItemState(routesByIp, Protocol::kInvalid, ItemState::STATE_NOT_CONNECTED);
    }

    if (!sacn.server)
      DestroysACN(sacn);
  }
}

////////////////////////////////////////////////////////////////////////////////

int ArtNetRecv(artnet_node n, void *pp, void *d)
{
  artnet_packet p = reinterpret_cast<artnet_packet>(pp);
  if (!p)
    return 0;

  if (p->type != ARTNET_DMX)
    return 0;

  RouterThread::ArtNet *artnet = reinterpret_cast<RouterThread::ArtNet *>(d);
  if (!artnet)
    return 0;

  artnet->inputIPs[n] = static_cast<unsigned int>(htonl(p->from.s_addr));
  return 0;
}

int ArtNetUniverseData(artnet_node n, int port, void *d)
{
  if (!n || port != 0)
    return 0;

  RouterThread::ArtNet *artnet = reinterpret_cast<RouterThread::ArtNet *>(d);
  if (!artnet)
    return 0;

  artnet->dirty.insert(n);
  return 0;
}

void RouterThread::BuildArtNet(ROUTES_BY_PORT &routesByPort, ROUTES_BY_PORT &routesBysACNUniverse, ROUTES_BY_PORT &routesByArtNetUniverse, ROUTES_BY_PORT &routesByMIDI, ROUTES_BY_PORT &routesByOTP,
                               ArtNet &artnet)
{
  bool hasInput = !routesByArtNetUniverse.empty();
  bool hasOutput = HasProtocolOutput(routesByPort, Protocol::kArtNet) || HasProtocolOutput(routesBysACNUniverse, Protocol::kArtNet) || HasProtocolOutput(routesByArtNetUniverse, Protocol::kArtNet) ||
                   HasProtocolOutput(routesByMIDI, Protocol::kArtNet) || HasProtocolOutput(routesByOTP, Protocol::kArtNet);
  if (!hasInput && !hasOutput)
    return;

  if (hasInput)
  {
    for (ROUTES_BY_PORT::const_iterator universeIter = routesByArtNetUniverse.begin(); universeIter != routesByArtNetUniverse.end(); ++universeIter)
    {
      uint8_t universeNumber = static_cast<uint8_t>(universeIter->first);
      const ROUTES_BY_IP &routesByIp = universeIter->second;

      ARTNET_RECV_UNIVERSE_LIST::const_iterator inputIter = artnet.inputs.find(universeNumber);
      if (inputIter != artnet.inputs.end())
        continue;  // already listening on this universe

      artnet_node client = artnet_new(m_Settings.artNetIP.isEmpty() ? nullptr : m_Settings.artNetIP.toLatin1().constData(), 0);
      if (!client)
      {
        SetItemState(routesByIp, Protocol::kInvalid, ItemState::STATE_NOT_CONNECTED);
        m_PrivateLog.AddError(QStringLiteral("ArtNet client listen on universe %1 creation failed").arg(universeNumber).toUtf8().constData());
        continue;
      }

      artnet_set_short_name(client, VER_PRODUCTNAME_STR);
      artnet_set_long_name(client, VER_PRODUCTNAME_STR);
      artnet_set_port_type(client, 0, ARTNET_ENABLE_OUTPUT, ARTNET_PORT_DMX);
      if (universeNumber > 16)
      {
        artnet_set_subnet_addr(client, (universeNumber >> 4) & 0xf);
        artnet_set_port_addr(client, 0, ARTNET_OUTPUT_PORT, universeNumber & 0xf);
      }
      else
        artnet_set_port_addr(client, 0, ARTNET_OUTPUT_PORT, universeNumber);
      artnet_set_handler(client, ARTNET_RECV_HANDLER, ArtNetRecv, &artnet);

      if (artnet_set_dmx_handler(client, ArtNetUniverseData, &artnet) != ARTNET_EOK)
      {
        SetItemState(routesByIp, Protocol::kInvalid, ItemState::STATE_NOT_CONNECTED);
        m_PrivateLog.AddError(QStringLiteral("ArtNet register listen on universe %1 failed").arg(universeNumber).toUtf8().constData());
        artnet_destroy(client);
        continue;
      }

      if (artnet_start(client) != ARTNET_EOK)
      {
        SetItemState(routesByIp, Protocol::kInvalid, ItemState::STATE_NOT_CONNECTED);
        m_PrivateLog.AddError(QStringLiteral("ArtNet start listen on universe %1 failed").arg(universeNumber).toUtf8().constData());
        artnet_destroy(client);
        continue;
      }

      SetItemState(routesByIp, Protocol::kInvalid, ItemState::STATE_CONNECTED);
      m_PrivateLog.AddInfo(QStringLiteral("ArtNet started listening on universe %1").arg(universeNumber).toUtf8().constData());
      artnet.inputs[universeNumber].node = client;
    }
  }

  if (hasOutput)
  {
    artnet.server = artnet_new(m_Settings.artNetIP.isEmpty() ? nullptr : m_Settings.artNetIP.toLatin1().constData(), 0);
    if (artnet.server)
    {
      m_PrivateLog.AddInfo(QLatin1String("ArtNet server created").toUtf8().constData());

      artnet_set_node_type(artnet.server, ARTNET_RAW);
      artnet_set_short_name(artnet.server, VER_PRODUCTNAME_STR);
      artnet_set_long_name(artnet.server, VER_PRODUCTNAME_STR);

      if (artnet_start(artnet.server) != ARTNET_EOK)
      {
        m_PrivateLog.AddInfo(QLatin1String("ArtNet server startup failed").toUtf8().constData());
        artnet_destroy(artnet.server);
        artnet.server = nullptr;
      }
      else
        m_PrivateLog.AddInfo(QLatin1String("ArtNet server started").toUtf8().constData());
    }
    else
      m_PrivateLog.AddError(QLatin1String("ArtNet server creation failed").toUtf8().constData());

    ItemState::EnumState state = artnet.server ? ItemState::STATE_CONNECTED : ItemState::STATE_NOT_CONNECTED;
    SetItemState(routesByPort, Protocol::kArtNet, state);
    SetItemState(routesBysACNUniverse, Protocol::kArtNet, state);
    SetItemState(routesByArtNetUniverse, Protocol::kArtNet, state);
    SetItemState(routesByMIDI, Protocol::kArtNet, state);
    SetItemState(routesByOTP, Protocol::kArtNet, state);
  }
}

////////////////////////////////////////////////////////////////////////////////

void RouterThread::BuildMIDI(ROUTES_BY_PORT &routesByMIDI, MIDI &midi)
{
  if (routesByMIDI.empty())
    return;

  for (ROUTES_BY_PORT::const_iterator portIter = routesByMIDI.begin(); portIter != routesByMIDI.end(); ++portIter)
  {
    unsigned int port = portIter->first;
    const ROUTES_BY_IP &routesByIp = portIter->second;

    MIDI_INPUT_LIST::const_iterator inputIter = midi.inputs.find(port);
    if (inputIter != midi.inputs.end())
      continue;  // already listening on this port

    MIDIIn midiIn;

    try
    {
      std::shared_ptr<RtMidiIn> input = std::make_shared<RtMidiIn>();
      input->openPort(port, VER_PRODUCTNAME_STR);
      input->ignoreTypes(/*midiSysex*/ false, /*midiTime*/ false, /*midiSense*/ false);
      midiIn.name = input->getPortName(port);
      midiIn.midi = input;
    }
    catch (RtMidiError &error)
    {
      m_PrivateLog.AddError("RtMidiIn openPort error: " + error.getMessage());
    }

    if (!midiIn.midi)
    {
      SetItemState(routesByIp, Protocol::kInvalid, ItemState::STATE_NOT_CONNECTED);
      continue;
    }

    SetItemState(routesByIp, Protocol::kInvalid, ItemState::STATE_CONNECTED);
    m_PrivateLog.AddInfo(QStringLiteral("MIDI started listening on port %1").arg(port).toUtf8().constData());
    midi.inputs[port] = midiIn;
  }
}

////////////////////////////////////////////////////////////////////////////////

void RouterThread::BuildOTP(ROUTES_BY_PORT &routesByPort, ROUTES_BY_PORT &routesBysACNUniverse, ROUTES_BY_PORT &routesByArtNetUniverse, ROUTES_BY_PORT &routesByMIDI, ROUTES_BY_PORT &routesByOTP,
                            OTPI &otpi)
{
  bool hasInput = !routesByOTP.empty();
  bool hasOutput = HasProtocolOutput(routesByPort, Protocol::kOTP) || HasProtocolOutput(routesBysACNUniverse, Protocol::kOTP) || HasProtocolOutput(routesByArtNetUniverse, Protocol::kOTP) ||
                   HasProtocolOutput(routesByMIDI, Protocol::kOTP) || HasProtocolOutput(routesByOTP, Protocol::kOTP);

  if (hasInput)
  {
    otp::SystemSet systems;

    if (!m_Settings.otpModuleTypes.empty())
    {
      for (ROUTES_BY_PORT::const_iterator portIter = routesByOTP.begin(); portIter != routesByOTP.end(); ++portIter)
      {
        unsigned short port = portIter->first;
        if (port >= otp::kMinSystemNumber && port <= otp::kMaxSystemNumber)
          systems.insert(static_cast<otp::SystemNumber>(port));
      }
    }

    otpi.consumer.SetCallbacks(this);
    otpi.consumer.SetName(QLatin1String(VER_PRODUCTNAME_STR));
    otpi.consumer.SetModuleTypes(m_Settings.otpModuleTypes);
    otpi.consumer.Init(m_Settings.otpIP);
    otpi.consumer.SetSystems(systems);

    otp::SystemList active_systems = otpi.consumer.GetSystems();
    for (ROUTES_BY_PORT::const_iterator portIter = routesByOTP.begin(); portIter != routesByOTP.end(); ++portIter)
    {
      unsigned short port = portIter->first;
      const ROUTES_BY_IP &routesByIp = portIter->second;
      bool connected = port >= otp::kMinSystemNumber && port <= otp::kMaxSystemNumber && active_systems.find(static_cast<otp::SystemNumber>(port)) != active_systems.end();
      SetItemState(routesByIp, Protocol::kInvalid, connected ? ItemState::STATE_CONNECTED : ItemState::STATE_NOT_CONNECTED);
    }
  }

  if (hasOutput)
  {
    otpi.producer.SetCallbacks(this);
    otpi.producer.SetName(QLatin1String(VER_PRODUCTNAME_STR));
    otpi.producer.Init(m_Settings.otpIP);
  }
}

////////////////////////////////////////////////////////////////////////////////

EosUdpOutThread *RouterThread::CreateUdpOutThread(const EosAddr &addr, QString multicastInterfaceIP, ItemStateTable::ID itemStateTableId, UDP_OUT_THREADS &udpOutThreads)
{
  if (addr.port != 0 && !addr.ip.isEmpty())
  {
    UDP_OUT_THREADS::iterator i = udpOutThreads.find(addr);
    if (i == udpOutThreads.end())
    {
      EosUdpOutThread *thread = new EosUdpOutThread();
      udpOutThreads[addr] = thread;
      thread->Start(addr, multicastInterfaceIP, itemStateTableId, m_ReconnectDelay);
      return thread;
    }
    else
      return i->second;
  }

  return 0;
}

////////////////////////////////////////////////////////////////////////////////

void RouterThread::AddRoutingDestinations(bool isOSC, const QString &path, const sRoutesByIp &routesByIp, DESTINATIONS_LIST &destinations)
{
  // send to any routes with an explicit path specified
  if (isOSC && !path.isEmpty())
  {
    // exact matches
    ROUTES_BY_PATH::const_iterator pathIter = routesByIp.routesByPath.find(path);
    if (pathIter != routesByIp.routesByPath.end())
      destinations.push_back(&(pathIter->second));

    // wildcard matches
    if (!routesByIp.routesByWildcardPath.empty())
    {
      for (ROUTES_BY_PATH::const_iterator i = routesByIp.routesByWildcardPath.begin(); i != routesByIp.routesByWildcardPath.end(); i++)
      {
        if (QRegularExpression::fromWildcard(i->first, Qt::CaseSensitive, QRegularExpression::NonPathWildcardConversion).match(path).hasMatch())
          destinations.push_back(&(i->second));
      }
    }
  }

  // send to any routes without an explicit path specified
  QString noPath;
  ROUTES_BY_PATH::const_iterator pathIter = routesByIp.routesByPath.find(noPath);
  if (pathIter != routesByIp.routesByPath.end())
    destinations.push_back(&(pathIter->second));
}

////////////////////////////////////////////////////////////////////////////////

void RouterThread::ProcessRecvQ(bool muteAllOutgoing, sACN &sacn, ArtNet &artnet, MIDI &midi, OTPI &otpi, OSCParser &oscBundleParser, ROUTES_BY_PORT &routesByPort,
                                DESTINATIONS_LIST &routingDestinationList, UDP_OUT_THREADS &udpOutThreads, TCP_SERVER_THREADS &tcpServerThreads, TCP_CLIENT_THREADS &tcpClientThreads,
                                const EosAddr &addr, EosUdpInThread::RECV_Q &recvQ)
{
  for (EosUdpInThread::RECV_Q::iterator i = recvQ.begin(); i != recvQ.end(); i++)
  {
    EosUdpInThread::sRecvPacket &recvPacket = *i;

    char *buf = recvPacket.packet.GetData();
    size_t packetSize = static_cast<size_t>(std::max(0, recvPacket.packet.GetSize()));
    if (OSCParser::IsOSCPacket(buf, packetSize))
    {
      OSCBundleMethod *bundleHandler = static_cast<OSCBundleMethod *>(oscBundleParser.GetRoot());
      bundleHandler->SetIP(recvPacket.ip);
      oscBundleParser.ProcessPacket(*this, recvPacket.packet.GetData(), static_cast<size_t>(qMax(0, recvPacket.packet.GetSize())));
      EosUdpInThread::RECV_Q bundleQ;
      bundleHandler->Flush(bundleQ);
      if (!bundleQ.empty())
      {
        for (EosUdpInThread::RECV_Q::iterator j = bundleQ.begin(); j != bundleQ.end(); j++)
          ProcessRecvPacket(muteAllOutgoing, sacn, artnet, midi, otpi, routesByPort, routingDestinationList, udpOutThreads, tcpServerThreads, tcpClientThreads, addr, Protocol::kOSC, *j);

        continue;
      }
    }

    ProcessRecvPacket(muteAllOutgoing, sacn, artnet, midi, otpi, routesByPort, routingDestinationList, udpOutThreads, tcpServerThreads, tcpClientThreads, addr, Protocol::kInvalid, recvPacket);
  }
  recvQ.clear();
}

////////////////////////////////////////////////////////////////////////////////

void RouterThread::ProcessRecvPacket(bool muteAllOutgoing, sACN &sacn, ArtNet &artnet, MIDI &midi, OTPI &otpi, ROUTES_BY_PORT &routesByPort, DESTINATIONS_LIST &routingDestinationList,
                                     UDP_OUT_THREADS &udpOutThreads, TCP_SERVER_THREADS &tcpServerThreads, TCP_CLIENT_THREADS &tcpClientThreads, const EosAddr &addr, Protocol protocol,
                                     EosUdpInThread::sRecvPacket &recvPacket)
{
  routingDestinationList.clear();

  // find osc path null terminator
  char *buf = recvPacket.packet.GetData();
  size_t packetSize = ((recvPacket.packet.GetSize() > 0) ? static_cast<size_t>(recvPacket.packet.GetSize()) : 0);
  QString path;

  if (protocol == Protocol::kOSC)
  {
    // get OSC path
    for (size_t i = 0; i < packetSize; i++)
    {
      if (buf[i] == 0)
      {
        if (i != 0)
          path = QString::fromUtf8(buf);
        break;
      }
    }
  }

  // send to matching ports
  ROUTES_BY_PORT::const_iterator portsIter = routesByPort.find(addr.port);
  if (portsIter != routesByPort.end())
  {
    const ROUTES_BY_IP &routesByIp = portsIter->second;

    // send to matching ips
    ROUTES_BY_IP::const_iterator ipIter = routesByIp.find(recvPacket.ip);
    if (ipIter != routesByIp.end())
      AddRoutingDestinations(protocol == Protocol::kOSC, path, ipIter->second, routingDestinationList);

    // send to unspecified ips
    if (recvPacket.ip != 0)
    {
      ipIter = routesByIp.find(0);
      if (ipIter != routesByIp.end())
        AddRoutingDestinations(protocol == Protocol::kOSC, path, ipIter->second, routingDestinationList);
    }
  }

  if (!routingDestinationList.empty())
  {
    size_t argsCount = 0;
    OSCArgument *args = 0;
    if (protocol == Protocol::kOSC)
    {
      argsCount = 0xffffffff;
      args = OSCArgument::GetArgs(buf, packetSize, argsCount);
    }

    for (DESTINATIONS_LIST::const_iterator i = routingDestinationList.begin(); i != routingDestinationList.end(); i++)
    {
      const ROUTE_DESTINATIONS &destinations = **i;
      for (ROUTE_DESTINATIONS::const_iterator j = destinations.begin(); j != destinations.end(); j++)
      {
        const sRouteDst &routeDst = *j;
        SetItemActivity(routeDst.srcItemStateTableId);

        if (muteAllOutgoing || IsRouteMuted(routeDst.dstItemStateTableId))
          continue;

        EosAddr dstAddr(routeDst.dst.addr);
        if (dstAddr.ip.isEmpty())
          EosAddr::UIntToIP(recvPacket.ip, dstAddr.ip);

        // send UDP or TCP?
        bool tcp = false;
        EosTcpClientThread *tcpClient = nullptr;
        if (routeDst.dst.protocol != Protocol::kPSN && routeDst.dst.protocol != Protocol::ksACN && routeDst.dst.protocol != Protocol::kArtNet && routeDst.dst.protocol != Protocol::kMIDI &&
            routeDst.dst.protocol != Protocol::kOTP)
        {
          TCP_CLIENT_THREADS::const_iterator k = tcpClientThreads.find(dstAddr);
          if (k != tcpClientThreads.end())
          {
            tcpClient = k->second;
            tcp = true;
          }
          else if (tcpServerThreads.find(dstAddr) != tcpServerThreads.end())
            tcp = true;
        }

        if (tcp)
        {
          if (tcpClient)
          {
            if (protocol == Protocol::kOSC)
            {
              EosPacket packet;
              if (MakeOSCPacket(artnet, addr, protocol, path, routeDst, args, argsCount, packet) && tcpClient->SendFramed(packet))
              {
                SetItemActivity(routeDst.dstItemStateTableId);
                SetItemActivity(tcpClient->GetItemStateTableId());
              }
            }
            else if (tcpClient->Send(recvPacket.packet))
            {
              SetItemActivity(routeDst.dstItemStateTableId);
              SetItemActivity(tcpClient->GetItemStateTableId());
            }
          }
        }
        else if (protocol == Protocol::kOSC || protocol == Protocol::ksACN || protocol == Protocol::kArtNet || protocol == Protocol::kMIDI || protocol == Protocol::kOTP)
        {
          EosPacket oscPacket;
          MakeOSCPacket(artnet, addr, protocol, path, routeDst, args, argsCount, oscPacket);

          if (routeDst.dst.protocol == Protocol::kPSN)
          {
            EosPacket psnPacket;
            if (MakePSNPacket(oscPacket, psnPacket))
            {
              EosUdpOutThread *thread = CreateUdpOutThread(dstAddr, routeDst.dst.multicastInterfaceIP, routeDst.dstItemStateTableId, udpOutThreads);
              if (thread && thread->Send(psnPacket))
                SetItemActivity(routeDst.dstItemStateTableId);
            }
          }
          else if (routeDst.dst.protocol == Protocol::ksACN)
          {
            if (SendsACN(sacn, artnet, addr, protocol, routeDst, oscPacket))
              SetItemActivity(routeDst.dstItemStateTableId);
          }
          else if (routeDst.dst.protocol == Protocol::kArtNet)
          {
            if (SendArtNet(artnet, addr, protocol, routeDst.dst, oscPacket))
              SetItemActivity(routeDst.dstItemStateTableId);
          }
          else if (routeDst.dst.protocol == Protocol::kMIDI)
          {
            SendMIDI(midi, routeDst, oscPacket);
          }
          else if (routeDst.dst.protocol == Protocol::kOTP)
          {
            SendOTP(otpi, routeDst, oscPacket);
          }
          else if (oscPacket.GetDataConst() && oscPacket.GetSize() > 0)
          {
            EosUdpOutThread *thread = CreateUdpOutThread(dstAddr, routeDst.dst.multicastInterfaceIP, routeDst.dstItemStateTableId, udpOutThreads);
            if (thread && thread->Send(oscPacket))
              SetItemActivity(routeDst.dstItemStateTableId);
          }
        }
        else
        {
          EosUdpOutThread *thread = CreateUdpOutThread(dstAddr, routeDst.dst.multicastInterfaceIP, routeDst.dstItemStateTableId, udpOutThreads);
          if (thread && thread->Send(recvPacket.packet))
            SetItemActivity(routeDst.dstItemStateTableId);
        }
      }
    }

    if (args)
      delete[] args;
  }

  routingDestinationList.clear();
}

////////////////////////////////////////////////////////////////////////////////

bool RouterThread::MakeOSCPacket(ArtNet &artnet, const EosAddr &addr, Protocol protocol, const QString &srcPath, const sRouteDst &route, OSCArgument *args, size_t argsCount, EosPacket &packet)
{
  QString sendPath;
  if (route.dst.script)
  {
    QString error;

    if (protocol == Protocol::ksACN)
    {
      std::array<uint8_t, UNIVERSE_SIZE> dmx;
      {
        QMutexLocker locker(&m_sACNRecv.mutex);
        UNIVERSE_LIST::const_iterator universeIter = m_sACNRecv.merged.find(addr.port);
        if (universeIter != m_sACNRecv.merged.end())
        {
          const Universe &universe = universeIter->second;
          dmx = universe.dmx;
        }
      }

      error = m_ScriptEngine->evaluate(route.dst.scriptText, &m_PrivateLog, route.label, srcPath, /*args*/ nullptr, /*argsCount*/ 0, dmx.data(), dmx.size(), &packet);
    }
    else if (protocol == Protocol::kArtNet)
    {
      const uint8_t *universe = nullptr;
      size_t universeCount = 0;

      uint8_t universeNumber = static_cast<uint8_t>(addr.port);
      ARTNET_RECV_UNIVERSE_LIST::const_iterator i = artnet.inputs.find(universeNumber);
      if (i != artnet.inputs.end())
      {
        int length = 0;
        uint8_t *data = artnet_read_dmx(i->second.node, 0, &length);
        if (data && length > 0)
        {
          universe = data;
          universeCount = static_cast<size_t>(length);
        }
      }

      error = m_ScriptEngine->evaluate(route.dst.scriptText, &m_PrivateLog, route.label, srcPath, args, argsCount, universe, universeCount, &packet);
    }
    else
      error = m_ScriptEngine->evaluate(route.dst.scriptText, &m_PrivateLog, route.label, srcPath, args, argsCount, /*universe*/ nullptr, /*universeCount*/ 0, &packet);

    if (error.isEmpty())
      return true;

    OSCParserClient_Log(error.toStdString());
    return false;
  }

  MakeSendPath(artnet, addr, protocol, srcPath, route.dst.path, args, argsCount, sendPath);
  if (!sendPath.isEmpty())
  {
    size_t oscPacketSize = 0;
    char *oscPacketData = nullptr;

    int index = sendPath.indexOf('=');
    if (index > 0)
    {
      oscPacketData = OSCPacketWriter::CreateForString(sendPath.toUtf8().constData(), oscPacketSize);

      if (oscPacketData && oscPacketSize && route.dst.hasAnyTransforms())
      {
        argsCount = 1;
        args = OSCArgument::GetArgs(oscPacketData, oscPacketSize, argsCount);
        if (args)
        {
          OSCPacketWriter oscPacket(sendPath.left(index).toUtf8().constData());

          if (ApplyTransform(args[0], route.dst, oscPacket))
          {
            delete[] oscPacketData;
            oscPacketData = oscPacket.Create(oscPacketSize);
          }

          delete[] args;
        }
      }
    }
    else
    {
      OSCPacketWriter oscPacket(sendPath.toUtf8().constData());

      if (protocol != Protocol::ksACN && protocol != Protocol::kArtNet)
      {
        if (route.dst.hasAnyTransforms())
        {
          if (args && argsCount != 0)
          {
            if (!ApplyTransform(args[0], route.dst, oscPacket))
              return false;
          }
          else
            return false;
        }
        else
          oscPacket.AddOSCArgList(args, argsCount);
      }

      oscPacketData = oscPacket.Create(oscPacketSize);
    }

    if (oscPacketData && oscPacketSize)
    {
      packet = EosPacket(oscPacketData, static_cast<int>(oscPacketSize));
      delete[] oscPacketData;
      return true;
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////

bool GetFloat3(OSCArgument *args, size_t argCount, size_t index, psn::float3 &f3)
{
  if (!args || (index + 2) >= argCount)
    return false;

  if (!args[index].GetFloat(f3.x))
    return false;

  if (!args[index + 1].GetFloat(f3.y))
    return false;

  return args[index + 2].GetFloat(f3.z);
}

bool RouterThread::MakePSNPacket(EosPacket &osc, EosPacket &psn)
{
  char *data = osc.GetData();
  if (!data || osc.GetSize() < 1)
    return false;

  // find osc path null terminator
  for (int i = 0; i < osc.GetSize(); i++)
  {
    if (data[i] == 0)
    {
      if (i < 1)
        break;

      QString path = QString::fromUtf8(data[0] == '/' ? &data[1] : &data[0]);
      QStringList parts = path.split(QLatin1Char('/'));
      if (parts.size() < 2)
        break;

      if (parts[0] != QLatin1String("psn"))
        break;

      psn::tracker tracker(parts[1].toUShort());

      if (parts.size() > 2)
      {
        size_t argCount = 0xffffffff;
        OSCArgument *args = OSCArgument::GetArgs(&data[i], static_cast<size_t>(osc.GetSize()), argCount);
        size_t argIndex = 0;
        psn::float3 f3;
        for (int part = 2; part < parts.size(); ++part)
        {
          if (parts[part] == QLatin1String("pos"))
          {
            if (GetFloat3(args, argCount, argIndex, f3))
              tracker.set_pos(f3);
            argIndex += 3;
          }
          else if (parts[part] == QLatin1String("speed"))
          {
            if (GetFloat3(args, argCount, argIndex, f3))
              tracker.set_speed(f3);
            argIndex += 3;
          }
          else if (parts[part] == QLatin1String("orientation"))
          {
            if (GetFloat3(args, argCount, argIndex, f3))
              tracker.set_ori(f3);
            argIndex += 3;
          }
          else if (parts[part] == QLatin1String("acceleration"))
          {
            if (GetFloat3(args, argCount, argIndex, f3))
              tracker.set_accel(f3);
            argIndex += 3;
          }
          else if (parts[part] == QLatin1String("target"))
          {
            if (GetFloat3(args, argCount, argIndex, f3))
              tracker.set_target_pos(f3);
            argIndex += 3;
          }
          else if (parts[part] == QLatin1String("status"))
          {
            float f = 0;
            if (args && argIndex < argCount && args[argIndex].GetFloat(f))
              tracker.set_status(f);
            ++argIndex;
          }
          else if (parts[part] == QLatin1String("timestamp"))
          {
            uint64_t u = 0;
            if (args && argIndex < argCount && args[argIndex].GetUInt64(u))
              tracker.set_timestamp(u);
            ++argIndex;
          }
        }

        if (args)
          delete[] args;
      }

      psn::tracker_map trackers;
      trackers[tracker.get_id()] = tracker;

      uint64_t timestamp = 0;
      if (m_PSNEncoderTimer.isValid())
        timestamp = m_PSNEncoderTimer.elapsed();
      else
        m_PSNEncoderTimer.start();

      std::list<std::string> packets = m_PSNEncoder->encode_data(trackers, tracker.is_timestamp_set() ? tracker.get_timestamp() : timestamp);
      if (!packets.empty() && packets.front().data() && packets.front().size() != 0)
      {
        psn = EosPacket(packets.front().data(), static_cast<int>(packets.front().size()));
        return true;
      }

      break;
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////

bool RouterThread::SendsACN(sACN &sacn, ArtNet &artnet, const EosAddr &addr, Protocol protocol, const sRouteDst &routeDst, EosPacket &osc)
{
  if (!sacn.server)
    return false;

  uint16_t universeNumber = routeDst.dst.addr.port;
  if (universeNumber == 0)
    return false;

  int offset = 0;
  uint1 priority = static_cast<uint1>(DEFAULT_PRIORITY);
  bool hasPriority = false;
  bool perChannelPriority = false;
  size_t argCount = 0xffffffff;
  OSCArgument *args = nullptr;

  // find osc path null terminator
  bool sent = false;
  char *data = osc.GetData();
  if (data)
  {
    for (int i = 0; i < osc.GetSize(); i++)
    {
      if (data[i] == 0)
      {
        args = OSCArgument::GetArgs(&data[i], static_cast<size_t>(osc.GetSize()), argCount);
        QString path = QString::fromUtf8(data[0] == '/' ? &data[1] : &data[0]);
        QStringList parts = path.split(QLatin1Char('/'));
        for (int part = 0; part < parts.size(); ++part)
        {
          if (parts[part] == QLatin1String("offset"))
          {
            int numberPart = part + 1;
            if (numberPart < parts.size())
            {
              bool ok = false;
              int n = parts[numberPart].toInt(&ok);
              if (ok)
              {
                offset = std::max(0, n - 1);
                ++part;
              }
            }
          }
          else if (parts[part] == QLatin1String("priority"))
          {
            int numberPart = part + 1;
            if (numberPart < parts.size())
            {
              bool ok = false;
              int n = parts[numberPart].toInt(&ok);
              if (ok && n >= 0)
              {
                priority = static_cast<uint1>(std::min(n, 255));
                hasPriority = true;
                ++part;
              }
            }
          }
          else if (parts[part] == QLatin1String("perChannelPriority"))
          {
            int numberPart = part + 1;
            if (numberPart < parts.size())
            {
              bool ok = false;
              int n = parts[numberPart].toInt(&ok);
              if (ok && n >= 0)
              {
                priority = static_cast<uint1>(std::min(n, 255));
                hasPriority = true;
                perChannelPriority = true;
                ++part;
              }
            }
          }
        }

        break;
      }
    }
  }

  if (offset < UNIVERSE_SIZE)
  {
    SendUniverse &universe = sacn.output[universeNumber];

    static const uint1 kCIDBytes[] = {0x37, 0x6b, 0xa8, 0x33, 0x93, 0xf1, 0x4c, 0xcf, 0x91, 0xc0, 0xe1, 0x4c, 0xaf, 0x76, 0xe2, 0xd4};
    static const CID kCID(kCIDBytes);

    bool dirty = false;

    // if priority changed, must re-create universe
    if (universe.dmx.channels && !perChannelPriority && hasPriority && universe.priority != priority)
      universe.dmx = SendUniverseData();

    if (!universe.dmx.channels)
    {
      // create dmx
      uint1 *pslots = nullptr;
      uint handle = 0;
      if (sacn.server->CreateUniverse(kCID, sacn.GetNetIFList(), sacn.GetNetIFListSize(), VER_PRODUCTNAME_STR, static_cast<uint1>(priority), 0, 0, STARTCODE_DMX, universeNumber,
                                      static_cast<uint2>(UNIVERSE_SIZE), pslots, handle))
      {
        universe.priority = priority;
        universe.dmx.handle = handle;
        universe.dmx.channels = pslots;
        dirty = true;

        SetItemState(routeDst.dstItemStateTableId, ItemState::STATE_CONNECTED);
        m_PrivateLog.AddInfo(QStringLiteral("created sACN dmx output universe %1").arg(universeNumber).toUtf8().constData());
      }
    }

    if (universe.dmx.channels)
    {
      if (perChannelPriority)
      {
        bool initialize = false;
        if (!universe.channelPriority.channels)
        {
          // create perChannelPriority
          uint1 *pslots = nullptr;
          uint handle = 0;
          if (sacn.server->CreateUniverse(kCID, sacn.GetNetIFList(), sacn.GetNetIFListSize(), VER_PRODUCTNAME_STR, static_cast<uint1>(priority), 0, 0, STARTCODE_PRIORITY, universeNumber,
                                          static_cast<uint2>(UNIVERSE_SIZE), pslots, handle))
          {
            universe.channelPriority.handle = handle;
            universe.channelPriority.channels = pslots;
            initialize = true;

            m_PrivateLog.AddInfo(QStringLiteral("created sACN per channel priority output universe %1").arg(universeNumber).toUtf8().constData());
          }
        }

        if (universe.channelPriority.channels && (initialize || (hasPriority && universe.perChannelPriority != priority)))
        {
          // update perChannelPriority
          universe.perChannelPriority = priority;

          for (size_t arg = 0; arg < argCount; ++arg)
          {
            int channel = offset + static_cast<int>(arg);
            if (channel >= UNIVERSE_SIZE)
              break;

            universe.channelPriority.channels[channel] = universe.perChannelPriority;
          }

          sacn.server->SetUniversesDirty(&universe.channelPriority.handle, 1);
        }
      }
      else if (universe.channelPriority.channels)
      {
        sacn.server->DestroyUniverse(universe.channelPriority.handle);
        universe.channelPriority = SendUniverseData();

        m_PrivateLog.AddInfo(QStringLiteral("destroyed sACN per channel priority output universe %1").arg(universeNumber).toUtf8().constData());
      }

      // update dmx
      if (args && argCount != 0)
      {
        for (size_t arg = 0; arg < argCount; ++arg)
        {
          int channel = offset + static_cast<int>(arg);
          if (channel >= UNIVERSE_SIZE)
            break;

          int n = 0;
          if (!args[arg].GetInt(n))
            n = 0;

          uint1 value = static_cast<uint1>(std::clamp(n, 0, 255));
          if (universe.dmx.channels[channel] != value)
          {
            universe.dmx.channels[channel] = value;
            dirty = true;
          }
        }
      }
      else if (protocol == Protocol::ksACN)
      {
        // special case: no args, so send sACN universe
        bool found = false;
        std::array<uint8_t, UNIVERSE_SIZE> srcDMX;
        {
          QMutexLocker locker(&m_sACNRecv.mutex);
          UNIVERSE_LIST::const_iterator sACNIter = m_sACNRecv.merged.find(addr.port);
          if (sACNIter != m_sACNRecv.merged.end())
          {
            srcDMX = sACNIter->second.dmx;
            found = true;
          }
        }

        if (found)
        {
          for (int i = 0; i < static_cast<int>(srcDMX.size()); ++i)
          {
            int channel = offset + i;
            if (channel >= UNIVERSE_SIZE)
              break;

            if (universe.dmx.channels[channel] != srcDMX[channel])
            {
              universe.dmx.channels[channel] = srcDMX[channel];
              dirty = true;
            }
          }
        }
      }
      else if (protocol == Protocol::kArtNet)
      {
        // special case: no args, so send ArtNet universe
        ARTNET_RECV_UNIVERSE_LIST::const_iterator artNetIter = artnet.inputs.find(static_cast<uint8_t>(addr.port));
        if (artNetIter != artnet.inputs.end())
        {
          int srcDMXLength = 0;
          uint8_t *srcDMX = artnet_read_dmx(artNetIter->second.node, 0, &srcDMXLength);
          if (srcDMX)
          {
            for (int i = 0; i < srcDMXLength; ++i)
            {
              int channel = offset + i;
              if (channel >= UNIVERSE_SIZE)
                break;

              if (universe.dmx.channels[channel] != srcDMX[channel])
              {
                universe.dmx.channels[channel] = srcDMX[channel];
                dirty = true;
              }
            }
          }
        }
      }

      if (dirty)
      {
        sacn.server->SetUniversesDirty(&universe.dmx.handle, 1);
        sent = true;
      }
    }
  }

  if (args)
    delete[] args;

  return sent;
}

////////////////////////////////////////////////////////////////////////////////

bool RouterThread::SendArtNet(ArtNet &artnet, const EosAddr &addr, Protocol protocol, const EosRouteDst &dst, EosPacket &osc)
{
  if (!artnet.server)
    return false;

  uint8_t universeNumber = static_cast<uint8_t>(dst.addr.port);

  int offset = 0;
  size_t argCount = 0xffffffff;
  OSCArgument *args = nullptr;

  // find osc path null terminator
  bool sent = false;
  char *data = osc.GetData();
  if (data)
  {
    for (int i = 0; i < osc.GetSize(); i++)
    {
      if (data[i] == 0)
      {
        args = OSCArgument::GetArgs(&data[i], static_cast<size_t>(osc.GetSize()), argCount);
        QString path = QString::fromUtf8(data[0] == '/' ? &data[1] : &data[0]);
        QStringList parts = path.split(QLatin1Char('/'));
        for (int part = 0; part < parts.size(); ++part)
        {
          if (parts[part] == QLatin1String("offset"))
          {
            int numberPart = part + 1;
            if (numberPart < parts.size())
            {
              bool ok = false;
              int n = parts[numberPart].toInt(&ok);
              if (ok)
              {
                offset = std::max(0, n - 1);
                ++part;
              }
            }
          }
        }

        break;
      }
    }
  }

  if (offset < ARTNET_DMX_LENGTH)
  {
    ArtNetSendUniverse *universe = nullptr;
    ARTNET_SEND_UNIVERSE_LIST::iterator universeIter = artnet.output.find(universeNumber);
    if (universeIter == artnet.output.end())
    {
      m_PrivateLog.AddInfo(QStringLiteral("created ArtNet dmx output universe %1").arg(universeNumber).toUtf8().constData());
      universe = &artnet.output[universeNumber];
    }
    else
      universe = &universeIter->second;

    if (args && argCount != 0)
    {
      // update dmx
      for (size_t arg = 0; arg < argCount; ++arg)
      {
        int channel = offset + static_cast<int>(arg);
        if (channel >= static_cast<int>(universe->dmx.size()))
          break;

        int n = 0;
        if (!args[arg].GetInt(n))
          n = 0;

        uint1 value = static_cast<uint1>(std::clamp(n, 0, 255));
        if (universe->dmx[channel] != value)
        {
          universe->dmx[channel] = value;
          universe->dirty = true;
          sent = true;
        }
      }
    }
    else if (protocol == Protocol::ksACN)
    {
      // special case: no args, so send sACN universe
      bool found = false;
      std::array<uint8_t, UNIVERSE_SIZE> srcDMX;
      {
        QMutexLocker locker(&m_sACNRecv.mutex);
        UNIVERSE_LIST::const_iterator sACNIter = m_sACNRecv.merged.find(addr.port);
        if (sACNIter != m_sACNRecv.merged.end())
        {
          srcDMX = sACNIter->second.dmx;
          found = true;
        }
      }

      if (found)
      {
        for (int i = 0; i < static_cast<int>(srcDMX.size()); ++i)
        {
          int channel = offset + i;
          if (channel >= static_cast<int>(universe->dmx.size()))
            break;

          if (universe->dmx[channel] != srcDMX[channel])
          {
            universe->dmx[channel] = srcDMX[channel];
            universe->dirty = true;
            sent = true;
          }
        }
      }
    }
    else if (protocol == Protocol::kArtNet)
    {
      // special case: no args, so send ArtNet universe
      ARTNET_RECV_UNIVERSE_LIST::const_iterator artNetIter = artnet.inputs.find(static_cast<uint8_t>(addr.port));
      if (artNetIter != artnet.inputs.end())
      {
        int srcDMXLength = 0;
        uint8_t *srcDMX = artnet_read_dmx(artNetIter->second.node, 0, &srcDMXLength);
        if (srcDMX)
        {
          for (int i = 0; i < srcDMXLength; ++i)
          {
            int channel = offset + i;
            if (channel >= static_cast<int>(universe->dmx.size()))
              break;

            if (universe->dmx[channel] != srcDMX[channel])
            {
              universe->dmx[channel] = srcDMX[channel];
              universe->dirty = true;
              sent = true;
            }
          }
        }
      }
    }
  }

  if (args)
    delete[] args;

  return sent;
}

////////////////////////////////////////////////////////////////////////////////

void RouterThread::SendMIDI(MIDI &midi, const sRouteDst &routeDst, EosPacket &oscPacket)
{
  if (!oscPacket.GetData() || oscPacket.GetSize() < 1)
    return;

  MIDI_OUTPUT_LIST::iterator portIter = midi.outputs.find(routeDst.dst.addr.port);
  if (portIter == midi.outputs.end())
  {
    MIDIOut midiOut;

    try
    {
      midiOut.midi = std::make_shared<RtMidiOut>();
      midiOut.midi->openPort(routeDst.dst.addr.port, VER_PRODUCTNAME_STR);
      midiOut.name = midiOut.midi->getPortName(routeDst.dst.addr.port);
    }
    catch (RtMidiError &error)
    {
      m_PrivateLog.AddError("RtMidiOut openPort error: " + error.getMessage());
      return;
    }

    portIter = midi.outputs.insert(std::make_pair(routeDst.dst.addr.port, midiOut)).first;
    SetItemState(routeDst.dstItemStateTableId, ItemState::STATE_CONNECTED);
  }

  // find osc path null terminator
  QString path;
  size_t argCount = 0xffffffff;
  OSCArgument *args = nullptr;
  char *data = oscPacket.GetData();
  if (data)
  {
    for (int i = 0; i < oscPacket.GetSize(); i++)
    {
      if (data[i] == 0)
      {
        path = QString::fromUtf8(data);
        args = OSCArgument::GetArgs(&data[i], static_cast<size_t>(oscPacket.GetSize()), argCount);
        break;
      }
    }
  }

  std::vector<unsigned char> message;

  QStringList parts = path.split(QLatin1Char('/'));
  qDebug() << parts;
  if (parts.size() > 1 && parts[1] == QLatin1String("msc"))
  {
    message.push_back(static_cast<unsigned char>(MSC::kSysEx));
    message.push_back(static_cast<unsigned char>(MSC::kSysExStart));
    message.push_back((parts.size() > 2) ? static_cast<unsigned char>(parts[2].toInt()) : static_cast<unsigned char>(0x01u));
    message.push_back(static_cast<unsigned char>(MSC::kMSC));
    message.push_back((parts.size() > 3) ? static_cast<unsigned char>(parts[3].toInt()) : static_cast<unsigned char>(MSC::kLightingFormat));

    MSCCmd cmd = MSCCmd::kGo;
    if (parts.size() > 4)
    {
      std::optional<MSCCmd> named = MSCCmdForName(parts[4]);
      if (named.has_value())
        cmd = named.value();
    }
    message.push_back(MSCCmdValue(cmd));

    if (args)
    {
      if (MSCCmdStrings(cmd))
      {
        std::string str;
        bool terminatePrevStr = false;
        for (size_t argIndex = 0; argIndex < argCount; ++argIndex)
        {
          if (!args[argIndex].GetString(str) || str.empty())
            continue;

          if (terminatePrevStr)
            message.push_back(0);

          for (size_t strIndex = 0; strIndex < str.size(); ++strIndex)
            message.push_back(static_cast<unsigned char>(str[strIndex]));

          terminatePrevStr = true;
        }
      }
      else
      {
        int n = 0;
        for (size_t argIndex = 0; argIndex < argCount; ++argIndex)
        {
          if (args[argIndex].GetInt(n))
            message.push_back(static_cast<unsigned char>(n));
        }
      }
    }

    message.push_back(static_cast<unsigned char>(MSC::kSysExEnd));
  }
  else if (args)
  {
    message.reserve(argCount);
    for (size_t i = 0; i < argCount; ++i)
    {
      int n = 0;
      if (args[i].GetInt(n))
        message.push_back(n);
    }
  }

  delete[] args;

  if (message.empty())
    return;

  try
  {
    portIter->second.midi->sendMessage(&message);
    SetItemActivity(routeDst.dstItemStateTableId);
    LogMIDI(/*send*/ true, portIter->second.name, message);
  }
  catch (RtMidiError &error)
  {
    m_PrivateLog.AddError("RtMidiOut sendMessage error: " + error.getMessage());
  }
}

////////////////////////////////////////////////////////////////////////////////

void RouterThread::SendOTP(OTPI &otpi, const sRouteDst &routeDst, EosPacket &oscPacket)
{
  if (!oscPacket.GetData() || oscPacket.GetSize() < 1)
    return;

  if (routeDst.dst.addr.port < otp::kMinSystemNumber || routeDst.dst.addr.port > otp::kMaxSystemNumber)
    return;

  // find osc path null terminator
  QString path;
  size_t argCount = 0xffffffff;
  OSCArgument *args = nullptr;
  char *data = oscPacket.GetData();
  if (data)
  {
    for (int i = 0; i < oscPacket.GetSize(); i++)
    {
      if (data[i] == 0)
      {
        path = QString::fromUtf8(data);
        args = OSCArgument::GetArgs(&data[i], static_cast<size_t>(oscPacket.GetSize()), argCount);
        break;
      }
    }
  }

  if (!args)
    return;

  if (argCount < 3)
  {
    delete[] args;
    return;
  }

  QStringList parts = path.split(QLatin1Char('/'));
  if (parts.size() < 6)
  {
    delete[] args;
    return;
  }

  otp::Frame frame;
  frame.system = static_cast<otp::SystemNumber>(routeDst.dst.addr.port);
  frame.group = static_cast<otp::GroupNumber>(parts[2].toInt());
  frame.point = static_cast<otp::PointNumber>(parts[3].toInt());

  otp::Point point;
  point.name = QLatin1String(VER_PRODUCTNAME_STR) + QLatin1String("Point");
  point.priority = static_cast<otp::PriorityNumber>(parts[4].toInt());

  if (parts[5] == QLatin1String("pos"))
  {
    int x = 0;
    int y = 0;
    int z = 0;
    if (args[0].GetInt(x) && args[1].GetInt(y) && args[2].GetInt(z))
    {
      otp::Vector3 v;
      v.x = x;
      v.y = y;
      v.z = z;

      point.modules.pos = v;
    }
  }
  else if (parts[5] == QLatin1String("posVelAccel"))
  {
    if (parts.size() < 6)
    {
      delete[] args;
      return;
    }

    int vx = 0;
    int vy = 0;
    int vz = 0;
    int ax = 0;
    int ay = 0;
    int az = 0;
    if (args[0].GetInt(vx) && args[1].GetInt(vy) && args[2].GetInt(vz) && args[3].GetInt(ax) && args[4].GetInt(ay) && args[5].GetInt(az))
    {
      otp::VelAccel va;
      va.vel.x = vx;
      va.vel.y = vy;
      va.vel.z = vz;
      va.accel.x = ax;
      va.accel.y = ay;
      va.accel.z = az;

      point.modules.pos_vel_accel = va;
    }
  }
  else if (parts[5] == QLatin1String("rot"))
  {
    unsigned int x = 0;
    unsigned int y = 0;
    unsigned int z = 0;
    if (args[0].GetUInt(x) && args[1].GetUInt(y) && args[2].GetUInt(z))
    {
      otp::Vector3u v;
      v.x = x;
      v.y = y;
      v.z = z;

      point.modules.rot = v;
    }
  }
  else if (parts[5] == QLatin1String("rotVelAccel"))
  {
    if (parts.size() < 6)
    {
      delete[] args;
      return;
    }

    int vx = 0;
    int vy = 0;
    int vz = 0;
    unsigned int ax = 0;
    unsigned int ay = 0;
    unsigned int az = 0;
    if (args[0].GetInt(vx) && args[1].GetInt(vy) && args[2].GetInt(vz) && args[3].GetUInt(ax) && args[4].GetUInt(ay) && args[5].GetUInt(az))
    {
      otp::VelAccel va;
      va.vel.x = vx;
      va.vel.y = vy;
      va.vel.z = vz;
      va.accel.x = ax;
      va.accel.y = ay;
      va.accel.z = az;

      point.modules.rot_vel_accel = va;
    }
  }
  else if (parts[5] == QLatin1String("scale"))
  {
    int x = 0;
    int y = 0;
    int z = 0;
    if (args[0].GetInt(x) && args[1].GetInt(y) && args[2].GetInt(z))
    {
      otp::Vector3 v;
      v.x = x;
      v.y = y;
      v.z = z;

      point.modules.scale = v;
    }
  }
  else if (parts[5] == QLatin1String("frame"))
  {
    int frameSystem = 0;
    int frameGroup = 0;
    int framePoint = 0;
    if (args[0].GetInt(frameSystem) && args[1].GetInt(frameGroup) && args[2].GetInt(framePoint))
    {
      otp::Frame refFrame;
      refFrame.system = frameSystem;
      refFrame.group = frameGroup;
      refFrame.point = framePoint;

      point.modules.frame = refFrame;
    }
  }

  if (point.modules.empty())
  {
    delete[] args;
    return;
  }

  otp::PointChange result = otpi.producer.SetPoint(frame, point);

  if (result == otp::PointChange::kSystemAppeared)
    SetItemState(routeDst.dstItemStateTableId, ItemState::STATE_CONNECTED);

  QString log = QStringLiteral("OTP OUT [%1] %2/%3/%4 ").arg(frame.system).arg(frame.group).arg(frame.point).arg(point.priority) + point.modules.toString();
  m_PrivateLog.Add(EosLog::LOG_MSG_TYPE_SEND, log.toStdString());

  SetItemActivity(routeDst.dstItemStateTableId);
}

////////////////////////////////////////////////////////////////////////////////

void RouterThread::ProcessTcpConnectionQ(TCP_CLIENT_THREADS &tcpClientThreads, EosTcpServerThread &tcpServer, EosTcpServerThread::CONNECTION_Q &tcpConnectionQ, bool mute)
{
  for (EosTcpServerThread::CONNECTION_Q::const_iterator i = tcpConnectionQ.begin(); i != tcpConnectionQ.end(); i++)
  {
    const EosTcpServerThread::sConnection &tcpConnection = *i;

    // check if an existing connection has been replaced
    TCP_CLIENT_THREADS::iterator clientIter = tcpClientThreads.find(tcpConnection.addr);
    if (clientIter != tcpClientThreads.end())
    {
      EosTcpClientThread *thread = clientIter->second;
      delete thread;
      tcpClientThreads.erase(clientIter);
    }

    EosTcpClientThread *thread = new EosTcpClientThread();
    tcpClientThreads[tcpConnection.addr] = thread;
    thread->Start(tcpConnection.tcp, tcpConnection.addr, tcpServer.GetItemStateTableId(), tcpServer.GetFrameMode(), /*reconnectDelayMS*/ 0, mute);
  }
}

////////////////////////////////////////////////////////////////////////////////

bool RouterThread::ApplyTransform(OSCArgument &arg, const EosRouteDst &dst, OSCPacketWriter &packet)
{
  float f;
  if (arg.GetFloat(f))
  {
    if (dst.inMin.enabled && dst.inMax.enabled && dst.outMin.enabled && dst.outMax.enabled)
    {
      // scale

      float range = (dst.inMax.value - dst.inMin.value);
      float t = ((range > -EPSILLON && range < EPSILLON) ? 0 : ((f - dst.inMin.value) / range));
      range = (dst.outMax.value - dst.outMin.value);
      f = ((range > -EPSILLON && range < EPSILLON) ? dst.outMin.value : (dst.outMin.value + t * range));
    }
    else
    {
      // just min/max limits

      if (dst.inMin.enabled || dst.outMin.enabled)
      {
        float fMin = (dst.inMin.enabled ? (dst.outMin.enabled ? qMax(dst.inMin.value, dst.outMin.value) : dst.inMin.value) : dst.outMin.value);
        if (f < fMin)
        {
          packet.AddFloat32(fMin);
          return true;
        }
      }

      if (dst.inMax.enabled || dst.outMax.enabled)
      {
        float fMax = (dst.inMax.enabled ? (dst.outMax.enabled ? qMin(dst.inMax.value, dst.outMax.value) : dst.inMax.value) : dst.outMax.value);
        if (f > fMax)
          f = fMax;
      }
    }

    packet.AddFloat32(f);
    return true;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////

void RouterThread::MakeSendPath(ArtNet &artnet, const EosAddr &addr, Protocol protocol, const QString &srcPath, const QString &dstPath, const OSCArgument *args, size_t argsCount, QString &sendPath)
{
  if (dstPath.isEmpty() && protocol != Protocol::ksACN && protocol != Protocol::kArtNet)
  {
    sendPath = srcPath;
  }
  else
  {
    sendPath = dstPath;

    if (sendPath.contains('%'))
    {
      // possible in-line path replacements:
      // %1  => srcPath[0]
      // %2  => srcPath[1]
      // %3  => arg[0]
      // %%1 => %1
      // %A  => %A

      QStringList srcPathParts;
      bool srcPathPartsInitialized = false;

      // look for all instances of '%' follow by a number
      int digitCount = 0;
      for (int i = 0; i <= sendPath.size(); i++)
      {
        if (i < sendPath.size() && sendPath[i].isDigit())
        {
          digitCount++;
        }
        else if (digitCount > 0)
        {
          // is number preceeded by a '%'?
          int startIndex = (i - digitCount - 1);
          if (startIndex > 0 && sendPath[startIndex] == '%')
          {
            // is '%' precceed by a '%'?
            if ((startIndex - 1) > 0 && sendPath[startIndex - 1] == '%')
            {
              // %%xxx => %xxx
              sendPath.remove(startIndex, 1);
              --i;
            }
            else
            {
              // %xxx => srcPath[xxx-1]

              int srcPathIndex = sendPath.mid(startIndex + 1, digitCount).toInt();
              if (!srcPath.isEmpty())
                --srcPathIndex;

              if (!srcPathPartsInitialized)
              {
                srcPathParts = srcPath.split(OSC_ADDR_SEPARATOR, Qt::SkipEmptyParts);
                if (srcPathParts.isEmpty())
                  srcPathParts << srcPath;
                srcPathPartsInitialized = true;
              }

              QString insertStr;
              if (srcPathIndex >= 0)
              {
                if (srcPathIndex >= srcPathParts.size())
                {
                  if (protocol == Protocol::ksACN)
                  {
                    srcPathIndex -= srcPathParts.size();
                    uint8_t value = 0;

                    if (srcPathIndex >= 0)
                    {
                      QMutexLocker locker(&m_sACNRecv.mutex);
                      UNIVERSE_LIST::const_iterator universeIter = m_sACNRecv.merged.find(addr.port);
                      if (universeIter != m_sACNRecv.merged.end())
                      {
                        const Universe &universe = universeIter->second;
                        if (static_cast<size_t>(srcPathIndex) < universe.dmx.size())
                          value = universe.dmx[srcPathIndex];
                      }
                    }

                    insertStr = QString::number(static_cast<ushort>(value));
                  }
                  else if (protocol == Protocol::kArtNet)
                  {
                    srcPathIndex -= srcPathParts.size();
                    uint8_t value = 0;

                    if (srcPathIndex >= 0)
                    {
                      uint8_t universeNumber = static_cast<uint8_t>(addr.port);
                      ARTNET_RECV_UNIVERSE_LIST::const_iterator universeIter = artnet.inputs.find(universeNumber);
                      if (universeIter != artnet.inputs.end())
                      {
                        int length = 0;
                        uint8_t *data = artnet_read_dmx(universeIter->second.node, 0, &length);
                        if (data && srcPathIndex < length)
                          value = data[srcPathIndex];
                      }
                    }

                    insertStr = QString::number(static_cast<ushort>(value));
                  }
                  else if (args)
                  {
                    srcPathIndex -= srcPathParts.size();
                    if (srcPathIndex >= 0 && static_cast<size_t>(srcPathIndex) < argsCount)
                    {
                      std::string argStr;
                      if (args[srcPathIndex].GetString(argStr))
                        insertStr = QString::fromStdString(argStr);
                    }
                  }
                }
                else
                  insertStr = srcPathParts[srcPathIndex];
              }

              if (insertStr.isEmpty())
              {
                QString msg = QString("Unable to remap %1 => %2, invalid replacement index %3").arg(srcPath).arg(sendPath).arg(srcPathIndex + 1);
                m_PrivateLog.AddWarning(msg.toUtf8().constData());
                sendPath.clear();
                return;
              }

              int midIndex = (startIndex + digitCount + 1);

              if (midIndex < sendPath.size())
                sendPath = sendPath.left(startIndex) + insertStr + sendPath.mid(midIndex);
              else
                sendPath = sendPath.left(startIndex) + insertStr;

              i = (midIndex - 1);
            }
          }

          digitCount = 0;
        }
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////

void RouterThread::UpdateLog()
{
  m_Mutex.lock();
  m_Log.AddLog(m_PrivateLog);
  m_Mutex.unlock();

  m_PrivateLog.Clear();
}

////////////////////////////////////////////////////////////////////////////////

RouterThread::MuteAll RouterThread::GetMuteAll()
{
  MuteAll muteAll;

  m_Mutex.lock();
  muteAll.incoming = m_ItemStateTable.GetMuteAllIncoming();
  muteAll.outgoing = m_ItemStateTable.GetMuteAllOutgoing();
  m_Mutex.unlock();

  return muteAll;
}

////////////////////////////////////////////////////////////////////////////////

bool RouterThread::IsRouteMuted(ItemStateTable::ID id)
{
  m_Mutex.lock();
  const ItemState *itemState = m_ItemStateTable.GetItemState(id);
  bool muted = (itemState && itemState->mute);
  m_Mutex.unlock();

  return muted;
}

////////////////////////////////////////////////////////////////////////////////

void RouterThread::SetItemState(ItemStateTable::ID id, ItemState::EnumState state)
{
  m_Mutex.lock();
  const ItemState *itemState = m_ItemStateTable.GetItemState(id);
  if (itemState && itemState->state != state)
  {
    ItemState newItemState(*itemState);
    newItemState.state = state;
    m_ItemStateTable.Update(id, newItemState);
  }
  m_Mutex.unlock();
}

////////////////////////////////////////////////////////////////////////////////

void RouterThread::SetItemState(const ROUTES_BY_PORT &routesByPort, Protocol dstProtocol, ItemState::EnumState state)
{
  for (ROUTES_BY_PORT::const_iterator portIter = routesByPort.begin(); portIter != routesByPort.end(); ++portIter)
    SetItemState(portIter->second, dstProtocol, state);
}

////////////////////////////////////////////////////////////////////////////////

void RouterThread::SetItemState(const ROUTES_BY_IP &routesByIp, Protocol dstProtocol, ItemState::EnumState state)
{
  for (ROUTES_BY_IP::const_iterator ipIter = routesByIp.begin(); ipIter != routesByIp.end(); ++ipIter)
  {
    SetItemState(ipIter->second.routesByPath, dstProtocol, state);
    SetItemState(ipIter->second.routesByWildcardPath, dstProtocol, state);
  }
}

////////////////////////////////////////////////////////////////////////////////

void RouterThread::SetItemState(const ROUTES_BY_PATH &routesByPath, Protocol dstProtocol, ItemState::EnumState state)
{
  for (ROUTES_BY_PATH::const_iterator pathIter = routesByPath.begin(); pathIter != routesByPath.end(); ++pathIter)
  {
    const ROUTE_DESTINATIONS &destinations = pathIter->second;
    for (ROUTE_DESTINATIONS::const_iterator dstIter = destinations.begin(); dstIter != destinations.end(); ++dstIter)
    {
      if (dstProtocol == Protocol::kInvalid)
        SetItemState(dstIter->srcItemStateTableId, state);
      else if (dstIter->dst.protocol == dstProtocol)
        SetItemState(dstIter->dstItemStateTableId, state);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////

void RouterThread::SetItemActivity(ItemStateTable::ID id)
{
  m_Mutex.lock();
  const ItemState *itemState = m_ItemStateTable.GetItemState(id);
  if (itemState && !itemState->activity)
  {
    ItemState newItemState(*itemState);
    newItemState.activity = true;
    m_ItemStateTable.Update(id, newItemState);
  }
  m_Mutex.unlock();
}

////////////////////////////////////////////////////////////////////////////////

void RouterThread::run()
{
  MainLoop();
  UpdateLog();
}

////////////////////////////////////////////////////////////////////////////////

void RouterThread::MainLoop()
{
  m_PrivateLog.AddInfo("router thread started");
  UpdateLog();

  m_ScriptEngine = new ScriptEngine();
  m_PSNEncoder = new psn::psn_encoder(VER_PRODUCTNAME_STR);
  m_PSNEncoderTimer.invalidate();

  UDP_IN_THREADS udpInThreads;
  UDP_OUT_THREADS udpOutThreads;
  TCP_CLIENT_THREADS tcpClientThreads;
  TCP_SERVER_THREADS tcpServerThreads;
  ROUTES_BY_PORT routesByPort;
  ROUTES_BY_PORT routesBysACNUniverse;
  ROUTES_BY_PORT routesByArtNetUniverse;
  ROUTES_BY_PORT routesByMIDI;
  ROUTES_BY_PORT routesByOTP;
  DESTINATIONS_LIST routingDestinationList;
  EosUdpInThread::RECV_PORT_Q dmxRecvQ;
  EosUdpInThread::RECV_Q recvQ;
  EosTcpServerThread::CONNECTION_Q tcpConnectionQ;
  EosLog::LOG_Q tempLogQ;

  OSCParser oscBundleParser;
  oscBundleParser.SetRoot(new OSCBundleMethod());
  PacketLogger packetLogger(EosLog::LOG_MSG_TYPE_RECV, m_PrivateLog);

  BuildRoutes(routesByPort, routesBysACNUniverse, routesByArtNetUniverse, routesByMIDI, routesByOTP, udpInThreads, udpOutThreads, tcpClientThreads, tcpServerThreads);

  sACN sacn;
  BuildsACN(routesByPort, routesBysACNUniverse, routesByArtNetUniverse, routesByMIDI, routesByOTP, sacn);

  ArtNet artnet;
  BuildArtNet(routesByPort, routesBysACNUniverse, routesByArtNetUniverse, routesByMIDI, routesByOTP, artnet);

  MIDI midi;
  BuildMIDI(routesByMIDI, midi);

  OTPI otpi;
  BuildOTP(routesByPort, routesBysACNUniverse, routesByArtNetUniverse, routesByMIDI, routesByOTP, otpi);

  if (!m_Settings.script.isEmpty())
  {
    QString error = m_ScriptEngine->evaluate(m_Settings.script, &m_PrivateLog);
    if (!error.isEmpty())
      OSCParserClient_Log(error.toStdString());
  }

  while (m_Run)
  {
    MuteAll muteAll = GetMuteAll();

    // sACN input
    RecvsACN(sacn, dmxRecvQ);

    if (!muteAll.incoming)
    {
      EosAddr dmxAddr;
      for (size_t i = 0; i < dmxRecvQ.size(); ++i)
      {
        EosUdpInThread::sRecvPortPacket &dmxPacket = dmxRecvQ[i];
        dmxAddr.fromUInt(dmxPacket.p.ip);
        dmxAddr.port = dmxPacket.port;
        ProcessRecvPacket(muteAll.outgoing, sacn, artnet, midi, otpi, routesBysACNUniverse, routingDestinationList, udpOutThreads, tcpServerThreads, tcpClientThreads, dmxAddr, Protocol::ksACN,
                          dmxPacket.p);
      }
    }

    // ArtNet input
    RecvArtNet(artnet, dmxRecvQ);

    if (!muteAll.incoming)
    {
      EosAddr dmxAddr;
      for (size_t i = 0; i < dmxRecvQ.size(); ++i)
      {
        EosUdpInThread::sRecvPortPacket &dmxPacket = dmxRecvQ[i];
        dmxAddr.fromUInt(dmxPacket.p.ip);
        dmxAddr.port = dmxPacket.port;
        ProcessRecvPacket(muteAll.outgoing, sacn, artnet, midi, otpi, routesByArtNetUniverse, routingDestinationList, udpOutThreads, tcpServerThreads, tcpClientThreads, dmxAddr, Protocol::kArtNet,
                          dmxPacket.p);
      }
    }

    // MIDI input
    RecvMIDI(oscBundleParser, packetLogger, muteAll.incoming, muteAll.outgoing, sacn, artnet, midi, otpi, routesByMIDI, routingDestinationList, udpOutThreads, tcpServerThreads, tcpClientThreads);

    // OTP input
    RecvOTP(oscBundleParser, packetLogger, muteAll.incoming, muteAll.outgoing, sacn, artnet, midi, otpi, routesByOTP, routingDestinationList, udpOutThreads, tcpServerThreads, tcpClientThreads);

    // UDP input
    for (UDP_IN_THREADS::iterator i = udpInThreads.begin(); i != udpInThreads.end();)
    {
      EosUdpInThread *thread = i->second;
      bool running = thread->isRunning();
      thread->Mute(muteAll.incoming);
      thread->Flush(tempLogQ, recvQ);
      m_PrivateLog.AddQ(tempLogQ);
      tempLogQ.clear();

      SetItemState(thread->GetItemStateTableId(), thread->GetState());
      ProcessRecvQ(muteAll.outgoing, sacn, artnet, midi, otpi, oscBundleParser, routesByPort, routingDestinationList, udpOutThreads, tcpServerThreads, tcpClientThreads, thread->GetAddr(), recvQ);

      if (!running)
      {
        delete thread;
        udpInThreads.erase(i++);
      }
      else
        i++;
    }

    // TCP servers
    for (TCP_SERVER_THREADS::iterator i = tcpServerThreads.begin(); i != tcpServerThreads.end();)
    {
      EosTcpServerThread *thread = i->second;
      bool running = thread->isRunning();
      thread->Flush(tempLogQ, tcpConnectionQ);
      m_PrivateLog.AddQ(tempLogQ);
      tempLogQ.clear();

      SetItemState(thread->GetItemStateTableId(), thread->GetState());

      if (!tcpConnectionQ.empty())
      {
        SetItemActivity(thread->GetItemStateTableId());
        ProcessTcpConnectionQ(tcpClientThreads, *thread, tcpConnectionQ, muteAll.incoming);
      }

      if (!running)
      {
        delete thread;
        tcpServerThreads.erase(i++);
      }
      else
        i++;
    }

    // TCP clients
    for (TCP_CLIENT_THREADS::iterator i = tcpClientThreads.begin(); i != tcpClientThreads.end();)
    {
      EosTcpClientThread *thread = i->second;
      bool running = thread->isRunning();
      thread->Mute(muteAll.incoming);
      thread->Flush(tempLogQ, recvQ);
      m_PrivateLog.AddQ(tempLogQ);
      tempLogQ.clear();

      SetItemState(thread->GetItemStateTableId(), thread->GetState());
      ProcessRecvQ(muteAll.outgoing, sacn, artnet, midi, otpi, oscBundleParser, routesByPort, routingDestinationList, udpOutThreads, tcpServerThreads, tcpClientThreads, thread->GetAddr(), recvQ);

      if (!running)
      {
        delete thread;
        tcpClientThreads.erase(i++);
      }
      else
        i++;
    }

    // UDP output
    for (UDP_OUT_THREADS::iterator i = udpOutThreads.begin(); i != udpOutThreads.end();)
    {
      EosUdpOutThread *thread = i->second;
      bool running = thread->isRunning();
      thread->Flush(tempLogQ);
      m_PrivateLog.AddQ(tempLogQ);
      tempLogQ.clear();

      SetItemState(thread->GetItemStateTableId(), thread->GetState());

      if (!running)
      {
        delete thread;
        udpOutThreads.erase(i++);
      }
      else
        i++;
    }

    // sACN output
    if (sacn.server)
    {
      if (sacn.sendTimer.isValid())
      {
        if (sacn.sendTimer.elapsed() >= 22)
        {
          if (!muteAll.outgoing)
            sacn.server->Tick(nullptr, 0);

          sacn.sendTimer.start();
        }
      }
      else
        sacn.sendTimer.start();
    }

    // ArtNet output
    FlushArtNet(artnet);

    // OTP output
    otpi.producer.Tick();

    UpdateLog();

    msleep(1);
  }

  // shutdown
  for (TCP_SERVER_THREADS::const_iterator i = tcpServerThreads.begin(); i != tcpServerThreads.end(); i++)
  {
    EosTcpServerThread *thread = i->second;
    thread->Stop();
    thread->Flush(tempLogQ, tcpConnectionQ);
    for (EosTcpServerThread::CONNECTION_Q::const_iterator j = tcpConnectionQ.begin(); j != tcpConnectionQ.end(); j++)
      delete j->tcp;
    m_PrivateLog.AddQ(tempLogQ);
    tempLogQ.clear();
    delete thread;
  }

  for (TCP_CLIENT_THREADS::const_iterator i = tcpClientThreads.begin(); i != tcpClientThreads.end(); i++)
  {
    EosTcpClientThread *thread = i->second;
    thread->Stop();
    thread->Flush(tempLogQ, recvQ);
    m_PrivateLog.AddQ(tempLogQ);
    tempLogQ.clear();
    delete thread;
  }

  for (UDP_OUT_THREADS::const_iterator i = udpOutThreads.begin(); i != udpOutThreads.end(); i++)
  {
    EosUdpOutThread *thread = i->second;
    thread->Stop();
    thread->Flush(tempLogQ);
    m_PrivateLog.AddQ(tempLogQ);
    tempLogQ.clear();
    delete thread;
  }

  for (UDP_IN_THREADS::const_iterator i = udpInThreads.begin(); i != udpInThreads.end(); i++)
  {
    EosUdpInThread *thread = i->second;
    thread->Stop();
    thread->Flush(tempLogQ, recvQ);
    m_PrivateLog.AddQ(tempLogQ);
    tempLogQ.clear();
    delete thread;
  }

  m_ItemStateTable.Deactivate();

  DestroyArtNet(artnet);
  DestroysACN(sacn);

  delete m_PSNEncoder;
  m_PSNEncoder = nullptr;

  delete m_ScriptEngine;
  m_ScriptEngine = nullptr;

  m_PrivateLog.AddInfo("router thread ended");
  UpdateLog();
}

////////////////////////////////////////////////////////////////////////////////

void RouterThread::FlushArtNet(ArtNet &artnet)
{
  for (ARTNET_SEND_UNIVERSE_LIST::iterator outputIter = artnet.output.begin(); outputIter != artnet.output.end(); ++outputIter)
  {
    uint8_t universeNumber = outputIter->first;
    ArtNetSendUniverse &universe = outputIter->second;

    qint64 timeout = universe.dirty ? 22 : 1000;
    if (universe.timer.isValid() && universe.timer.elapsed() < timeout)
      continue;

    artnet_raw_send_dmx(artnet.server, universeNumber, static_cast<int16_t>(universe.dmx.size()), universe.dmx.data());
    universe.timer.start();
    universe.dirty = false;
  }
}

////////////////////////////////////////////////////////////////////////////////

void RouterThread::RecvsACN(sACN &sacn, EosUdpInThread::RECV_PORT_Q &recvQ)
{
  recvQ.clear();

  if (sacn.client)
  {
    if (sacn.recvTimer.isValid())
    {
      if (sacn.recvTimer.elapsed() >= 200)
      {
        sacn.client->FindExpiredSources();
        sacn.recvTimer.start();
      }
    }
    else
      sacn.recvTimer.start();
  }

  QMutexLocker locker(&m_sACNRecv.mutex);

  if (m_sACNRecv.dirtyUniverses.empty())
    return;

  UNIVERSE_NUMBER_SET activeUniverses;

  for (SACN_SOURCE_LIST::const_iterator sourceIter = m_sACNRecv.sources.begin(); sourceIter != m_sACNRecv.sources.end(); ++sourceIter)
  {
    const sACNSource &source = sourceIter->second;

    for (UNIVERSE_NUMBER_SET::const_iterator dirtyIter = m_sACNRecv.dirtyUniverses.begin(); dirtyIter != m_sACNRecv.dirtyUniverses.end(); ++dirtyIter)
    {
      uint16_t universeNumber = *dirtyIter;
      UNIVERSE_LIST::const_iterator universeIter = source.universes.find(universeNumber);
      if (universeIter == source.universes.end())
        continue;

      const Universe &universe = universeIter->second;
      Universe &merged = m_sACNRecv.merged[universeNumber];

      if (activeUniverses.insert(universeNumber).second)
      {
        // fist instance of this universe
        merged.ip = universe.ip;
        merged.dmx = universe.dmx;
        merged.hasPerChannelPriority = universe.hasPerChannelPriority;
        if (merged.hasPerChannelPriority)
          merged.channelPriority = universe.channelPriority;
        else
          merged.priority = universe.priority;
      }
      else if (universe.hasPerChannelPriority)
      {
        if (merged.hasPerChannelPriority)
        {
          // merge per channel priority universe with existing per channel priority universe
          for (int channel = 0; channel < UNIVERSE_SIZE; ++channel)
          {
            if (universe.channelPriority[channel] > merged.channelPriority[channel])
            {
              merged.dmx[channel] = universe.dmx[channel];
              merged.channelPriority[channel] = universe.channelPriority[channel];
              merged.ip = universe.ip;
            }
          }
        }
        else
        {
          // merge per channel priority unviverse with basic priority universe
          merged.hasPerChannelPriority = true;
          for (int channel = 0; channel < UNIVERSE_SIZE; ++channel)
          {
            if (universe.channelPriority[channel] > merged.priority)
            {
              merged.dmx[channel] = universe.dmx[channel];
              merged.channelPriority[channel] = universe.channelPriority[channel];
              merged.ip = universe.ip;
            }
            else
              merged.channelPriority[channel] = merged.priority;
          }
        }
      }
      else
      {
        if (merged.hasPerChannelPriority)
        {
          // merge basic priority universe with existing per channel priority universe
          for (int channel = 0; channel < UNIVERSE_SIZE; ++channel)
          {
            if (universe.priority > merged.channelPriority[channel])
            {
              merged.dmx[channel] = universe.dmx[channel];
              merged.channelPriority[channel] = universe.priority;
              merged.ip = universe.ip;
            }
          }
        }
        else if (universe.priority > merged.priority)
        {
          // merge basic priority universe with existing basic priority universe
          merged.dmx = universe.dmx;
          merged.priority = universe.priority;
          merged.ip = universe.ip;
        }
      }
    }
  }

  // remove inactive universes
  if (activeUniverses.size() < m_sACNRecv.dirtyUniverses.size())
  {
    for (UNIVERSE_NUMBER_SET::const_iterator dirtyIter = m_sACNRecv.dirtyUniverses.begin(); dirtyIter != m_sACNRecv.dirtyUniverses.end(); ++dirtyIter)
    {
      uint16_t universeNumber = *dirtyIter;
      if (activeUniverses.find(universeNumber) == activeUniverses.end())
        m_sACNRecv.merged.erase(universeNumber);
    }
  }

  // queue OSC style packets
  for (UNIVERSE_NUMBER_SET::const_iterator activeIter = activeUniverses.begin(); activeIter != activeUniverses.end(); ++activeIter)
  {
    uint16_t universeNumber = *activeIter;
    UNIVERSE_LIST::iterator universeIter = m_sACNRecv.merged.find(universeNumber);
    if (universeIter == m_sACNRecv.merged.end())
      continue;

    Universe &universe = universeIter->second;
    if (m_Settings.levelChangesOnly)
    {
      if (universe.hasPrevDMX && universe.dmx == universe.prevDMX)
        continue;  // no levels changed

      universe.prevDMX = universe.dmx;
      universe.hasPrevDMX = true;
    }

    recvQ.push_back(EosUdpInThread::sRecvPortPacket(universeNumber, nullptr, 0, universe.ip));
  }

  m_sACNRecv.dirtyUniverses.clear();

  m_PrivateLog.AddLog(m_sACNRecv.log);
  m_sACNRecv.log.Clear();
}

////////////////////////////////////////////////////////////////////////////////

void RouterThread::RecvArtNet(ArtNet &artnet, EosUdpInThread::RECV_PORT_Q &recvQ)
{
  recvQ.clear();

  for (ARTNET_RECV_UNIVERSE_LIST::const_iterator inputIter = artnet.inputs.begin(); inputIter != artnet.inputs.end(); ++inputIter)
    artnet_read(inputIter->second.node, 0);

  for (ARTNET_DIRTY_LIST::const_iterator dirtyIter = artnet.dirty.begin(); dirtyIter != artnet.dirty.end(); ++dirtyIter)
  {
    artnet_node node = *dirtyIter;

    int universeNumber = artnet_get_universe_addr(node, 0, ARTNET_OUTPUT_PORT);
    if (universeNumber < 0)
      continue;

    unsigned int ip = 0;
    ARTNET_NODE_IP_LIST::const_iterator ipIter = artnet.inputIPs.find(*dirtyIter);
    if (ipIter != artnet.inputIPs.end())
      ip = ipIter->second;

    if (m_Settings.levelChangesOnly)
    {
      ARTNET_RECV_UNIVERSE_LIST::iterator recvIter = artnet.inputs.find(universeNumber);
      if (recvIter == artnet.inputs.end())
        continue;  // no such universe to compare

      ArtNetRecvUniverse &recv = recvIter->second;

      int length = 0;
      uint8_t *data = artnet_read_dmx(recv.node, 0, &length);
      if (!data || length < 1)
        continue;  // no levels to compare

      size_t dmxLength = std::min(recv.prevDMX.size(), static_cast<size_t>(length));
      if (recv.hasPrevDMX && memcmp(recv.prevDMX.data(), data, dmxLength) == 0)
        continue;  // no levels changed

      memcpy(recv.prevDMX.data(), data, dmxLength);
      recv.hasPrevDMX = true;
    }

    recvQ.push_back(EosUdpInThread::sRecvPortPacket(static_cast<uint16_t>(universeNumber), nullptr, 0, ip));
  }

  artnet.dirty.clear();
}

void RouterThread::RecvMIDI(OSCParser &oscParser, PacketLogger &packetLogger, bool muteAllIncoming, bool muteAllOutgoing, sACN &sacn, ArtNet &artnet, MIDI &midi, OTPI &otpi,
                            ROUTES_BY_PORT &routesByPort, DESTINATIONS_LIST &routingDestinationList, UDP_OUT_THREADS &udpOutThreads, TCP_SERVER_THREADS &tcpServerThreads,
                            TCP_CLIENT_THREADS &tcpClientThreads)
{
  if (midi.inputs.empty())
    return;

  EosAddr addr;
  std::vector<unsigned char> message;
  for (MIDI_INPUT_LIST::const_iterator portIter = midi.inputs.begin(); portIter != midi.inputs.end(); ++portIter)
  {
    try
    {
      portIter->second.midi->getMessage(&message);
    }
    catch (RtMidiError &error)
    {
      m_PrivateLog.AddError("RtMidiOut getMessage error: " + error.getMessage());
      continue;
    }

    if (message.empty() || muteAllIncoming)
      continue;

    LogMIDI(/*send*/ false, portIter->second.name, message);

    packetLogger.SetPrefix(QStringLiteral("MIDI IN  [%1] ").arg(portIter->second.name).toUtf8().constData());

    // raw MIDI
    {
      OSCPacketWriter osc("/midi");
      for (size_t i = 0; i < message.size(); ++i)
        osc.AddInt32(message[i]);

      size_t oscPacketSize = 0;
      char *oscPacket = osc.Create(oscPacketSize);
      if (oscPacket)
      {
        addr.port = static_cast<unsigned short>(portIter->first);
        packetLogger.PrintPacket(oscParser, oscPacket, oscPacketSize);
        EosUdpInThread::sRecvPacket packet(oscPacket, static_cast<int>(oscPacketSize), /*Ip*/ 0);
        delete[] oscPacket;

        ProcessRecvPacket(muteAllOutgoing, sacn, artnet, midi, otpi, routesByPort, routingDestinationList, udpOutThreads, tcpServerThreads, tcpClientThreads, addr, Protocol::kOSC, packet);
      }
    }

    // MIDI Show Control
    if (message.size() >= 8 && static_cast<MSC>(message[0]) == MSC::kSysEx && static_cast<MSC>(message[1]) == MSC::kSysExStart && static_cast<MSC>(message[3]) == MSC::kMSC)
    {
      MSCCmd mscCmd = ValueMSCCmd(message[5]);
      OSCPacketWriter osc("/msc/" + std::to_string(message[2]) + "/" + std::to_string(message[4]) + "/" + MSCCmdName(mscCmd).toStdString());

      if (MSCCmdStrings(mscCmd))
      {
        std::string str;
        for (size_t i = 6; i < message.size(); ++i)
        {
          if (message[i] == 0)
          {
            if (!str.empty())
              osc.AddString(str);
            str.clear();
            continue;
          }

          if (message[i] == static_cast<unsigned char>(MSC::kSysExEnd))
            break;

          str += static_cast<char>(message[i]);
        }

        if (!str.empty())
          osc.AddString(str);
      }
      else
      {
        for (size_t i = 6; i < message.size(); ++i)
        {
          if (message[i] == static_cast<unsigned char>(MSC::kSysExEnd))
            break;

          osc.AddInt32(static_cast<int32_t>(message[i]));
        }
      }

      size_t oscPacketSize = 0;
      char *oscPacket = osc.Create(oscPacketSize);
      if (oscPacket)
      {
        addr.port = static_cast<unsigned short>(portIter->first);
        packetLogger.PrintPacket(oscParser, oscPacket, oscPacketSize);
        EosUdpInThread::sRecvPacket packet(oscPacket, static_cast<int>(oscPacketSize), /*Ip*/ 0);
        delete[] oscPacket;

        ProcessRecvPacket(muteAllOutgoing, sacn, artnet, midi, otpi, routesByPort, routingDestinationList, udpOutThreads, tcpServerThreads, tcpClientThreads, addr, Protocol::kOSC, packet);
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////

void RouterThread::RecvOTP(OSCParser &oscParser, PacketLogger &packetLogger, bool muteAllIncoming, bool muteAllOutgoing, sACN &sacn, ArtNet &artnet, MIDI &midi, OTPI &otpi,
                           ROUTES_BY_PORT &routesByPort, DESTINATIONS_LIST &routingDestinationList, UDP_OUT_THREADS &udpOutThreads, TCP_SERVER_THREADS &tcpServerThreads,
                           TCP_CLIENT_THREADS &tcpClientThreads)
{
  otpi.consumer.Tick();

  if (m_OTPRecv.systems.empty())
    return;

  if (muteAllIncoming)
  {
    m_OTPRecv.systems.clear();
    return;
  }

  EosAddr addr;
  packetLogger.SetPrefix("OTP IN  ");

  for (otp::SystemList::const_iterator sysIter = m_OTPRecv.systems.begin(); sysIter != m_OTPRecv.systems.end(); ++sysIter)
  {
    otp::SystemNumber systemNumber = sysIter->first;
    const otp::GroupList &groups = sysIter->second.groups;

    for (otp::GroupList::const_iterator groupIter = groups.begin(); groupIter != groups.end(); ++groupIter)
    {
      otp::GroupNumber groupNumber = groupIter->first;
      const otp::PointList &points = groupIter->second.points;

      std::string path = "/otp/" + std::to_string(groupNumber) + "/";
      for (otp::PointList::const_iterator pointIter = points.begin(); pointIter != points.end(); ++pointIter)
      {
        otp::PointNumber pointNumber = pointIter->first;
        const otp::Point &point = pointIter->second;

        if (point.modules.pos.has_value())
        {
          OSCPacketWriter osc(path + std::to_string(pointNumber) + "/" + std::to_string(point.priority) + "/pos");
          osc.AddInt32(point.modules.pos->x);
          osc.AddInt32(point.modules.pos->y);
          osc.AddInt32(point.modules.pos->z);

          size_t oscPacketSize = 0;
          char *oscPacket = osc.Create(oscPacketSize);
          if (oscPacket)
          {
            addr.port = systemNumber;
            packetLogger.PrintPacket(oscParser, oscPacket, oscPacketSize);
            EosUdpInThread::sRecvPacket packet(oscPacket, static_cast<int>(oscPacketSize), /*Ip*/ 0);
            delete[] oscPacket;

            ProcessRecvPacket(muteAllOutgoing, sacn, artnet, midi, otpi, routesByPort, routingDestinationList, udpOutThreads, tcpServerThreads, tcpClientThreads, addr, Protocol::kOSC, packet);
          }
        }

        if (point.modules.pos_vel_accel.has_value())
        {
          OSCPacketWriter osc(path + std::to_string(pointNumber) + "/" + std::to_string(point.priority) + "/posVelAccel");
          osc.AddInt32(point.modules.pos_vel_accel->vel.x);
          osc.AddInt32(point.modules.pos_vel_accel->vel.y);
          osc.AddInt32(point.modules.pos_vel_accel->vel.z);
          osc.AddInt32(point.modules.pos_vel_accel->accel.x);
          osc.AddInt32(point.modules.pos_vel_accel->accel.y);
          osc.AddInt32(point.modules.pos_vel_accel->accel.z);

          size_t oscPacketSize = 0;
          char *oscPacket = osc.Create(oscPacketSize);
          if (oscPacket)
          {
            addr.port = systemNumber;
            packetLogger.PrintPacket(oscParser, oscPacket, oscPacketSize);
            EosUdpInThread::sRecvPacket packet(oscPacket, static_cast<int>(oscPacketSize), /*Ip*/ 0);
            delete[] oscPacket;

            ProcessRecvPacket(muteAllOutgoing, sacn, artnet, midi, otpi, routesByPort, routingDestinationList, udpOutThreads, tcpServerThreads, tcpClientThreads, addr, Protocol::kOSC, packet);
          }
        }

        if (point.modules.rot.has_value())
        {
          OSCPacketWriter osc(path + std::to_string(pointNumber) + "/" + std::to_string(point.priority) + "/rot");
          osc.AddInt32(point.modules.rot->x);
          osc.AddInt32(point.modules.rot->y);
          osc.AddInt32(point.modules.rot->z);

          size_t oscPacketSize = 0;
          char *oscPacket = osc.Create(oscPacketSize);
          if (oscPacket)
          {
            addr.port = systemNumber;
            packetLogger.PrintPacket(oscParser, oscPacket, oscPacketSize);
            EosUdpInThread::sRecvPacket packet(oscPacket, static_cast<int>(oscPacketSize), /*Ip*/ 0);
            delete[] oscPacket;

            ProcessRecvPacket(muteAllOutgoing, sacn, artnet, midi, otpi, routesByPort, routingDestinationList, udpOutThreads, tcpServerThreads, tcpClientThreads, addr, Protocol::kOSC, packet);
          }
        }

        if (point.modules.rot_vel_accel.has_value())
        {
          OSCPacketWriter osc(path + std::to_string(pointNumber) + "/" + std::to_string(point.priority) + "/rotVelAccel");
          osc.AddInt32(point.modules.rot_vel_accel->vel.x);
          osc.AddInt32(point.modules.rot_vel_accel->vel.y);
          osc.AddInt32(point.modules.rot_vel_accel->vel.z);
          osc.AddInt32(point.modules.rot_vel_accel->accel.x);
          osc.AddInt32(point.modules.rot_vel_accel->accel.y);
          osc.AddInt32(point.modules.rot_vel_accel->accel.z);

          size_t oscPacketSize = 0;
          char *oscPacket = osc.Create(oscPacketSize);
          if (oscPacket)
          {
            addr.port = systemNumber;
            packetLogger.PrintPacket(oscParser, oscPacket, oscPacketSize);
            EosUdpInThread::sRecvPacket packet(oscPacket, static_cast<int>(oscPacketSize), /*Ip*/ 0);
            delete[] oscPacket;

            ProcessRecvPacket(muteAllOutgoing, sacn, artnet, midi, otpi, routesByPort, routingDestinationList, udpOutThreads, tcpServerThreads, tcpClientThreads, addr, Protocol::kOSC, packet);
          }
        }

        if (point.modules.scale.has_value())
        {
          OSCPacketWriter osc(path + std::to_string(pointNumber) + "/" + std::to_string(point.priority) + "/scale");
          osc.AddInt32(point.modules.scale->x);
          osc.AddInt32(point.modules.scale->y);
          osc.AddInt32(point.modules.scale->z);

          size_t oscPacketSize = 0;
          char *oscPacket = osc.Create(oscPacketSize);
          if (oscPacket)
          {
            addr.port = systemNumber;
            packetLogger.PrintPacket(oscParser, oscPacket, oscPacketSize);
            EosUdpInThread::sRecvPacket packet(oscPacket, static_cast<int>(oscPacketSize), /*Ip*/ 0);
            delete[] oscPacket;

            ProcessRecvPacket(muteAllOutgoing, sacn, artnet, midi, otpi, routesByPort, routingDestinationList, udpOutThreads, tcpServerThreads, tcpClientThreads, addr, Protocol::kOSC, packet);
          }
        }

        if (point.modules.frame.has_value())
        {
          OSCPacketWriter osc(path + std::to_string(pointNumber) + "/" + std::to_string(point.priority) + "/frame");
          osc.AddInt32(point.modules.frame->system);
          osc.AddInt32(point.modules.frame->group);
          osc.AddUInt32(point.modules.frame->point);

          size_t oscPacketSize = 0;
          char *oscPacket = osc.Create(oscPacketSize);
          if (oscPacket)
          {
            addr.port = systemNumber;
            packetLogger.PrintPacket(oscParser, oscPacket, oscPacketSize);
            EosUdpInThread::sRecvPacket packet(oscPacket, static_cast<int>(oscPacketSize), /*Ip*/ 0);
            delete[] oscPacket;

            ProcessRecvPacket(muteAllOutgoing, sacn, artnet, midi, otpi, routesByPort, routingDestinationList, udpOutThreads, tcpServerThreads, tcpClientThreads, addr, Protocol::kOSC, packet);
          }
        }
      }
    }
  }

  m_OTPRecv.systems.clear();
}

////////////////////////////////////////////////////////////////////////////////

void RouterThread::OSCParserClient_Log(const std::string &message)
{
  m_PrivateLog.AddWarning(message);
}

////////////////////////////////////////////////////////////////////////////////

void RouterThread::OSCParserClient_Send(const char * /*buf*/, size_t /*size*/) {}

////////////////////////////////////////////////////////////////////////////////

void RouterThread::SourceDisappeared(const CID &source, uint2 universe)
{
  QMutexLocker locker(&m_sACNRecv.mutex);

  SACN_SOURCE_LIST::iterator sourceIter = m_sACNRecv.sources.find(source);
  if (sourceIter == m_sACNRecv.sources.end())
    return;

  sACNSource &recvSource = sourceIter->second;
  UNIVERSE_LIST::iterator universeIter = recvSource.universes.find(universe);
  if (universeIter == recvSource.universes.end())
    return;

  char str[CID::CIDSTRINGBYTES];
  CID::CIDIntoString(source, str);
  m_sACNRecv.log.AddInfo(QStringLiteral("sACN universe %1 source disappeared: %2 {%3}").arg(universe).arg(recvSource.name).arg(str).toUtf8().constData());

  recvSource.universes.erase(universeIter);
  m_sACNRecv.dirtyUniverses.insert(universe);

  if (recvSource.universes.empty())
    m_sACNRecv.sources.erase(sourceIter);
}

////////////////////////////////////////////////////////////////////////////////

void RouterThread::SourcePCPExpired(const CID &source, uint2 universe)
{
  QMutexLocker locker(&m_sACNRecv.mutex);

  SACN_SOURCE_LIST::iterator sourceIter = m_sACNRecv.sources.find(source);
  if (sourceIter == m_sACNRecv.sources.end())
    return;

  sACNSource &recvSource = sourceIter->second;
  UNIVERSE_LIST::iterator universeIter = recvSource.universes.find(universe);
  if (universeIter == recvSource.universes.end())
    return;

  Universe &recvUniverse = universeIter->second;
  if (!recvUniverse.hasPerChannelPriority)
    return;

  char str[CID::CIDSTRINGBYTES];
  CID::CIDIntoString(source, str);
  m_sACNRecv.log.AddInfo(QStringLiteral("sACN universe %1 per channel priority source expired: %2 {%3}").arg(universe).arg(recvSource.name).arg(str).toUtf8().constData());

  recvUniverse.hasPerChannelPriority = false;
  m_sACNRecv.dirtyUniverses.insert(universe);
}

////////////////////////////////////////////////////////////////////////////////

void RouterThread::UniverseData(const CID &source, const char *source_name, const CIPAddr &source_ip, uint2 universe, uint2 /*reserved*/, uint1 /*sequence*/, uint1 /*options*/, uint1 priority,
                                uint1 start_code, uint2 slot_count, uint1 *pdata)
{
  QMutexLocker locker(&m_sACNRecv.mutex);

  unsigned int ip = source_ip.GetV4Address();

  sACNSource &recvSource = m_sACNRecv.sources[source];
  if (source_name)
    recvSource.name = QString::fromUtf8(source_name);

  std::pair<UNIVERSE_LIST::iterator, bool> result = recvSource.universes.insert(std::make_pair(universe, Universe()));
  Universe &recvUniverse = result.first->second;
  if (start_code == STARTCODE_DMX)
  {
    if (result.second)
    {
      char cidStr[CID::CIDSTRINGBYTES];
      CID::CIDIntoString(source, cidStr);

      char ipStr[CIPAddr::ADDRSTRINGBYTES];
      CIPAddr::AddrIntoString(source_ip, ipStr, /*showport*/ false, /*showint*/ false);

      recvUniverse.ips.insert(ip);

      m_sACNRecv.log.AddInfo(
          QStringLiteral("sACN universe %1 source appeared: %2 {%3}, priority: %4, ip: %5").arg(universe).arg(recvSource.name).arg(cidStr).arg(priority).arg(ipStr).toUtf8().constData());
    }
    else
    {
      if (recvUniverse.priority != priority)
      {
        char str[CID::CIDSTRINGBYTES];
        CID::CIDIntoString(source, str);
        m_sACNRecv.log.AddInfo(QStringLiteral("sACN universe %1 source priority changed: %2 {%3}, priority: %4 -> %5")
                                   .arg(universe)
                                   .arg(recvSource.name)
                                   .arg(str)
                                   .arg(recvUniverse.priority)
                                   .arg(priority)
                                   .toUtf8()
                                   .constData());
      }

      if (recvUniverse.ips.insert(ip).second)
      {
        char cidStr[CID::CIDSTRINGBYTES];
        CID::CIDIntoString(source, cidStr);

        char ipStr[CIPAddr::ADDRSTRINGBYTES];
        CIPAddr::AddrIntoString(source_ip, ipStr, /*showport*/ false, /*showint*/ false);

        m_sACNRecv.log.AddInfo(
            QStringLiteral("sACN universe %1 source appeared: %2 {%3}, priority: %4, ip: %5").arg(universe).arg(recvSource.name).arg(cidStr).arg(priority).arg(ipStr).toUtf8().constData());
      }
    }

    recvUniverse.priority = priority;
    recvUniverse.ip = ip;
    m_sACNRecv.dirtyUniverses.insert(universe);

    if (slot_count != 0 && pdata)
      memcpy(recvUniverse.dmx.data(), pdata, std::min(recvUniverse.dmx.size(), static_cast<size_t>(slot_count)));
  }
  else if (start_code == STARTCODE_PRIORITY)
  {
    if (result.second)
    {
      char cidStr[CID::CIDSTRINGBYTES];
      CID::CIDIntoString(source, cidStr);

      char ipStr[CIPAddr::ADDRSTRINGBYTES];
      CIPAddr::AddrIntoString(source_ip, ipStr, /*showport*/ false, /*showint*/ false);

      recvUniverse.ips.insert(ip);

      m_sACNRecv.log.AddInfo(QStringLiteral("sACN universe %1 per channel priority source appeared: %2 {%3}, ip: %4").arg(universe).arg(recvSource.name).arg(cidStr).arg(ipStr).toUtf8().constData());
    }
    else if (!recvUniverse.hasPerChannelPriority)
    {
      char str[CID::CIDSTRINGBYTES];
      CID::CIDIntoString(source, str);
      m_sACNRecv.log.AddInfo(QStringLiteral("sACN universe %1 changed to per channel priority: %2 {%3}").arg(universe).arg(recvSource.name).arg(str).toUtf8().constData());
    }
    else if (recvUniverse.ips.insert(ip).second)
    {
      char cidStr[CID::CIDSTRINGBYTES];
      CID::CIDIntoString(source, cidStr);

      char ipStr[CIPAddr::ADDRSTRINGBYTES];
      CIPAddr::AddrIntoString(source_ip, ipStr, /*showport*/ false, /*showint*/ false);

      m_sACNRecv.log.AddInfo(QStringLiteral("sACN universe %1 per channel priority source appeared: %2 {%3}, ip: %4").arg(universe).arg(recvSource.name).arg(cidStr).arg(ipStr).toUtf8().constData());
    }

    recvUniverse.hasPerChannelPriority = true;
    recvUniverse.ip = ip;
    m_sACNRecv.dirtyUniverses.insert(universe);

    if (slot_count != 0 && pdata)
      memcpy(recvUniverse.channelPriority.data(), pdata, std::min(recvUniverse.channelPriority.size(), static_cast<size_t>(slot_count)));
  }
}

////////////////////////////////////////////////////////////////////////////////

void RouterThread::LogMIDI(bool send, const std::string &name, const std::vector<unsigned char> &message)
{
  const char *kHex = "0123456789ABCDEF";

  std::string log = send ? "MIDI OUT [" : "MIDI IN  [";
  log += name;
  log += "]";

  size_t hexOffset = log.size();
  log.resize(hexOffset + message.size() * 3);

  for (size_t i = 0; i < message.size(); ++i)
  {
    unsigned char c = message[i];
    size_t offset = hexOffset + i * 3;
    log[offset] = ' ';
    log[offset + 1] = kHex[(c >> 4) & 0x0f];
    log[offset + 2] = kHex[c & 0x0f];
  }

  m_PrivateLog.Add(send ? EosLog::LOG_MSG_TYPE_SEND : EosLog::LOG_MSG_TYPE_RECV, log);
}

////////////////////////////////////////////////////////////////////////////////

void RouterThread::LogOTP(const std::string &name, const otp::LogMsg &msg)
{
  if (msg.text.isEmpty())
    return;

  switch (msg.type)
  {
    case QtDebugMsg: m_PrivateLog.AddDebug(name + msg.text.toStdString()); break;
    case QtWarningMsg: m_PrivateLog.AddWarning(name + msg.text.toStdString()); break;

    case QtCriticalMsg:
    case QtFatalMsg: m_PrivateLog.AddError(name + msg.text.toStdString()); break;

    default: m_PrivateLog.AddInfo(name + msg.text.toStdString()); break;
  }
}

////////////////////////////////////////////////////////////////////////////////

void RouterThread::ConsumerCallback_ProducerChanged(otp::EndpointChange change, const QUuid &cid, const otp::Endpoint &producer)
{
  switch (change)
  {
    case otp::EndpointChange::kAppeared:
      ConsumerCallback_Log(otp::LogMsg(QtInfoMsg, QStringLiteral("producer appeared, cid(%1), %2").arg(cid.toString(QUuid::WithoutBraces)).arg(producer.toString())));
      break;

    case otp::EndpointChange::kExpired:
      ConsumerCallback_Log(otp::LogMsg(QtInfoMsg, QStringLiteral("producer expired, cid(%1), %2").arg(cid.toString(QUuid::WithoutBraces)).arg(producer.toString())));
      break;

    case otp::EndpointChange::kUpdated:
      ConsumerCallback_Log(otp::LogMsg(QtInfoMsg, QStringLiteral("producer changed, cid(%1), %2").arg(cid.toString(QUuid::WithoutBraces)).arg(producer.toString())));
      break;

    default: break;
  }
}

////////////////////////////////////////////////////////////////////////////////

void RouterThread::ConsumerCallback_PointChanged(otp::PointChange /*change*/, const otp::Frame &frame, const otp::Point &point)
{
  m_OTPRecv.systems[frame.system].groups[frame.group].points[frame.point] = point;
}

////////////////////////////////////////////////////////////////////////////////

void RouterThread::ConsumerCallback_Log(const otp::LogMsg &msg)
{
  LogOTP("OTP Consumer: ", msg);
}

////////////////////////////////////////////////////////////////////////////////

void RouterThread::ProducerCallback_ConsumerChanged(otp::EndpointChange change, const QUuid &cid, const otp::Endpoint &consumer)
{
  switch (change)
  {
    case otp::EndpointChange::kAppeared:
      ProducerCallback_Log(otp::LogMsg(QtInfoMsg, QStringLiteral("consumer appeared, cid(%1), %2").arg(cid.toString(QUuid::WithoutBraces)).arg(consumer.toString())));
      break;

    case otp::EndpointChange::kExpired:
      ProducerCallback_Log(otp::LogMsg(QtInfoMsg, QStringLiteral("consumer expired, cid(%1), %2").arg(cid.toString(QUuid::WithoutBraces)).arg(consumer.toString())));
      break;

    case otp::EndpointChange::kUpdated:
      ProducerCallback_Log(otp::LogMsg(QtInfoMsg, QStringLiteral("consumer changed, cid(%1), %2").arg(cid.toString(QUuid::WithoutBraces)).arg(consumer.toString())));
      break;

    default: break;
  }
}

void RouterThread::ProducerCallback_Log(const otp::LogMsg &msg)
{
  LogOTP("OTP Producer: ", msg);
}

////////////////////////////////////////////////////////////////////////////////

QString ScriptEngine::evaluate(const QString &script, EosLog *log /*= nullptr*/, const QString &label /*= QString()*/, const QString &path /*= QString()*/, const OSCArgument *args /*= nullptr*/,
                               size_t argsCount /*= 0*/, const uint8_t *universe /*= nullptr*/, size_t universeCount /*= 0*/, EosPacket *packet /*= nullptr*/)
{
  // set globals
  m_JS.globalObject().setProperty(QLatin1String("NAME"), label);
  m_JS.globalObject().setProperty(QLatin1String("OSC"), path);

  QJSValue jsarray;

  if (args && argsCount != 0)
  {
    jsarray = m_JS.newArray(static_cast<quint32>(argsCount));
    for (quint32 i = 0; i < static_cast<quint32>(argsCount); ++i)
    {
      switch (args[i].GetType())
      {
        case OSCArgument::OSC_TYPE_INT32:
        case OSCArgument::OSC_TYPE_INT64:
        case OSCArgument::OSC_TYPE_TIME:
        case OSCArgument::OSC_TYPE_RGBA32:
        case OSCArgument::OSC_TYPE_MIDI:
        {
          int n = 0;
          if (args[i].GetInt(n))
            jsarray.setProperty(i, n);
        }
        break;

        case OSCArgument::OSC_TYPE_FLOAT32:
        {
          float n = 0;
          if (args[i].GetFloat(n))
            jsarray.setProperty(i, n);
        }
        break;

        case OSCArgument::OSC_TYPE_FLOAT64:
        {
          double n = 0;
          if (args[i].GetDouble(n))
            jsarray.setProperty(i, n);
        }
        break;

        case OSCArgument::OSC_TYPE_TRUE: jsarray.setProperty(i, true); break;
        case OSCArgument::OSC_TYPE_FALSE: jsarray.setProperty(i, false); break;
        case OSCArgument::OSC_TYPE_INFINITY: jsarray.setProperty(i, std::numeric_limits<int>::infinity()); break;

        default:
        {
          std::string str;
          if (args[i].GetString(str))
            jsarray.setProperty(i, QString::fromStdString(str));
        }
        break;
      }
    }
  }
  else if (universe && universeCount != 0)
  {
    jsarray = m_JS.newArray(static_cast<quint32>(universeCount));
    for (quint32 i = 0; i < static_cast<quint32>(universeCount); ++i)
      jsarray.setProperty(i, universe[i]);
  }
  else
    jsarray = m_JS.newArray(0);

  m_JS.globalObject().setProperty(QLatin1String("ARGS"), jsarray);
  m_JS.globalObject().setProperty(QLatin1String("LOGS"), m_JS.newArray(0));

  // evaluate
  QStringList stack_trace;
  QJSValue eval = m_JS.evaluate(script, QString(), 1, &stack_trace);
  if (eval.isError())
  {
    QString error = eval.toString();

    if (!stack_trace.isEmpty())
    {
      if (!error.isEmpty())
        error += QLatin1Char('\n');

      error += stack_trace.join(QLatin1Char('\n'));
    }

    return error;
  }

  if (log)
  {
    jsarray = m_JS.globalObject().property(QLatin1String("LOGS"));
    quint32 count = jsarray.property(QLatin1String("length")).toUInt();
    for (quint32 i = 0; i < count; ++i)
    {
      QString msg = jsarray.property(i).toString();
      if (!msg.isEmpty())
        log->AddInfo(msg.toUtf8().constData());
    }
  }

  if (!packet)
    return QString();  // done with evaluation, no packet needed

  QString sendPath = m_JS.globalObject().property(QLatin1String("OSC")).toString();
  if (sendPath.isEmpty())
    return QString();  // done with evaluation, no packet needed

  OSCPacketWriter osc(sendPath.toUtf8().constData());

  jsarray = m_JS.globalObject().property(QLatin1String("ARGS"));
  quint32 count = jsarray.property(QLatin1String("length")).toUInt();
  for (quint32 i = 0; i < count; ++i)
  {
    QJSValue arg = jsarray.property(i);
    switch (arg.toPrimitive().type())
    {
      case QJSPrimitiveValue::Boolean: osc.AddBool(arg.toBool()); break;
      case QJSPrimitiveValue::Integer: osc.AddInt32(arg.toInt()); break;
      case QJSPrimitiveValue::Double: osc.AddFloat32(static_cast<float>(arg.toNumber())); break;
      case QJSPrimitiveValue::String: osc.AddString(arg.toString().toStdString()); break;
      default: break;
    }
  }

  size_t packetSize = 0;
  char *packetData = osc.Create(packetSize);
  if (packetData && packetSize)
  {
    *packet = EosPacket(packetData, static_cast<int>(packetSize));
    delete[] packetData;
  }

  return QString();
}

////////////////////////////////////////////////////////////////////////////////
