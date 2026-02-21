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

#pragma once
#ifndef ROUTER_H
#define ROUTER_H

#ifndef NETWORK_UTILS_H
#include "NetworkUtils.h"
#endif

#ifndef EOS_LOG_H
#include "EosLog.h"
#endif

#ifndef OSC_PARSER_H
#include "OSCParser.h"
#endif

#ifndef ITEM_STATE_H
#include "ItemState.h"
#endif

#ifndef NETWORK_UTILS_H
#include "NetworkUtils.h"
#endif

#ifndef _DEFTYPES_H_
#include "deftypes.h"
#endif

#ifndef _CID_H_
#include "CID.h"
#endif

#ifndef _IPADDR_H_
#include "ipaddr.h"
#endif

#ifndef _ASYNCSOCKETINTERFACE_H_
#include "AsyncSocketInterface.h"
#endif

#ifndef _STREAMACNCLIINTERFACE_H_
#include "StreamACNCliInterface.h"
#endif

#ifndef _STREAMACNSRVINTERFACE_H_
#include "StreamACNSrvInterface.h"
#endif

#ifdef WIN32

#include <WinSock2.h>

#ifndef _WIN_ASYNCSOCKETINTERFACE_H_
#include "Win_AsyncSocketInterface.h"
#endif

#ifndef _WIN_STREAMACNCLIINTERFACE_H_
#include "Win_StreamACNCliInterface.h"
#endif

#ifndef _WIN_STREAMACNSRVINTERFACE_H_
#include "Win_StreamACNSrvInterface.h"
#endif

#define IPlatformAsyncSocketServ IWinAsyncSocketServ
#define IPlatformStreamACNCli IWinStreamACNCli
#define IPlatformStreamACNSrv IWinStreamACNSrv

#else

#ifndef _OSX_ASYNCSOCKETINTERFACE_H_
#include "OSX_AsyncSocketInterface.h"
#endif

#ifndef _OSX_STREAMACNCLIINTERFACE_H_
#include "OSX_StreamACNCliInterface.h"
#endif

#ifndef _OSX_STREAMACNSRVINTERFACE_H_
#include "OSX_StreamACNSrvInterface.h"
#endif

#define IPlatformAsyncSocketServ IOSXAsyncSocketServ
#define IPlatformStreamACNCli IOSXStreamACNCli
#define IPlatformStreamACNSrv IOSXStreamACNSrv

#endif

#include "artnet/artnet.h"

#ifndef RTMIDI_H
#include "RtMidi.h"
#endif

#include "otp/otp.h"

#include <unordered_set>

class EosTcp;

namespace psn
{
class psn_decoder;
class psn_encoder;
};  // namespace psn

////////////////////////////////////////////////////////////////////////////////

class ScriptEngine
{
public:
  ScriptEngine() = default;

  QJSEngine &js() { return m_JS; }
  QString evaluate(const QString &script, EosLog *log = nullptr, const QString &label = QString(), const QString &path = QString(), const OSCArgument *args = nullptr, size_t argsCount = 0,
                   const uint8_t *universe = nullptr, size_t universeCount = 0, EosPacket *packet = nullptr);

private:
  QJSEngine m_JS;
};

////////////////////////////////////////////////////////////////////////////////

class Router
{
public:
  struct sConnection
  {
    QString label;
    bool server = false;
    OSCStream::EnumFrameMode frameMode = OSCStream::FRAME_MODE_DEFAULT;
    EosAddr addr;
    ItemStateTable::ID itemStateTableId = ItemStateTable::sm_Invalid_Id;
  };

  typedef std::vector<sConnection> CONNECTIONS;

  struct sRoute
  {
    sRoute() {}
    bool enable = true;
    bool mute = false;
    QString label;
    EosRouteSrc src;
    ItemStateTable::ID srcItemStateTableId = ItemStateTable::sm_Invalid_Id;
    EosRouteDst dst;
    ItemStateTable::ID dstItemStateTableId = ItemStateTable::sm_Invalid_Id;
  };

