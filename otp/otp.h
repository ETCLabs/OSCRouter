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

#include <QtCore/QtCore>
#include <QtNetwork/QtNetwork>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <optional>
#include <chrono>

namespace otp
{
typedef uint8_t SystemNumber;
typedef uint16_t GroupNumber;
typedef uint32_t PointNumber;
typedef uint8_t PriorityNumber;
typedef uint64_t TimestampNumber;
typedef uint32_t FolioNumber;
typedef uint16_t PageNumber;

constexpr uint16_t kPort = 5568;
constexpr PriorityNumber kDefaultPriority = 100;
constexpr int kAdvertRate = 10000;               // ms
constexpr int kTransformRate = 3000;             // ms
constexpr int kTransformDataLossTimeout = 7500;  // ms
constexpr int kAdvertDataLossTimeout = 30000;    // ms
constexpr SystemNumber kMinSystemNumber = 0;
constexpr SystemNumber kMaxSystemNumber = 200;

enum class PointChange
{
  kNone,  // received a new point, but it did not change
  kSystemAppeared,
  kGroupAppeared,
  kPointAppeared,
  kPointUpdated
};

enum class EndpointChange
{
  kAppeared,
  kExpired,
  kUpdated
};

struct Vector3
{
  int32_t x = 0;
  int32_t y = 0;
  int32_t z = 0;

  Vector3() = default;
  Vector3(int32_t vx, int32_t vy, int32_t vz)
    : x(vx)
    , y(vy)
    , z(vz)
  {
  }
  bool operator==(const Vector3& other) const { return x == other.x && y == other.y && z == other.z; }
  bool operator!=(const Vector3& other) const { return !operator==(other); }
};  // namespace otp

struct Vector3u
{
  uint32_t x = 0;
  uint32_t y = 0;
  uint32_t z = 0;

  Vector3u() = default;
  Vector3u(uint32_t vx, uint32_t vy, uint32_t vz)
    : x(vx)
    , y(vy)
    , z(vz)
  {
  }
  bool operator==(const Vector3u& other) const { return x == other.x && y == other.y && z == other.z; }
  bool operator!=(const Vector3u& other) const { return !operator==(other); }
};

struct VelAccel
{
  Vector3 vel;
  Vector3 accel;

  VelAccel() = default;
  VelAccel(const Vector3& v, const Vector3& a)
    : vel(v)
    , accel(a)
  {
  }
  bool operator==(const VelAccel& other) const { return vel == other.vel && accel == other.accel; }
  bool operator!=(const VelAccel& other) const { return !operator==(other); }
};

struct Frame
{
  SystemNumber system = 0;
  GroupNumber group = 0;
  PointNumber point = 0;

  Frame() = default;
  Frame(SystemNumber s, GroupNumber g, PointNumber p)
    : system(s)
    , group(g)
    , point(p)
  {
  }
  bool operator==(const Frame& other) const { return system == other.system && group == other.group && point == other.point; }
  bool operator!=(const Frame& other) const { return !operator==(other); }
  QString toString() const { return QStringLiteral("%1/%2/%3").arg(system).arg(group).arg(point); }
};

typedef std::chrono::time_point<std::chrono::steady_clock> Timepoint;

enum class ModuleType
{
  kPos = 0,
  kPosVelAccel,
  kRot,
  kRotVelAccel,
  kScale,
  kFrame,

  kCount
};

typedef std::set<ModuleType> ModuleTypeList;

QString ModuleTypeText(ModuleType module_type);
QString ModuleTypesText(const ModuleTypeList& module_types);

struct Modules
{
  std::optional<Vector3> pos;             // micrometers
  std::optional<VelAccel> pos_vel_accel;  // micrometers/sec
  std::optional<Vector3u> rot;            // millionths of a decimal degree
  std::optional<VelAccel> rot_vel_accel;  // thousandths of a decimal degree/sec
  std::optional<Vector3> scale;           // unitless millionths
  std::optional<Frame> frame;

  bool operator==(const Modules& other) const
  {
    return pos == other.pos && pos_vel_accel == other.pos_vel_accel && rot == other.rot && rot_vel_accel == other.rot_vel_accel && scale == other.scale && frame == other.frame;
  }
  bool operator!=(const Modules& other) const { return !operator==(other); }
  bool empty() const { return !pos.has_value() && !pos_vel_accel.has_value() && !rot.has_value() && !rot_vel_accel.has_value() && !scale.has_value() && !frame.has_value(); }
  QString toString() const;
  void applyFilter(const ModuleTypeList& module_types);
};

struct Point
{
  PriorityNumber priority = kDefaultPriority;
  TimestampNumber timestamp = 0;
  Modules modules;
  QString name;