  struct Settings
  {
    QString sACNIP;
    QString artNetIP;
    bool levelChangesOnly = false;
    QString otpIP;
    otp::ModuleTypeList otpModuleTypes = {otp::ModuleType::kPos};
    QString script;
  };

  typedef std::vector<sRoute> ROUTES;

  static uint16_t GetDefaultPSNPort();
  static QString GetDefaultPSNIP();
};

////////////////////////////////////////////////////////////////////////////////

class PacketLogger : public OSCParserClient
{
public:
  PacketLogger(EosLog::EnumLogMsgType logType, EosLog &log)
    : m_LogType(logType)
    , m_pLog(&log)
  {
  }

  virtual void SetPrefix(const std::string &prefix) { m_Prefix = prefix; }
  virtual void OSCParserClient_Log(const std::string &message);
  virtual void OSCParserClient_Send(const char *, size_t) {}
  virtual void PrintPacket(OSCParser &oscParser, const char *packet, size_t size);

protected:
  EosLog::EnumLogMsgType m_LogType;
  EosLog *m_pLog;
  std::string m_Prefix;
  std::string m_LogMsg;
};

////////////////////////////////////////////////////////////////////////////////

class EosUdpInThread : public QThread
{
public:
  struct sRecvPacket
  {
    sRecvPacket(const char *data, int size, unsigned int Ip)
      : packet(data, size)
      , ip(Ip)
    {
    }
    EosPacket packet;
    unsigned int ip;
  };
  typedef std::vector<sRecvPacket> RECV_Q;

  struct sRecvPortPacket
  {
    sRecvPortPacket(uint16_t Port, const char *data, int size, unsigned int Ip)
      : port(Port)
      , p(data, size, Ip)
    {
    }
    uint16_t port;
    sRecvPacket p;
  };
  typedef std::vector<sRecvPortPacket> RECV_PORT_Q;

  EosUdpInThread();
  virtual ~EosUdpInThread();

  virtual void Start(const EosAddr &addr, QString multicastIP, Protocol protocol, ItemStateTable::ID itemStateTableId, unsigned int reconnectDelayMS, bool mute);
  virtual void Stop();
  const EosAddr &GetAddr() const { return m_Addr; }
  Protocol GetProtocol() const { return m_Protocol; }
  ItemStateTable::ID GetItemStateTableId() const { return m_ItemStateTableId; }
  ItemState::EnumState GetState();
  virtual void Flush(EosLog::LOG_Q &logQ, RECV_Q &recvQ);
  virtual void Mute(bool b) { m_Mute = b; }

protected:
  EosAddr m_Addr;
  QString m_MulticastIP;
  Protocol m_Protocol = Protocol::kDefault;
  ItemStateTable::ID m_ItemStateTableId;
  ItemState::EnumState m_State;
  unsigned int m_ReconnectDelay;
  bool m_Run;
  EosLog m_Log;
  EosLog m_PrivateLog;
  RECV_Q m_Q;
  QRecursiveMutex m_Mutex;
  psn::psn_decoder *m_PSNDecoder = nullptr;
  std::optional<uint8_t> m_PSNFrame;
  bool m_Mute;

  virtual void run();
  virtual void UpdateLog();
  virtual void SetState(ItemState::EnumState state);
  virtual void RecvPacket(const QHostAddress &host, const char *data, int len, OSCParser &logParser, PacketLogger &packetLogger);
  virtual void RecvPacketPSN(const QHostAddress &host, const char *data, int len, OSCParser &logParser, PacketLogger &packetLogger);
  virtual void QueuePacket(const QHostAddress &host, const char *data, int len, OSCParser &logParser, PacketLogger &packetLogger);
};

////////////////////////////////////////////////////////////////////////////////

class EosUdpOutThread : public QThread
{
public:
  EosUdpOutThread();
  virtual ~EosUdpOutThread();

  virtual void Start(const EosAddr &addr, ItemStateTable::ID itemStateTableId, unsigned int reconnectDelayMS);
  virtual void Stop();
  const EosAddr &GetAddr() const { return m_Addr; }
  ItemStateTable::ID GetItemStateTableId() const { return m_ItemStateTableId; }
  ItemState::EnumState GetState();
  virtual bool Send(const EosPacket &packet);
  virtual void Flush(EosLog::LOG_Q &logQ);

protected:
  EosAddr m_Addr;
  ItemStateTable::ID m_ItemStateTableId;
  ItemState::EnumState m_State;
  unsigned int m_ReconnectDelay;
  bool m_Run;
  EosLog m_Log;
  EosLog m_PrivateLog;
  EosPacket::Q m_Q;
  bool m_QEnabled;
  QRecursiveMutex m_Mutex;

  virtual void run();
  virtual void UpdateLog();
  virtual void SetState(ItemState::EnumState state);
};

////////////////////////////////////////////////////////////////////////////////

class EosTcpClientThread : public QThread
{
public:
  EosTcpClientThread();
  virtual ~EosTcpClientThread();

  virtual void Start(const EosAddr &addr, ItemStateTable::ID itemStateTableId, OSCStream::EnumFrameMode frameMode, unsigned int reconnectDelayMS, bool mute);
  virtual void Start(EosTcp *tcp, const EosAddr &addr, ItemStateTable::ID itemStateTableId, OSCStream::EnumFrameMode frameMode, unsigned int reconnectDelayMS, bool mute);
  virtual void Stop();
  const EosAddr &GetAddr() const { return m_Addr; }
  ItemStateTable::ID GetItemStateTableId() const { return m_ItemStateTableId; }
  ItemState::EnumState GetState();
  virtual bool Send(const EosPacket &packet);
  virtual bool SendFramed(const EosPacket &packet);
  virtual void Flush(EosLog::LOG_Q &logQ, EosUdpInThread::RECV_Q &recvQ);
  virtual void Mute(bool b) { m_Mute = b; }

protected:
  EosTcp *m_AcceptedTcp;
  EosAddr m_Addr;
  ItemStateTable::ID m_ItemStateTableId;
  ItemState::EnumState m_State;
  OSCStream::EnumFrameMode m_FrameMode;
  unsigned int m_ReconnectDelay;
  bool m_Run;
  EosLog m_Log;
  EosLog m_PrivateLog;
  EosUdpInThread::RECV_Q m_RecvQ;
  EosPacket::Q m_SendQ;
  QRecursiveMutex m_Mutex;
  bool m_Mute;

  virtual void run();
  virtual void UpdateLog();
  virtual void SetState(ItemState::EnumState state);
};

////////////////////////////////////////////////////////////////////////////////

class EosTcpServerThread : public QThread
{
public:
  struct sConnection
  {
    sConnection()
      : tcp(0)
    {
    }
    EosTcp *tcp;
    EosAddr addr;
  };
  typedef std::vector<sConnection> CONNECTION_Q;

  EosTcpServerThread();
  virtual ~EosTcpServerThread();

  virtual void Start(const EosAddr &addr, ItemStateTable::ID itemStateTableId, OSCStream::EnumFrameMode frameMode, unsigned int reconnectDelayMS);
  virtual void Stop();
  const EosAddr &GetAddr() const { return m_Addr; }
  ItemStateTable::ID GetItemStateTableId() const { return m_ItemStateTableId; }
  ItemState::EnumState GetState();
  OSCStream::EnumFrameMode GetFrameMode() const { return m_FrameMode; }
  virtual void Flush(EosLog::LOG_Q &logQ, CONNECTION_Q &connectionQ);

protected:
  EosAddr m_Addr;
  ItemStateTable::ID m_ItemStateTableId;
  ItemState::EnumState m_State;
  OSCStream::EnumFrameMode m_FrameMode;
  unsigned int m_ReconnectDelay;
  bool m_Run;
  EosLog m_Log;
  EosLog m_PrivateLog;
  CONNECTION_Q m_Q;
  QRecursiveMutex m_Mutex;

  virtual void run();
  virtual void UpdateLog();
  virtual void SetState(ItemState::EnumState state);
};

////////////////////////////////////////////////////////////////////////////////