  QUuid cid;                                              // Consumer: owner of point based on priority/expiration
  Timepoint received = std::chrono::steady_clock::now();  // Consumer: point received for expiration

  bool operator==(const Point& other) const { return priority == other.priority && timestamp == other.timestamp && modules == other.modules && name == other.name && cid == other.cid; }
  bool operator!=(const Point& other) const { return !operator==(other); }
  bool isExpired() const;
  QString toString() const;
};

typedef std::map<PointNumber, Point> PointList;

struct Group
{
  PointList points;
};

typedef std::map<GroupNumber, Group> GroupList;

struct System
{
  TimestampNumber timestamp = 0;
  bool full_point_set = false;
  GroupList groups;
};

typedef std::map<SystemNumber, System> SystemList;
typedef std::set<SystemNumber> SystemSet;

struct Data
{
  SystemList systems;

  void toStrings(QStringList& strs) const;
};

struct Endpoint
{
  QString name;
  QHostAddress addr;
  ModuleTypeList module_types;
  Timepoint timepoint = std::chrono::steady_clock::now();

  bool isExpired() const;
  QString toString() const;
};

typedef std::map<QUuid, Endpoint> EndpointList;

struct LogMsg
{
  QtMsgType type = QtInfoMsg;
  QString text;

  explicit LogMsg(QtMsgType log_type = QtInfoMsg, const QString& log_text = QString())
    : type(log_type)
    , text(log_text)
  {
  }
};

struct Udp
{
  std::shared_ptr<QUdpSocket> sock;
  QNetworkInterface net;
};

typedef uint32_t IPv4;
typedef std::unordered_set<IPv4> IPv4List;
typedef std::unordered_map<IPv4, Udp> IPv4UdpSocketList;

struct NetInterfaces
{
  NetInterfaces(IPv4 net_ip = 0);
  NetInterfaces(const QString& net_ip);
  NetInterfaces(IPv4List& net_ips);
  NetInterfaces(const QStringList& net_ips);

  const IPv4List& getIPs() const { return ips; }
  void initSocketList(IPv4UdpSocketList& sockets, QStringList& errors, quint16 port = 0, QUdpSocket::BindMode mode = QUdpSocket::DefaultForPlatform);

  static QString toString(IPv4 net_ip);
  static IPv4 fromString(const QString& net_ip);

private:
  IPv4List ips;
};

class Message
{
public:
  enum class Type
  {
    kUnknown,
    kTransform,
    kAdvert
  };

  enum class AdvertType
  {
    kUnknown,
    kModule,
    kName,
    kSystem
  };

  enum class Op
  {
    kUnknown = 0,
    kRequest,
    kResponse
  };

  struct Advert
  {
    AdvertType type = AdvertType::kUnknown;
    Op op = Op::kUnknown;
    ModuleTypeList module_types;
  };

  struct Props
  {
    Type type = Type::kUnknown;
    QUuid cid;
    FolioNumber folio = 0;
    PageNumber page = 0;
    PageNumber last_page = 0;
    QString name;
    Advert advert;
    Data data;
  };

  Message() = default;
  Message(const Props& props);  // allow non-explicit
  const Props& GetProps() const { return props_; }
  void SetProps(const Props& props) { props_ = props; }
  bool FromDatagram(const QByteArray& datagram);
  bool ToDatagram(QByteArray& datagram) const;

  static QString MulticastTransformIP(SystemNumber system);
  static QString MulticastAdvertIP();
  static QByteArray ToProtocolString(const QString& str);
  static QString FromProtocolString(const QByteArray& str);

private:
  Props props_;