class OSCBundleMethod : public OSCMethod
{
public:
  virtual void SetIP(unsigned int ip) { m_IP = ip; }
  virtual bool ProcessPacket(OSCParserClient &client, char *buf, size_t size);
  virtual void Flush(EosUdpInThread::RECV_Q &q);

private:
  unsigned int m_IP = 0u;
  EosUdpInThread::RECV_Q m_Q;
};

////////////////////////////////////////////////////////////////////////////////

class RouterThread : public QThread, private OSCParserClient, private IStreamACNCliNotify, private otp::Consumer::Callbacks, private otp::Producer::Callbacks
{
public:
  struct ArtNetSendUniverse
  {
    std::array<uint8_t, ARTNET_DMX_LENGTH> dmx;
    bool dirty = true;
    QElapsedTimer timer;
  };

  struct ArtNetRecvUniverse
  {
    artnet_node node = nullptr;
    std::array<uint8_t, ARTNET_DMX_LENGTH> prevDMX;
    bool hasPrevDMX = false;
  };

  typedef std::unordered_map<uint8_t, ArtNetSendUniverse> ARTNET_SEND_UNIVERSE_LIST;
  typedef std::unordered_map<uint8_t, ArtNetRecvUniverse> ARTNET_RECV_UNIVERSE_LIST;
  typedef std::unordered_map<artnet_node, /*ip*/ unsigned int> ARTNET_NODE_IP_LIST;
  typedef std::unordered_set<artnet_node> ARTNET_DIRTY_LIST;

  struct ArtNet
  {
    artnet_node server = nullptr;
    ARTNET_SEND_UNIVERSE_LIST output;
    ARTNET_RECV_UNIVERSE_LIST inputs;
    ARTNET_NODE_IP_LIST inputIPs;
    ARTNET_DIRTY_LIST dirty;
  };

  RouterThread(const Router::ROUTES &routes, const Router::CONNECTIONS &tcpConnections, const Router::Settings &settings, const ItemStateTable &itemStateTable, unsigned int reconnectDelayMS);
  virtual ~RouterThread();

  virtual void Stop();
  virtual void Sync(EosLog::LOG_Q &logQ, ItemStateTable &itemStateTable);

protected:
  struct sRouteDst
  {
    QString label;
    EosRouteDst dst;
    ItemStateTable::ID srcItemStateTableId;
    ItemStateTable::ID dstItemStateTableId;
  };

  typedef std::vector<sRouteDst> ROUTE_DESTINATIONS;

  typedef std::map<QString, ROUTE_DESTINATIONS> ROUTES_BY_PATH;
  typedef std::pair<QString, ROUTE_DESTINATIONS> ROUTES_BY_PATH_PAIR;

  struct sRoutesByIp
  {
    ROUTES_BY_PATH routesByPath;
    ROUTES_BY_PATH routesByWildcardPath;
  };

  typedef std::map<unsigned int, sRoutesByIp> ROUTES_BY_IP;
  typedef std::pair<unsigned int, sRoutesByIp> ROUTES_BY_IP_PAIR;

  typedef std::map<unsigned short, ROUTES_BY_IP> ROUTES_BY_PORT;
  typedef std::pair<unsigned short, ROUTES_BY_IP> ROUTES_BY_PORT_PAIR;

  typedef std::map<EosAddr, EosUdpInThread *> UDP_IN_THREADS;
  typedef std::map<EosAddr, EosUdpOutThread *> UDP_OUT_THREADS;

  typedef std::map<EosAddr, EosTcpClientThread *> TCP_CLIENT_THREADS;
  typedef std::map<EosAddr, EosTcpServerThread *> TCP_SERVER_THREADS;

  typedef std::vector<const ROUTE_DESTINATIONS *> DESTINATIONS_LIST;

  enum EnumConstants
  {
    UNIVERSE_SIZE = 512,
    DEFAULT_PRIORITY = 100
  };

  struct Universe
  {
    uint8_t priority = 0;
    unsigned int ip = 0;
    std::unordered_set<unsigned int> ips;
    bool hasPerChannelPriority = false;
    bool hasPrevDMX = false;
    std::array<uint8_t, UNIVERSE_SIZE> dmx;
    std::array<uint8_t, UNIVERSE_SIZE> prevDMX;
    std::array<uint8_t, UNIVERSE_SIZE> channelPriority;

    Universe()
    {
      dmx.fill(0);
      channelPriority.fill(0);
    }
  };

  typedef std::map<uint16_t, Universe> UNIVERSE_LIST;

  struct sACNSource
  {
    QString name;
    UNIVERSE_LIST universes;
  };

  typedef std::map<CID, sACNSource> SACN_SOURCE_LIST;
  typedef std::unordered_set<uint16_t> UNIVERSE_NUMBER_SET;

  struct sACNRecv
  {
    QRecursiveMutex mutex;
    UNIVERSE_NUMBER_SET dirtyUniverses;
    SACN_SOURCE_LIST sources;
    UNIVERSE_LIST merged;
    EosLog log;
  };

  struct SendUniverseData
  {
    uint handle = 0;
    uint1 *channels = nullptr;
  };

  struct SendUniverse
  {
    uint1 priority = static_cast<uint1>(DEFAULT_PRIORITY);
    uint1 perChannelPriority = static_cast<uint1>(DEFAULT_PRIORITY);
    SendUniverseData dmx;
    SendUniverseData channelPriority;
  };

  typedef std::unordered_map<uint16_t, SendUniverse> SEND_UNIVERSE_LIST;

  struct sACN
  {
    IPlatformAsyncSocketServ *net = nullptr;
    IPlatformStreamACNCli *client = nullptr;
    IPlatformStreamACNSrv *server = nullptr;
    SEND_UNIVERSE_LIST output;
    QElapsedTimer recvTimer;
    QElapsedTimer sendTimer;
    std::vector<netintid> ifaces;

    netintid *GetNetIFList() { return ifaces.empty() ? nullptr : ifaces.data(); }
    int GetNetIFListSize() { return static_cast<int>(ifaces.size()); }
  };

  struct MIDIIn
  {
    std::shared_ptr<RtMidiIn> midi;
    std::string name;
  };

  struct MIDIOut
  {
    std::shared_ptr<RtMidiOut> midi;
    std::string name;
  };

  typedef std::map<unsigned int, MIDIIn> MIDI_INPUT_LIST;
  typedef std::map<unsigned int, MIDIOut> MIDI_OUTPUT_LIST;

  struct MIDI
  {
    MIDI_INPUT_LIST inputs;
    MIDI_OUTPUT_LIST outputs;
  };

  struct OTPI
  {
    otp::Consumer consumer;
    otp::Producer producer;
  };

  struct MuteAll
  {
    bool incoming = false;
    bool outgoing = false;
  };

  bool m_Run;
  unsigned int m_ReconnectDelay;
  Router::ROUTES m_Routes;
  Router::CONNECTIONS m_TcpConnections;
  Router::Settings m_Settings;
  EosLog m_Log;
  EosLog m_PrivateLog;
  ItemStateTable m_ItemStateTable;
  QRecursiveMutex m_Mutex;
  ScriptEngine *m_ScriptEngine = nullptr;
  psn::psn_encoder *m_PSNEncoder = nullptr;
  QElapsedTimer m_PSNEncoderTimer;
  sACNRecv m_sACNRecv;
  otp::Data m_OTPRecv;