  bool FromDatagramTransform(QDataStream& st);
  bool FromDatagramAdvert(QDataStream& st);
  bool ToDatagramTransform(QDataStream& st) const;
  bool ToDatagramTransformPoint(QDataStream& st, GroupNumber group_number, PointNumber point_number, const Point& point) const;
  bool ToDatagramTransformModule(QDataStream& st, const Modules& modules, ModuleType module_type) const;
  bool ToDatagramAdvert(QDataStream& st) const;
  bool ToDatagramAdvertModule(QDataStream& st) const;
  bool ToDatagramAdvertName(QDataStream& st) const;
  bool ToDatagramAdvertSystem(QDataStream& st) const;
};

class Consumer
{
public:
  class Callbacks
  {
  public:
    virtual void ConsumerCallback_ProducerChanged(EndpointChange change, const QUuid& cid, const Endpoint& producer) = 0;
    virtual void ConsumerCallback_PointChanged(PointChange change, const Frame& frame, const Point& point) = 0;
    virtual void ConsumerCallback_Log(const LogMsg& msg) = 0;
  };

  explicit Consumer(const QString& name = QString(), const ModuleTypeList& module_types = {}, const QUuid& cid = QUuid::createUuid());
  virtual ~Consumer();

  Callbacks* GetCallbacks() const { return callbacks_; }
  void SetCallbacks(Callbacks* callbacks) { callbacks_ = callbacks; }
  const NetInterfaces& GetNetInterfaces() const { return nets_; }
  const EndpointList& GetProducers() const { return producers_; }
  const QString& GetName() const { return props_.name; }
  void SetName(const QString& name);
  const ModuleTypeList& GetModuleTypes() const { return props_.advert.module_types; }
  void SetModuleTypes(const ModuleTypeList& module_types);
  const QUuid& GetCID() const { return props_.cid; }
  void SetCID(const QUuid& cid);
  const SystemList& GetSystems() const { return systems_; }
  void SetSystems(const SystemSet& systems);
  void Init(const NetInterfaces& nets);
  void Tick();
  void Log(const LogMsg& msg);

private:
  struct Advert
  {
    QElapsedTimer timer;
    QByteArray datagram;
  };

  Callbacks* callbacks_ = nullptr;
  NetInterfaces nets_;
  IPv4UdpSocketList send_sockets_;
  IPv4UdpSocketList recv_sockets_;
  Message::Props props_;
  Advert advert_;
  SystemList systems_;
  QByteArray reusable_;
  EndpointList producers_;

  void TickAdvert();
  void UpdateAdvertDatagram();
  void TickProducers();
  void TickTransform();
  void AddProducer(const Message& message, const QHostAddress& addr);
  void MergeTransform(const Message& message);
};

class Producer
{
public:
  class Callbacks
  {
  public:
    virtual void ProducerCallback_ConsumerChanged(EndpointChange change, const QUuid& cid, const Endpoint& consumer) = 0;
    virtual void ProducerCallback_Log(const LogMsg& msg) = 0;
  };

  explicit Producer(const QString& name = QString(), const QUuid& cid = QUuid::createUuid());
  virtual ~Producer();

  Callbacks* GetCallbacks() const { return callbacks_; }
  void SetCallbacks(Callbacks* callbacks) { callbacks_ = callbacks; }
  const NetInterfaces& GetNetInterfaces() const { return nets_; }
  const EndpointList& GetConsumers() const { return consumers_; }
  const ModuleTypeList& GetModuleTypes() const { return mods_; }
  const QString& GetName() const { return props_.name; }
  void SetName(const QString& name) { props_.name = name; }
  const QUuid& GetCID() const { return props_.cid; }
  const Data& GetPoints() const { return props_.data; }
  PointChange SetPoint(const Frame& frame, const Point& point);
  bool RemovePoint(const Frame& frame);
  bool RemoveAllPoints();
  void Init(const NetInterfaces& nets);
  void Tick();
  void Log(const LogMsg& msg);

private:
  Callbacks* callbacks_ = nullptr;
  NetInterfaces nets_;
  IPv4UdpSocketList send_sockets_;
  IPv4UdpSocketList recv_sockets_;
  EndpointList consumers_;
  ModuleTypeList mods_;  // global module types all consumers are interested in
  Message::Props props_;
  QByteArray reusable_;
  QElapsedTimer resend_;

  void TickAdvert();
  void TickConsumers();
  void TickResend();
  void HandleAdvert(const Message& request, const QHostAddress& addr);
  void UpdateModulesTypes();
  void SendPoint(const Frame& frame, const Point& point);
  void SendPoints(const Data& data, bool full_point_set);
  void Send(const QByteArray& datagram, const QHostAddress& addr);
  void Leave();
};
}  // namespace otp