  virtual void run();
  virtual void MainLoop();
  virtual void RecvsACN(sACN &sacn, EosUdpInThread::RECV_PORT_Q &recvPortQ);
  virtual void RecvArtNet(ArtNet &artnet, EosUdpInThread::RECV_PORT_Q &recvPortQ);
  virtual void RecvMIDI(OSCParser &oscParser, PacketLogger &packetLogger, bool muteAllIncoming, bool muteAllOutgoing, sACN &sacn, ArtNet &artnet, MIDI &midi, OTPI &otpi, ROUTES_BY_PORT &routesByPort,
                        DESTINATIONS_LIST &routingDestinationList, UDP_OUT_THREADS &udpOutThreads, TCP_SERVER_THREADS &tcpServerThreads, TCP_CLIENT_THREADS &tcpClientThreads);
  virtual void RecvOTP(OSCParser &oscParser, PacketLogger &packetLogger, bool muteAllIncoming, bool muteAllOutgoing, sACN &sacn, ArtNet &artnet, MIDI &midi, OTPI &otpi, ROUTES_BY_PORT &routesByPort,
                       DESTINATIONS_LIST &routingDestinationList, UDP_OUT_THREADS &udpOutThreads, TCP_SERVER_THREADS &tcpServerThreads, TCP_CLIENT_THREADS &tcpClientThreads);
  virtual void BuildRoutes(ROUTES_BY_PORT &routesByPort, ROUTES_BY_PORT &routesBysACNUniverse, ROUTES_BY_PORT &routesByArtNetUniverse, ROUTES_BY_PORT &routesByMIDI, ROUTES_BY_PORT &routesByOTP,
                           UDP_IN_THREADS &udpInThreads, UDP_OUT_THREADS &udpOutThreads, TCP_CLIENT_THREADS &tcpClientThreads, TCP_SERVER_THREADS &tcpServerThreads);
  virtual void BuildsACN(ROUTES_BY_PORT &routesByPort, ROUTES_BY_PORT &routesBysACNUniverse, ROUTES_BY_PORT &routesByArtNetUniverse, ROUTES_BY_PORT &routesByMIDI, ROUTES_BY_PORT &routesByOTP,
                         sACN &sacn);
  virtual void BuildArtNet(ROUTES_BY_PORT &routesByPort, ROUTES_BY_PORT &routesBysACNUniverse, ROUTES_BY_PORT &routesByArtNetUniverse, ROUTES_BY_PORT &routesByMIDI, ROUTES_BY_PORT &routesByOTP,
                           ArtNet &artnet);
  virtual void BuildMIDI(ROUTES_BY_PORT &routesByMIDI, MIDI &midi);
  virtual void BuildOTP(ROUTES_BY_PORT &routesByPort, ROUTES_BY_PORT &routesBysACNUniverse, ROUTES_BY_PORT &routesByArtNetUniverse, ROUTES_BY_PORT &routesByMIDI, ROUTES_BY_PORT &routesByOTP,
                        OTPI &otpi);
  virtual EosUdpOutThread *CreateUdpOutThread(const EosAddr &addr, ItemStateTable::ID itemStateTableId, UDP_OUT_THREADS &udpOutThreads);
  virtual void AddRoutingDestinations(bool isOSC, const QString &path, const sRoutesByIp &routesByIp, DESTINATIONS_LIST &destinations);
  virtual void ProcessRecvQ(bool muteAllOutgoing, sACN &sacn, ArtNet &artnet, MIDI &midi, OTPI &otpi, OSCParser &oscBundleParser, ROUTES_BY_PORT &routesByPort,
                            DESTINATIONS_LIST &routingDestinationList, UDP_OUT_THREADS &udpOutThreads, TCP_SERVER_THREADS &tcpServerThreads, TCP_CLIENT_THREADS &tcpClientThreads, const EosAddr &addr,
                            EosUdpInThread::RECV_Q &recvQ);
  virtual void ProcessRecvPacket(bool muteAllOutgoing, sACN &sacn, ArtNet &artnet, MIDI &midi, OTPI &otpi, ROUTES_BY_PORT &routesByPort, DESTINATIONS_LIST &routingDestinationList,
                                 UDP_OUT_THREADS &udpOutThreads, TCP_SERVER_THREADS &tcpServerThreads, TCP_CLIENT_THREADS &tcpClientThreads, const EosAddr &addr, Protocol protocol,
                                 EosUdpInThread::sRecvPacket &recvPacket);
  virtual bool MakeOSCPacket(ArtNet &artnet, const EosAddr &addr, Protocol protocol, const QString &srcPath, const sRouteDst &route, OSCArgument *args, size_t argsCount, EosPacket &packet);
  virtual bool MakePSNPacket(EosPacket &osc, EosPacket &psn);
  virtual bool SendsACN(sACN &sacn, ArtNet &artnet, const EosAddr &addr, Protocol protocol, const sRouteDst &routeDst, EosPacket &osc);
  virtual bool SendArtNet(ArtNet &artnet, const EosAddr &addr, Protocol protocol, const EosRouteDst &dst, EosPacket &osc);
  virtual void FlushArtNet(ArtNet &artnet);
  virtual void SendMIDI(MIDI &midi, const sRouteDst &routeDst, EosPacket &oscPacket);
  virtual void SendOTP(OTPI &otpi, const sRouteDst &routeDst, EosPacket &oscPacket);
  virtual void ProcessTcpConnectionQ(TCP_CLIENT_THREADS &tcpClientThreads, EosTcpServerThread &tcpServer, EosTcpServerThread::CONNECTION_Q &tcpConnectionQ, bool mute);
  virtual bool ApplyTransform(OSCArgument &arg, const EosRouteDst &dst, OSCPacketWriter &packet);
  virtual void MakeSendPath(ArtNet &artnet, const EosAddr &addr, Protocol protocol, const QString &srcPath, const QString &dstPath, const OSCArgument *args, size_t argsCount, QString &sendPath);
  virtual void UpdateLog();
  virtual MuteAll GetMuteAll();
  virtual bool IsRouteMuted(ItemStateTable::ID id);
  virtual void SetItemState(ItemStateTable::ID id, ItemState::EnumState state);
  virtual void SetItemState(const ROUTES_BY_PORT &routesByPort, Protocol dstProtocol, ItemState::EnumState state);
  virtual void SetItemState(const ROUTES_BY_IP &routesByIp, Protocol dstProtocol, ItemState::EnumState state);
  virtual void SetItemState(const ROUTES_BY_PATH &routesByPath, Protocol dstProtocol, ItemState::EnumState state);
  virtual void SetItemActivity(ItemStateTable::ID id);
  virtual void DestroysACN(sACN &sacn);
  virtual void DestroyArtNet(ArtNet &artnet);
  virtual void LogMIDI(bool send, const std::string &name, const std::vector<unsigned char> &message);
  virtual void LogOTP(const std::string &name, const otp::LogMsg &msg);

  // OSCParserClient
  virtual void OSCParserClient_Log(const std::string &message);
  virtual void OSCParserClient_Send(const char *buf, size_t size);

  // IStreamACNCliNotify
  void SourceDisappeared(const CID &source, uint2 universe) override;
  void SourcePCPExpired(const CID &source, uint2 universe) override;
  void SamplingStarted(uint2 /*universe*/) override {}
  void SamplingEnded(uint2 /*universe*/) override {}
  void UniverseData(const CID &source, const char *source_name, const CIPAddr &source_ip, uint2 universe, uint2 reserved, uint1 sequence, uint1 options, uint1 priority, uint1 start_code,
                    uint2 slot_count, uint1 *pdata) override;
  void UniverseBad(uint2 /*universe*/, netintid /*iface*/) override {}

  // OTP Consumer Callbacks
  void ConsumerCallback_ProducerChanged(otp::EndpointChange change, const QUuid &cid, const otp::Endpoint &producer) override;
  void ConsumerCallback_PointChanged(otp::PointChange change, const otp::Frame &frame, const otp::Point &point) override;
  void ConsumerCallback_Log(const otp::LogMsg &msg) override;

  // OTP Producer Callbacks
  void ProducerCallback_ConsumerChanged(otp::EndpointChange change, const QUuid &cid, const otp::Endpoint &consumer) override;
  void ProducerCallback_Log(const otp::LogMsg &msg) override;

  static bool HasProtocolOutput(const ROUTES_BY_PORT &routesByPort, Protocol protocol);
  static bool HasProtocolOutput(const ROUTES_BY_PATH &routesByPath, Protocol protocol);
};

////////////////////////////////////////////////////////////////////////////////

#endif
