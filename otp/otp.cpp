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

#include "otp/otp.h"

namespace otp
{
constexpr std::array<char, 12> kHeader = {0x4f, 0x54, 0x50, 0x2d, 0x45, 0x31, 0x2e, 0x35, 0x39, 0x00, 0x00, 0x00};

constexpr uint16_t kMessageTypeTransform = 1;
constexpr uint16_t kMessageTypeAdvert = 2;

constexpr uint16_t kAdvertTypeModule = 1;
constexpr uint16_t kAdvertTypeName = 2;
constexpr uint16_t kAdvertTypeSystem = 3;

constexpr uint16_t kAdvertModuleTypeList = 1;
constexpr uint16_t kAdvertModuleNameList = 1;
constexpr uint16_t kAdvertModuleSystemList = 1;

constexpr uint16_t kPoint = 1;
constexpr uint16_t kModule = 1;

constexpr uint16_t kPosModule = 1;
constexpr uint16_t kPosVelAccelModule = 2;
constexpr uint16_t kRotModule = 3;
constexpr uint16_t kRotVelAccelModule = 4;
constexpr uint16_t kScaleModule = 5;
constexpr uint16_t kFrameModule = 6;

constexpr uint16_t kESTAManufacturerId = 0;

QString ModuleTypeText(ModuleType module_type)
{
  switch (module_type)
  {
    case ModuleType::kPos: return QLatin1String("pos");
    case ModuleType::kPosVelAccel: return QLatin1String("posVelAccel");
    case ModuleType::kRot: return QLatin1String("rot");
    case ModuleType::kRotVelAccel: return QLatin1String("rotVelAccel");
    case ModuleType::kScale: return QLatin1String("scale");
    case ModuleType::kFrame: return QLatin1String("frame");
    default: break;
  }

  return QString::number(static_cast<int>(module_type));
}

QString ModuleTypesText(const ModuleTypeList& module_types)
{
  QString text;

  for (ModuleTypeList::const_iterator i = module_types.begin(); i != module_types.end(); ++i)
  {
    if (!text.isEmpty())
      text += QLatin1String(", ");

    text += ModuleTypeText(*i);
  }

  return text;
}

QString Modules::toString() const
{
  QString str;

  if (pos.has_value())
    str = QStringLiteral("pos(%1, %2, %3)").arg(pos->x).arg(pos->y).arg(pos->z);

  if (pos_vel_accel.has_value())
  {
    if (str.isEmpty())
      str += QLatin1String(", ");

    str += QStringLiteral("posVel(%1, %2, %3)").arg(pos_vel_accel->vel.x).arg(pos_vel_accel->vel.y).arg(pos_vel_accel->vel.z);
    str += QStringLiteral(", posAccel(%1, %2, %3)").arg(pos_vel_accel->accel.x).arg(pos_vel_accel->accel.y).arg(pos_vel_accel->accel.z);
  }

  if (rot.has_value())
  {
    if (str.isEmpty())
      str += QLatin1String(", ");

    str += QStringLiteral("rot(%1, %2, %3)").arg(rot->x).arg(rot->y).arg(rot->z);
  }

  if (rot_vel_accel.has_value())
  {
    if (str.isEmpty())
      str += QLatin1String(", ");

    str += QStringLiteral("rotVel(%1, %2, %3)").arg(rot_vel_accel->vel.x).arg(rot_vel_accel->vel.y).arg(rot_vel_accel->vel.z);
    str += QStringLiteral(", rotAccel(%1, %2, %3)").arg(rot_vel_accel->accel.x).arg(rot_vel_accel->accel.y).arg(rot_vel_accel->accel.z);
  }

  if (scale.has_value())
  {
    if (str.isEmpty())
      str += QLatin1String(", ");

    str += QStringLiteral("scale(%1, %2, %3)").arg(scale->x).arg(scale->y).arg(scale->z);
  }

  if (frame.has_value())
  {
    if (str.isEmpty())
      str += QLatin1String(", ");

    str += QStringLiteral("frame(%1, %2, %3)").arg(frame->system).arg(frame->group).arg(frame->point);
  }

  return str;
}

void Modules::applyFilter(const ModuleTypeList& module_types)
{
  if (pos.has_value() && module_types.find(ModuleType::kPos) == module_types.end())
    pos.reset();

  if (pos_vel_accel.has_value() && module_types.find(ModuleType::kPosVelAccel) == module_types.end())
    pos_vel_accel.reset();

  if (rot.has_value() && module_types.find(ModuleType::kRot) == module_types.end())
    rot.reset();

  if (rot_vel_accel.has_value() && module_types.find(ModuleType::kRotVelAccel) == module_types.end())
    rot_vel_accel.reset();

  if (scale.has_value() && module_types.find(ModuleType::kScale) == module_types.end())
    scale.reset();

  if (frame.has_value() && module_types.find(ModuleType::kFrame) == module_types.end())
    frame.reset();
}

bool Point::isExpired() const
{
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - received).count() > std::chrono::milliseconds(kTransformDataLossTimeout).count();
}

QString Point::toString() const
{
  QString str = QStringLiteral("cid(%1), name(%2), priority(%3)").arg(cid.toString(QUuid::WithoutBraces)).arg(name).arg(priority);

  QString modules_str = modules.toString();
  if (!modules_str.isEmpty())
    str += QLatin1String(", ") + modules_str;

  str += QStringLiteral(", time(%1)").arg(timestamp);

  return str;
}

void Data::toStrings(QStringList& strs) const
{
  strs.clear();

  for (SystemList::const_iterator system_iter = systems.begin(); system_iter != systems.end(); ++system_iter)
  {
    SystemNumber system_number = system_iter->first;
    const GroupList& groups = system_iter->second.groups;
    for (GroupList::const_iterator group_iter = groups.begin(); group_iter != groups.end(); ++group_iter)
    {
      GroupNumber group_number = group_iter->first;
      const PointList& points = group_iter->second.points;
      for (PointList::const_iterator point_iter = points.begin(); point_iter != points.end(); ++point_iter)
      {
        PointNumber point_number = point_iter->first;
        const Point& point = point_iter->second;
        strs << QStringLiteral("%1/%2/%3 ").arg(system_number).arg(group_number).arg(point_number) + point.toString();
      }
    }
  }
}

bool Endpoint::isExpired() const
{
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - timepoint).count() > std::chrono::milliseconds(kAdvertDataLossTimeout).count();
}

QString Endpoint::toString() const
{
  QString str = QStringLiteral("name(%1), ip(%2)").arg(name).arg(QHostAddress(addr.toIPv4Address()).toString());
  if (!module_types.empty())
    str += QStringLiteral(", mods(%1)").arg(ModuleTypesText(module_types));
  return str;
}

NetInterfaces::NetInterfaces(IPv4 net_ip /*= 0*/)
{
  if (net_ip != 0)
    ips.insert(net_ip);
}

NetInterfaces::NetInterfaces(const QString& net_ip)
{
  IPv4 ip = fromString(net_ip);
  if (ip != 0)
    ips.insert(ip);
}

NetInterfaces::NetInterfaces(IPv4List& net_ips)
{
  for (IPv4List::const_iterator i = net_ips.begin(); i != net_ips.end(); ++i)
  {
    if (*i != 0)
      ips.insert(*i);
  }
}

NetInterfaces::NetInterfaces(const QStringList& net_ips)
{
  for (QStringList::const_iterator i = net_ips.begin(); i != net_ips.end(); ++i)
  {
    IPv4 ip = fromString(*i);
    if (ip != 0)
      ips.insert(ip);
  }
}

void NetInterfaces::initSocketList(IPv4UdpSocketList& sockets, QStringList& errors, quint16 port /*= 0*/, QUdpSocket::BindMode mode /*= QUdpSocket::DefaultForPlatform*/)
{
  sockets.clear();
  errors.clear();

  QList<QNetworkInterface> ifaces = QNetworkInterface::allInterfaces();
  if (ifaces.empty())
    return;

  for (QList<QNetworkInterface>::const_iterator iface_iter = ifaces.begin(); iface_iter != ifaces.end(); ++iface_iter)
  {
    const QNetworkInterface& iface = *iface_iter;
    if (!iface.flags().testFlag(QNetworkInterface::IsUp) || !iface.flags().testFlag(QNetworkInterface::IsRunning))
      continue;

    QList<QNetworkAddressEntry> addrs = iface.addressEntries();
    for (QList<QNetworkAddressEntry>::const_iterator addr_iter = addrs.begin(); addr_iter != addrs.end(); ++addr_iter)
    {
      QHostAddress addr = addr_iter->ip();
      if (addr.protocol() != QUdpSocket::IPv4Protocol)
        continue;

      IPv4 ip = addr.toIPv4Address();
      if (ip == 0 || (!ips.empty() && ips.find(ip) == ips.end()))
        continue;

      std::shared_ptr<QUdpSocket> sock = std::make_shared<QUdpSocket>();
      if (!sock->bind((port == 0) ? addr : QHostAddress::AnyIPv4, port, mode))
      {
        errors << QStringLiteral("bind to %1 failed with error %2").arg(port).arg(sock->errorString());
        continue;
      }

      Udp& udp = sockets[ip];
      udp.sock = sock;
      udp.net = iface;
      break;
    }
  }
}

QString NetInterfaces::toString(IPv4 net_ip)
{
  return QHostAddress(net_ip).toString();
}

IPv4 NetInterfaces::fromString(const QString& net_ip)
{
  bool ok = false;
  IPv4 ip = QHostAddress(net_ip).toIPv4Address(&ok);
  return ok ? ip : 0;
}

QString Message::MulticastTransformIP(SystemNumber system)
{
  return QLatin1String("239.159.1.") + QString::number(system);
}

QString Message::MulticastAdvertIP()
{
  return QLatin1String("239.159.2.1");
}

QByteArray Message::ToProtocolString(const QString& str)
{
  const qsizetype kMaxLength = 31;

  QByteArray utf8;
  QString chopped = str.left(kMaxLength);
  for (;;)
  {
    utf8 = chopped.toUtf8();
    if (utf8.size() <= kMaxLength)
      break;

    qsizetype old_size = chopped.size();
    chopped.chop(1);
    if (chopped.size() >= old_size)
    {
      // chop failed, prevent infinite loop
      utf8.clear();
      break;
    }
  }

  // null terminator padding
  qsizetype size = utf8.size();
  utf8.resize(kMaxLength + 1);
  for (; size < utf8.size(); ++size)
    utf8[size] = 0;

  return utf8;
}

QString Message::FromProtocolString(const QByteArray& str)
{
  return QString::fromUtf8(str.constData());
}

Message::Message(const Props& props)
  : props_(props)
{
}

bool Message::FromDatagram(const QByteArray& datagram)
{
  props_ = Props();

  if (!datagram.startsWith(QByteArrayView(kHeader.data(), kHeader.size())))
    return false;  // invalid header

  QDataStream st(datagram);
  st.setByteOrder(QDataStream::BigEndian);
  st.startTransaction();
  st.skipRawData(kHeader.size());

  uint16_t type = 0;
  uint16_t length = 0;
  uint8_t footer_options = 0;
  uint8_t footer_length = 0;
  st >> type >> length >> footer_options >> footer_length;

  QByteArray cid;
  cid.resize(16);
  st.readRawData(cid.data(), cid.size());

  uint8_t options = 0;
  uint32_t reserved = 0;
  st >> props_.folio >> props_.page >> props_.last_page >> options >> reserved;

  QByteArray name;
  name.resize(32);
  st.readRawData(name.data(), name.size());
  if (!st.commitTransaction())
    return false;  // invalid OTP layer

  props_.cid = QUuid::fromRfc4122(cid);
  props_.name = FromProtocolString(name);

  switch (type)
  {
    case kMessageTypeTransform:
    {
      props_.type = Type::kTransform;
      if (!FromDatagramTransform(st))
        return false;
    }
    break;

    case kMessageTypeAdvert:
    {
      props_.type = Type::kAdvert;
      if (!FromDatagramAdvert(st))
        return false;
    }
    break;

    default: return false;
  }

  return true;
}

bool Message::FromDatagramTransform(QDataStream& st)
{
  uint16_t type = 0;
  uint16_t length = 0;
  SystemNumber system_number = 0;
  TimestampNumber timestamp = 0;
  uint8_t options = 0;
  uint32_t reserved = 0;
  st.startTransaction();
  st >> type >> length >> system_number >> timestamp >> options >> reserved;
  if (!st.commitTransaction() || type != kPoint)
    return false;

  System& system = props_.data.systems[system_number];
  system.timestamp = timestamp;

  if (((options >> 7) & 0x1) != 0)
    system.full_point_set = true;

  // read all points
  while (!st.atEnd())
  {
    st.startTransaction();
    PriorityNumber priority = 0;
    GroupNumber group_number = 0;
    PointNumber point_number = 0;
    st >> type >> length;
    qint64 skip_pos = st.device()->pos();
    st >> priority >> group_number >> point_number >> timestamp >> options >> reserved;
    if (!st.commitTransaction())
      return false;

    if (type != kModule)
    {
      st.device()->seek(skip_pos + length);  // skip unknown point layer
      if (st.atEnd())
        break;

      continue;
    }

    Point point;
    point.cid = props_.cid;
    point.name = props_.name;
    point.priority = priority;
    point.timestamp = timestamp;

    // read all modules
    while (!st.atEnd())
    {
      uint16_t manufacturer_id = 0;
      st.startTransaction();
      st >> manufacturer_id >> length;
      skip_pos = st.device()->pos();
      st >> type;
      if (!st.commitTransaction())
        return false;

      if (manufacturer_id != kESTAManufacturerId)
      {
        st.skipRawData(skip_pos + length);
        continue;  // skip non-standard module
      }

      switch (type)
      {
        case kPosModule:
        {
          Vector3 p;
          st.startTransaction();
          st >> options >> p.x >> p.y >> p.z;
          if (!st.commitTransaction())
            return false;

          if ((options >> 7 & 0x1) != 0)
          {
            p.x *= 1000;
            p.y *= 1000;
            p.z *= 1000;
          }

          point.modules.pos = p;
        }
        break;

        case kPosVelAccelModule:
        {
          VelAccel va;
          st.startTransaction();
          st >> va.vel.x >> va.vel.y >> va.vel.z >> va.accel.x >> va.accel.y >> va.accel.z;
          if (!st.commitTransaction())
            return false;

          point.modules.pos_vel_accel = va;
        }
        break;

        case kRotModule:
        {
          Vector3u r;
          st.startTransaction();
          st >> r.x >> r.y >> r.z;
          if (!st.commitTransaction())
            return false;

          point.modules.rot = r;
        }
        break;

        case kRotVelAccelModule:
        {
          VelAccel va;
          st.startTransaction();
          st >> va.vel.x >> va.vel.y >> va.vel.z >> va.accel.x >> va.accel.y >> va.accel.z;
          if (!st.commitTransaction())
            return false;

          point.modules.rot_vel_accel = va;
        }
        break;

        case kScaleModule:
        {
          Vector3 s;
          st.startTransaction();
          st >> s.x >> s.y >> s.z;
          if (!st.commitTransaction())
            return false;

          point.modules.scale = s;
        }
        break;

        case kFrameModule:
        {
          Frame frame;
          st.startTransaction();
          st >> frame.system >> frame.group >> frame.point;
          if (!st.commitTransaction())
            return false;

          point.modules.frame = frame;
        }
        break;

        default:
        {
          st.skipRawData(skip_pos + length);  // skip non-standard module
        }
        break;
      }
    }

    // update point if new, >= priority than exisiting
    std::pair<PointList::iterator, bool> insert_result = system.groups[group_number].points.insert(std::make_pair(point_number, Point()));
    if (insert_result.second || point.priority >= insert_result.first->second.priority)
      insert_result.first->second = point;
  }

  return true;
}

bool Message::FromDatagramAdvert(QDataStream& st)
{
  uint16_t type = 0;
  uint16_t length = 0;
  uint32_t reserved = 0;
  st.startTransaction();
  st >> type >> length >> reserved;
  if (!st.commitTransaction())
    return false;

  switch (type)
  {
    case kAdvertTypeModule:
    {
      props_.advert.type = AdvertType::kModule;

      st.startTransaction();
      st >> type >> length >> reserved;
      if (!st.commitTransaction() || type != kAdvertModuleTypeList)
        return false;

      uint16_t manufacturer_id = 0;
      uint16_t module_number = 0;
      while (!st.atEnd())
      {
        st.startTransaction();
        st >> manufacturer_id >> module_number;
        if (!st.commitTransaction())
          return false;

        if (manufacturer_id != kESTAManufacturerId)
          continue;

        switch (module_number)
        {
          case kPosModule: props_.advert.module_types.insert(ModuleType::kPos); break;
          case kPosVelAccelModule: props_.advert.module_types.insert(ModuleType::kPosVelAccel); break;
          case kRotModule: props_.advert.module_types.insert(ModuleType::kRot); break;
          case kRotVelAccelModule: props_.advert.module_types.insert(ModuleType::kRotVelAccel); break;
          case kScaleModule: props_.advert.module_types.insert(ModuleType::kScale); break;
          case kFrameModule: props_.advert.module_types.insert(ModuleType::kFrame); break;
          default: break;
        }
      }
    }
    break;

    case kAdvertTypeName:
    {
      props_.advert.type = AdvertType::kName;

      uint8_t options = 0;
      st.startTransaction();
      st >> type >> length >> options >> reserved;
      if (!st.commitTransaction() || type != kAdvertModuleNameList)
        return false;

      if (((options >> 7) & 0x1) == 0)
        props_.advert.op = Op::kRequest;
      else
        props_.advert.op = Op::kResponse;

      SystemNumber system_number = 0;
      GroupNumber group_number = 0;
      PointNumber point_number = 0;
      while (!st.atEnd())
      {
        st.startTransaction();
        st >> system_number >> group_number >> point_number;
        QByteArray name;
        name.resize(32);
        st.readRawData(name.data(), name.size());
        if (!st.commitTransaction())
          return false;

        props_.data.systems[system_number].groups[group_number].points[point_number].name = FromProtocolString(name);
      }
    }
    break;

    case kAdvertTypeSystem:
    {
      props_.advert.type = AdvertType::kSystem;

      uint8_t options = 0;
      st.startTransaction();
      st >> type >> length >> options >> reserved;
      if (!st.commitTransaction() || type != kAdvertModuleSystemList)
        return false;

      if (((options >> 7) & 0x1) == 0)
        props_.advert.op = Op::kRequest;
      else
        props_.advert.op = Op::kResponse;

      SystemNumber system_number = 0;
      while (!st.atEnd())
      {
        st.startTransaction();
        st >> system_number;
        if (!st.commitTransaction())
          return false;

        props_.data.systems.insert(std::make_pair(system_number, System()));
      }
    }
    break;

    default: return false;
  }

  return true;
}

bool Message::ToDatagram(QByteArray& datagram) const
{
  QDataStream st(&datagram, QIODevice::WriteOnly | QIODevice::Truncate);
  st.setByteOrder(QDataStream::BigEndian);

  st.writeRawData(kHeader.data(), kHeader.size());
  switch (props_.type)
  {
    case Type::kTransform: st << kMessageTypeTransform; break;
    case Type::kAdvert: st << kMessageTypeAdvert; break;
    default: return false;
  }

  // length placeholder
  uint16_t length = 0;
  uint8_t footer = 0;
  uint8_t footer_length = 0;
  int64_t length_pos = st.device()->pos();
  st << length << footer << footer_length;

  QByteArray raw = props_.cid.toRfc4122();
  st.writeRawData(raw.data(), raw.size());

  uint8_t options = 0;
  uint32_t reserved = 0;
  st << props_.folio << props_.page << props_.last_page << options << reserved;

  raw = ToProtocolString(props_.name);
  st.writeRawData(raw.data(), raw.size());

  switch (props_.type)
  {
    case Type::kTransform:
    {
      if (!ToDatagramTransform(st))
        return false;
    }
    break;

    case Type::kAdvert:
    {
      if (!ToDatagramAdvert(st))
        return false;
    }
    break;

    default: return false;
  }

  // length
  int64_t pos = st.device()->pos();
  st.device()->seek(length_pos);
  length = static_cast<uint16_t>(pos - length_pos - sizeof(length));
  st << length;
  st.device()->seek(pos);

  return true;
}

bool Message::ToDatagramTransform(QDataStream& st) const
{
  const SystemList::const_iterator system_iter = props_.data.systems.begin();
  if (system_iter == props_.data.systems.end())
    return false;

  SystemNumber system_number = system_iter->first;
  const System& system = system_iter->second;

  st << kPoint;

  // length placeholder
  int64_t length_pos = st.device()->pos();
  uint16_t length = 0;
  uint8_t options = 0;
  if (system.full_point_set)
    options = 0x1 << 7;
  uint32_t reserved = 0;
  st << length << system_number << system.timestamp << options << reserved;

  for (GroupList::const_iterator group_iter = system.groups.begin(); group_iter != system.groups.end(); ++group_iter)
  {
    GroupNumber group_number = group_iter->first;
    const Group& group = group_iter->second;

    for (PointList::const_iterator point_iter = group.points.begin(); point_iter != group.points.end(); ++point_iter)
    {
      PointNumber point_number = point_iter->first;
      const Point& point = point_iter->second;
      if (!ToDatagramTransformPoint(st, group_number, point_number, point))
        return false;
    }
  }

  // length
  int64_t pos = st.device()->pos();
  st.device()->seek(length_pos);
  length = static_cast<uint16_t>(pos - length_pos - sizeof(length));
  st << length;
  st.device()->seek(pos);

  return true;
}

bool Message::ToDatagramTransformPoint(QDataStream& st, GroupNumber group_number, PointNumber point_number, const Point& point) const
{
  st << kModule;

  // length placeholder
  int64_t length_pos = st.device()->pos();
  uint16_t length = 0;
  uint8_t options = 0;
  uint32_t reserved = 0;
  st << length << point.priority << group_number << point_number << point.timestamp << options << reserved;

  for (int module_type = 0; module_type < static_cast<int>(ModuleType::kCount); ++module_type)
  {
    if (!ToDatagramTransformModule(st, point.modules, static_cast<ModuleType>(module_type)))
      return false;
  }

  // length
  int64_t pos = st.device()->pos();
  st.device()->seek(length_pos);
  length = static_cast<uint16_t>(pos - length_pos - sizeof(length));
  st << length;
  st.device()->seek(pos);

  return true;
}

bool Message::ToDatagramTransformModule(QDataStream& st, const Modules& modules, ModuleType module_type) const
{
  // length placeholder
  int64_t length_pos = 0;
  uint16_t length = 0;

  switch (module_type)
  {
    case ModuleType::kPos:
    {
      if (!modules.pos.has_value())
        return true;

      uint8_t options = 0;
      st << kESTAManufacturerId;
      length_pos = st.device()->pos();
      st << length << kPosModule << options << modules.pos->x << modules.pos->y << modules.pos->z;
    }
    break;

    case ModuleType::kPosVelAccel:
    {
      if (!modules.pos_vel_accel.has_value())
        return true;

      st << kESTAManufacturerId;
      length_pos = st.device()->pos();
      st << length << kPosVelAccelModule << modules.pos_vel_accel->vel.x << modules.pos_vel_accel->vel.y << modules.pos_vel_accel->vel.z << modules.pos_vel_accel->accel.x
         << modules.pos_vel_accel->accel.y << modules.pos_vel_accel->accel.z;
    }
    break;

    case ModuleType::kRot:
    {
      if (!modules.rot.has_value())
        return true;

      st << kESTAManufacturerId;
      length_pos = st.device()->pos();
      st << length << kRotModule << modules.rot->x << modules.rot->y << modules.rot->z;
    }
    break;

    case ModuleType::kRotVelAccel:
    {
      if (!modules.rot_vel_accel.has_value())
        return true;

      st << kESTAManufacturerId;
      length_pos = st.device()->pos();
      st << length << kRotVelAccelModule << modules.rot_vel_accel->vel.x << modules.rot_vel_accel->vel.y << modules.rot_vel_accel->vel.z << modules.rot_vel_accel->accel.x
         << modules.rot_vel_accel->accel.y << modules.rot_vel_accel->accel.z;
    }
    break;

    case ModuleType::kScale:
    {
      if (!modules.scale.has_value())
        return true;

      st << kESTAManufacturerId;
      length_pos = st.device()->pos();
      st << length << kScaleModule << modules.scale->x << modules.scale->y << modules.scale->z;
    }
    break;

    case ModuleType::kFrame:
    {
      if (!modules.frame.has_value())
        return true;

      st << kESTAManufacturerId;
      length_pos = st.device()->pos();
      st << length << kFrameModule << modules.frame->system << modules.frame->group << modules.frame->point;
    }
    break;

    default: return true;
  }

  // length
  int64_t pos = st.device()->pos();
  st.device()->seek(length_pos);
  length = static_cast<uint16_t>(pos - length_pos - sizeof(length));
  st << length;
  st.device()->seek(pos);

  return true;
}

bool Message::ToDatagramAdvert(QDataStream& st) const
{
  switch (props_.advert.type)
  {
    case AdvertType::kModule: st << kAdvertTypeModule; break;
    case AdvertType::kName: st << kAdvertTypeName; break;
    case AdvertType::kSystem: st << kAdvertTypeSystem; break;
    default: return false;
  }

  // length placeholder
  int64_t length_pos = st.device()->pos();
  uint16_t length = 0;
  uint32_t reserved = 0;
  st << length << reserved;

  switch (props_.advert.type)
  {
    case AdvertType::kModule:
    {
      if (!ToDatagramAdvertModule(st))
        return false;
    }
    break;

    case AdvertType::kName:
    {
      if (!ToDatagramAdvertName(st))
        return false;
    }
    break;

    case AdvertType::kSystem:
    {
      if (!ToDatagramAdvertSystem(st))
        return false;
    }
    break;

    default: return false;
  }

  // length
  int64_t pos = st.device()->pos();
  st.device()->seek(length_pos);
  length = static_cast<uint16_t>(pos - length_pos - sizeof(length));
  st << length;
  st.device()->seek(pos);

  return true;
}

bool Message::ToDatagramAdvertModule(QDataStream& st) const
{
  st << kAdvertModuleTypeList;

  // length placeholder
  int64_t length_pos = st.device()->pos();
  uint16_t length = 0;
  uint32_t reserved = 0;
  st << length << reserved;

  qint64 modules_pos = st.device()->pos();
  for (ModuleTypeList::const_iterator i = props_.advert.module_types.begin(); i != props_.advert.module_types.end(); ++i)
  {
    switch (*i)
    {
      case ModuleType::kPos: st << kESTAManufacturerId << kPosModule; break;
      case ModuleType::kPosVelAccel: st << kESTAManufacturerId << kPosVelAccelModule; break;
      case ModuleType::kRot: st << kESTAManufacturerId << kRotModule; break;
      case ModuleType::kRotVelAccel: st << kESTAManufacturerId << kRotVelAccelModule; break;
      case ModuleType::kScale: st << kESTAManufacturerId << kScaleModule; break;
      case ModuleType::kFrame: st << kESTAManufacturerId << kFrameModule; break;
      default: break;
    }
  }
  if (st.device()->pos() <= modules_pos)
    return false;  // module list may not be empty

  // length
  int64_t pos = st.device()->pos();
  st.device()->seek(length_pos);
  length = static_cast<uint16_t>(pos - length_pos - sizeof(length));
  st << length;
  st.device()->seek(pos);

  return true;
}

bool Message::ToDatagramAdvertName(QDataStream& st) const
{
  st << kAdvertModuleNameList;

  // length placeholder
  int64_t length_pos = st.device()->pos();
  uint16_t length = 0;
  uint8_t options = 0;
  uint32_t reserved = 0;
  st << length << options << reserved;

  for (SystemList::const_iterator system_iter = props_.data.systems.begin(); system_iter != props_.data.systems.end(); ++system_iter)
  {
    SystemNumber system_number = system_iter->first;
    const GroupList& groups = system_iter->second.groups;
    for (GroupList::const_iterator group_iter = groups.begin(); group_iter != groups.end(); ++group_iter)
    {
      GroupNumber group_number = group_iter->first;
      const PointList& points = group_iter->second.points;
      for (PointList::const_iterator point_iter = points.begin(); point_iter != points.end(); ++point_iter)
      {
        PointNumber point_number = point_iter->first;
        const Point& point = point_iter->second;

        st << system_number << group_number << point_number << ToProtocolString(point.name);
      }
    }
  }

  // length
  int64_t pos = st.device()->pos();
  st.device()->seek(length_pos);
  length = static_cast<uint16_t>(pos - length_pos - sizeof(length));
  st << length;
  st.device()->seek(pos);

  return true;
}

bool Message::ToDatagramAdvertSystem(QDataStream& st) const
{
  st << kAdvertModuleSystemList;

  // length placeholder
  int64_t length_pos = st.device()->pos();
  uint16_t length = 0;
  uint8_t options = 0;
  if (props_.advert.op == Op::kResponse)
    options = 0x1 << 7;
  uint32_t reserved = 0;
  st << length << options << reserved;

  for (SystemList::const_iterator i = props_.data.systems.begin(); i != props_.data.systems.end(); ++i)
    st << i->first;

  // length
  int64_t pos = st.device()->pos();
  st.device()->seek(length_pos);
  length = static_cast<uint16_t>(pos - length_pos - sizeof(length));
  st << length;
  st.device()->seek(pos);

  return true;
}

Consumer::Consumer(const QString& name /*= QString()*/, const ModuleTypeList& module_types /*= {}*/, const QUuid& cid /*= QUuid::createUuid()*/)
{
  props_.type = otp::Message::Type::kAdvert;
  props_.advert.type = otp::Message::AdvertType::kModule;
  props_.name = name;
  props_.advert.module_types = module_types;
  props_.cid = cid;
  UpdateAdvertDatagram();
}

Consumer::~Consumer()
{
  SetSystems({});
}

void Consumer::Init(const NetInterfaces& nets)
{
  // store existing systems
  SystemSet old_systems;
  for (SystemList::const_iterator i = systems_.begin(); i != systems_.end(); ++i)
    old_systems.insert(i->first);

  // clear systems
  SetSystems({});

  nets_ = nets;

  // init send sockets
  QStringList errors;
  nets_.initSocketList(send_sockets_, errors);
  for (QStringList::const_iterator i = errors.begin(); i != errors.end(); ++i)
    Log(LogMsg(QtCriticalMsg, *i));

  // init recv sockets
  nets_.initSocketList(recv_sockets_, errors, kPort, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
  for (QStringList::const_iterator i = errors.begin(); i != errors.end(); ++i)
    Log(LogMsg(QtCriticalMsg, *i));

  // restore old systems
  SetSystems(old_systems);
}

void Consumer::SetName(const QString& name)
{
  if (props_.name == name)
    return;

  props_.name = name;
  UpdateAdvertDatagram();
}

void Consumer::SetModuleTypes(const ModuleTypeList& module_types)
{
  if (props_.advert.module_types == module_types)
    return;

  props_.advert.module_types = module_types;
  UpdateAdvertDatagram();
}

void Consumer::SetCID(const QUuid& cid)
{
  if (props_.cid == cid)
    return;

  props_.cid = cid;
  UpdateAdvertDatagram();
}

void Consumer::SetSystems(const SystemSet& systems)
{
  // add new systems
  for (SystemSet::const_iterator new_iter = systems.begin(); new_iter != systems.end(); ++new_iter)
  {
    SystemNumber number = *new_iter;
    if (systems_.find(number) != systems_.end())
      continue;  // system already exists

    // add new system
    systems_[number];

    QString multicast_ip = Message::MulticastTransformIP(number);
    QHostAddress addr(multicast_ip);
    for (IPv4UdpSocketList::const_iterator sock_iter = recv_sockets_.begin(); sock_iter != recv_sockets_.end(); ++sock_iter)
    {
      const Udp& udp = sock_iter->second;
      QString net_ip = NetInterfaces::toString(sock_iter->first);

      if (udp.sock->joinMulticastGroup(addr, udp.net))
        Log(LogMsg(QtInfoMsg, QStringLiteral("interface %1 listening on multicast group %2:%3").arg(net_ip).arg(multicast_ip).arg(kPort)));
      else
        Log(LogMsg(QtCriticalMsg, QStringLiteral("interface %1 listen on multicast group %2 failed with error %3").arg(net_ip).arg(multicast_ip).arg(udp.sock->errorString())));

      udp.sock->setSocketOption(QAbstractSocket::MulticastLoopbackOption, 1);
      udp.sock->setMulticastInterface(udp.net);
    }
  }

  // remove old systems
  for (SystemList::iterator existing_iter = systems_.begin(); existing_iter != systems_.end();)
  {
    SystemNumber number = existing_iter->first;
    if (systems.find(number) != systems.end())
    {
      ++existing_iter;
      continue;  // keep system
    }

    QString multicast_ip = Message::MulticastTransformIP(number);
    QHostAddress addr(multicast_ip);
    for (IPv4UdpSocketList::const_iterator sock_iter = recv_sockets_.begin(); sock_iter != recv_sockets_.end(); ++sock_iter)
    {
      const Udp& udp = sock_iter->second;
      QString net_ip = NetInterfaces::toString(sock_iter->first);

      udp.sock->leaveMulticastGroup(addr, udp.net);
      Log(LogMsg(QtInfoMsg, QStringLiteral("interface %1 leaving multicast group %2:%3").arg(net_ip).arg(multicast_ip).arg(kPort)));
    }

    systems_.erase(existing_iter++);
  }
}

void Consumer::Tick()
{
  TickAdvert();
  TickProducers();
  TickTransform();
}

void Consumer::TickAdvert()
{
  if (send_sockets_.empty() || systems_.empty() || advert_.datagram.isEmpty() || (advert_.timer.isValid() && advert_.timer.elapsed() < kAdvertRate))
    return;

  QString multicast_ip = Message::MulticastAdvertIP();
  QHostAddress addr(multicast_ip);
  for (IPv4UdpSocketList::const_iterator sock_iter = send_sockets_.begin(); sock_iter != send_sockets_.end(); ++sock_iter)
  {
    const Udp& udp = sock_iter->second;

    if (udp.sock->writeDatagram(advert_.datagram, addr, kPort) < 0)
    {
      Log(LogMsg(QtCriticalMsg,
                 QStringLiteral("interface %1 send advert to %2:%3 failed with error %4").arg(NetInterfaces::toString(sock_iter->first)).arg(multicast_ip).arg(kPort).arg(udp.sock->errorString())));
    }
  }

  advert_.timer.start();
}

void Consumer::UpdateAdvertDatagram()
{
  if (!otp::Message(props_).ToDatagram(advert_.datagram))
    advert_.datagram.clear();
}

void Consumer::TickProducers()
{
  for (EndpointList::iterator prod_iter = producers_.begin(); prod_iter != producers_.end();)
  {
    Endpoint& producer = prod_iter->second;

    if (producer.isExpired())
    {
      QUuid old_cid = prod_iter->first;
      Endpoint old_producer = producer;

      producers_.erase(prod_iter++);

      if (callbacks_)
        callbacks_->ConsumerCallback_ProducerChanged(EndpointChange::kExpired, old_cid, old_producer);
    }
    else
      ++prod_iter;
  }
}

void Consumer::TickTransform()
{
  for (IPv4UdpSocketList::const_iterator sock_iter = recv_sockets_.begin(); sock_iter != recv_sockets_.end(); ++sock_iter)
  {
    const Udp& udp = sock_iter->second;

    for (;;)
    {
      if (!udp.sock->waitForReadyRead(0))
        break;

      qint64 size = udp.sock->pendingDatagramSize();
      if (size < 1)
        break;

      reusable_.resize(size);
      QHostAddress addr;
      qint64 size_read = udp.sock->readDatagram(reusable_.data(), reusable_.size(), &addr);
      if (size_read < 1)
        break;

      reusable_.resize(size_read);
      Message message;
      if (message.FromDatagram(reusable_))
      {
        AddProducer(message, addr);
        MergeTransform(message);
      }
    }
  }
}

void Consumer::AddProducer(const Message& message, const QHostAddress& addr)
{
  const Message::Props& props = message.GetProps();

  std::pair<EndpointList::iterator, bool> result = producers_.insert(std::make_pair(props.cid, Endpoint()));

  Endpoint& producer = result.first->second;
  producer.timepoint = std::chrono::steady_clock::now();

  if (!result.second && producer.name == props.name && producer.addr == addr)
    return;  // no change

  producer.name = props.name;
  producer.addr = addr;

  if (callbacks_)
    callbacks_->ConsumerCallback_ProducerChanged(result.second ? EndpointChange::kAppeared : EndpointChange::kUpdated, props.cid, producer);
}

void Consumer::MergeTransform(const Message& message)
{
  const Message::Props& props = message.GetProps();
  if (props.type != Message::Type::kTransform)
    return;

  const SystemList& new_systems = props.data.systems;
  for (SystemList::const_iterator new_system_iter = new_systems.begin(); new_system_iter != new_systems.end(); ++new_system_iter)
  {
    Frame frame;
    frame.system = new_system_iter->first;

    SystemList::iterator existing_system_iter = systems_.find(frame.system);
    if (existing_system_iter == systems_.end())
      continue;  // no such system

    System& existing_system = existing_system_iter->second;
    GroupList& existing_groups = existing_system.groups;
    const System& new_system = new_system_iter->second;
    const GroupList& new_groups = new_system.groups;
    for (GroupList::const_iterator new_group_iter = new_groups.begin(); new_group_iter != new_groups.end(); ++new_group_iter)
    {
      frame.group = new_group_iter->first;
      const Group& new_group = new_group_iter->second;

      GroupList::iterator existing_group_iter = existing_groups.find(frame.group);
      if (existing_group_iter == existing_groups.end())
      {
        // add new group
        existing_groups[frame.group] = new_group;
        existing_system.timestamp = new_system.timestamp;

        if (callbacks_)
        {
          const PointList& points = new_group.points;
          for (PointList::const_iterator point_iter = points.begin(); point_iter != points.end(); ++point_iter)
          {
            frame.point = point_iter->first;
            callbacks_->ConsumerCallback_PointChanged(PointChange::kGroupAppeared, frame, point_iter->second);
          }
        }
      }
      else
      {
        // udpate existing group
        Group& existing_group = existing_group_iter->second;
        PointList& existing_points = existing_group.points;
        const PointList& new_points = new_group.points;
        for (PointList::const_iterator new_point_iter = new_points.begin(); new_point_iter != new_points.end(); ++new_point_iter)
        {
          frame.point = new_point_iter->first;
          const Point& new_point = new_point_iter->second;

          PointList::iterator existing_point_iter = existing_points.find(frame.point);
          if (existing_point_iter == existing_points.end())
          {
            // add new point
            existing_points[frame.point] = new_point;
            existing_system.timestamp = new_system.timestamp;

            if (callbacks_)
              callbacks_->ConsumerCallback_PointChanged(PointChange::kPointAppeared, frame, new_point);
          }
          else
          {
            PointChange change = PointChange::kNone;
            Point& existing_point = existing_point_iter->second;
            if (new_point.priority >= existing_point.priority || existing_point.isExpired())
            {
              // udpate point
              if (existing_point != new_point)
              {
                existing_point = new_point;
                change = PointChange::kPointUpdated;
              }

              existing_system.timestamp = new_system.timestamp;
            }

            if (callbacks_)
              callbacks_->ConsumerCallback_PointChanged(change, frame, new_point);
          }
        }
      }
    }
  }
}

void Consumer::Log(const LogMsg& msg)
{
  if (callbacks_)
    callbacks_->ConsumerCallback_Log(msg);
}

Producer::Producer(const QString& name /*= QString()*/, const QUuid& cid /*= QUuid::createUuid()*/)
{
  props_.name = name;
  props_.cid = cid;
}

Producer::~Producer()
{
  Leave();
}

void Producer::Leave()
{
  QString multicast_ip = Message::MulticastAdvertIP();
  QHostAddress addr(multicast_ip);
  for (IPv4UdpSocketList::const_iterator i = recv_sockets_.begin(); i != recv_sockets_.end(); ++i)
  {
    const Udp& udp = i->second;
    QString net_ip = NetInterfaces::toString(i->first);

    udp.sock->leaveMulticastGroup(addr, udp.net);
    Log(LogMsg(QtInfoMsg, QStringLiteral("interface %1 leaving multicast group %2:%3").arg(net_ip).arg(multicast_ip).arg(kPort)));
  }
}

void Producer::Init(const NetInterfaces& nets)
{
  Leave();

  nets_ = nets;

  // init send sockets
  QStringList errors;
  nets_.initSocketList(send_sockets_, errors);
  for (QStringList::const_iterator i = errors.begin(); i != errors.end(); ++i)
    Log(LogMsg(QtCriticalMsg, *i));

  // init recv sockets
  nets_.initSocketList(recv_sockets_, errors, kPort, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
  for (QStringList::const_iterator i = errors.begin(); i != errors.end(); ++i)
    Log(LogMsg(QtCriticalMsg, *i));

  QString multicast_ip = Message::MulticastAdvertIP();
  QHostAddress addr(multicast_ip);
  for (IPv4UdpSocketList::const_iterator i = recv_sockets_.begin(); i != recv_sockets_.end(); ++i)
  {
    const Udp& udp = i->second;
    QString net_ip = NetInterfaces::toString(i->first);

    if (udp.sock->joinMulticastGroup(addr, udp.net))
      Log(LogMsg(QtInfoMsg, QStringLiteral("interface %1 listening on multicast group %2:%3").arg(net_ip).arg(multicast_ip).arg(kPort)));
    else
      Log(LogMsg(QtCriticalMsg, QStringLiteral("interface %1 listen on multicast group %2 failed with error %3").arg(net_ip).arg(multicast_ip).arg(udp.sock->errorString())));

    udp.sock->setSocketOption(QAbstractSocket::MulticastLoopbackOption, 1);
    udp.sock->setMulticastInterface(udp.net);
  }
}

PointChange Producer::SetPoint(const Frame& frame, const Point& point)
{
  PointChange change = PointChange::kNone;
  SystemList& systems = props_.data.systems;
  SystemList::iterator system_iter = systems.find(frame.system);
  if (system_iter == systems.end())
  {
    systems[frame.system].groups[frame.group].points[frame.point] = point;
    change = PointChange::kSystemAppeared;
  }
  else
  {
    GroupList& groups = system_iter->second.groups;
    GroupList::iterator group_iter = groups.find(frame.group);
    if (group_iter == groups.end())
    {
      groups[frame.group].points[frame.point] = point;
      change = PointChange::kGroupAppeared;
    }
    else
    {
      PointList& points = group_iter->second.points;
      PointList::iterator point_iter = points.find(frame.point);
      if (point_iter == points.end())
      {
        points[frame.point] = point;
        change = PointChange::kPointAppeared;
      }
      else
      {
        Point& existing_point = point_iter->second;
        if (existing_point != point)
        {
          existing_point = point;
          change = PointChange::kPointUpdated;
        }
      }
    }
  }

  if (change != PointChange::kNone)
    SendPoint(frame, point);

  return change;
}

bool Producer::RemovePoint(const Frame& frame)
{
  SystemList& systems = props_.data.systems;
  SystemList::iterator system_iter = systems.find(frame.system);
  if (system_iter == systems.end())
    return false;  // no such system

  GroupList& groups = system_iter->second.groups;
  GroupList::iterator group_iter = groups.find(frame.group);
  if (group_iter == groups.end())
    return false;  // no such group

  PointList& points = group_iter->second.points;
  PointList::iterator point_iter = points.find(frame.point);
  if (point_iter == points.end())
    return false;  // no such point

  points.erase(point_iter);

  if (points.empty())
  {
    groups.erase(group_iter);  // no more points, remove entire group

    if (groups.empty())
      systems.erase(system_iter);  // no more groups, remove entire system
  }

  return true;
}

bool Producer::RemoveAllPoints()
{
  SystemList& systems = props_.data.systems;
  if (systems.empty())
    return false;  // no systems

  systems.clear();
  return true;
}

void Producer::Tick()
{
  TickAdvert();
  TickConsumers();
  TickResend();
}

void Producer::TickAdvert()
{
  for (IPv4UdpSocketList::const_iterator i = recv_sockets_.begin(); i != recv_sockets_.end(); ++i)
  {
    const Udp& udp = i->second;

    for (;;)
    {
      if (!udp.sock->waitForReadyRead(0))
        break;

      qint64 size = udp.sock->pendingDatagramSize();
      if (size < 1)
        break;

      reusable_.resize(size);
      QHostAddress addr;
      qint64 size_read = udp.sock->readDatagram(reusable_.data(), reusable_.size(), &addr);
      if (size_read < 1)
        break;

      reusable_.resize(size_read);
      Message request;
      if (request.FromDatagram(reusable_))
        HandleAdvert(request, addr);
    }
  }
}

void Producer::HandleAdvert(const Message& request, const QHostAddress& addr)
{
  const Message::Props& request_props = request.GetProps();
  if (request_props.type != Message::Type::kAdvert)
    return;

  switch (request_props.advert.type)
  {
    case Message::AdvertType::kModule:
    {
      std::pair<EndpointList::iterator, bool> result = consumers_.insert(std::make_pair(request_props.cid, Endpoint()));

      Endpoint& consumer = result.first->second;
      consumer.timepoint = std::chrono::steady_clock::now();

      bool mods_dirty = ((result.second && !request_props.advert.module_types.empty()) || consumer.module_types != request_props.advert.module_types);

      if (result.second || mods_dirty || consumer.name != request_props.name || consumer.addr != addr)
      {
        consumer.name = request_props.name;
        consumer.addr = addr;
        consumer.module_types = request_props.advert.module_types;

        if (callbacks_)
          callbacks_->ProducerCallback_ConsumerChanged(result.second ? EndpointChange::kAppeared : EndpointChange::kUpdated, request_props.cid, consumer);

        if (mods_dirty)
          UpdateModulesTypes();
      }
    }
    break;

    case Message::AdvertType::kName:
    {
      if (request_props.advert.op != Message::Op::kRequest)
        return;

      Message::Props response;
      response.type = Message::Type::kAdvert;
      response.cid = props_.cid;
      response.name = props_.name;
      response.advert.type = Message::AdvertType::kName;
      response.advert.op = Message::Op::kResponse;
      response.data.systems = props_.data.systems;
      if (Message(response).ToDatagram(reusable_))
        Send(reusable_, addr);
    }
    break;

    case Message::AdvertType::kSystem:
    {
      if (request_props.advert.op != Message::Op::kRequest || props_.data.systems.empty())
        return;

      Message::Props response;
      response.type = Message::Type::kAdvert;
      response.cid = props_.cid;
      response.name = props_.name;
      response.advert.type = Message::AdvertType::kSystem;
      response.advert.op = Message::Op::kResponse;
      response.data.systems = props_.data.systems;
      if (Message(response).ToDatagram(reusable_))
        Send(reusable_, addr);
    }
    break;

    default: break;
  }
}

void Producer::TickConsumers()
{
  bool mods_dirty = false;

  for (EndpointList::iterator consumer_iter = consumers_.begin(); consumer_iter != consumers_.end();)
  {
    Endpoint& consumer = consumer_iter->second;

    if (consumer.isExpired())
    {
      QUuid old_cid = consumer_iter->first;
      Endpoint old_consumer = consumer;

      consumers_.erase(consumer_iter++);

      if (!old_consumer.module_types.empty())
        mods_dirty = true;

      if (callbacks_)
        callbacks_->ProducerCallback_ConsumerChanged(EndpointChange::kExpired, old_cid, old_consumer);
    }
    else
      ++consumer_iter;
  }

  if (mods_dirty)
    UpdateModulesTypes();
}

void Producer::TickResend()
{
  if (mods_.empty() || (resend_.isValid() && resend_.elapsed() < kTransformRate))
    return;

  SendPoints(props_.data, /*full_point_set*/ true);
  resend_.start();
}

void Producer::Log(const LogMsg& msg)
{
  if (callbacks_)
    callbacks_->ProducerCallback_Log(msg);
}

void Producer::UpdateModulesTypes()
{
  ModuleTypeList new_mods;
  for (EndpointList::const_iterator consumer_iter = consumers_.begin(); consumer_iter != consumers_.end(); ++consumer_iter)
  {
    const Endpoint& consumer = consumer_iter->second;
    new_mods.insert(consumer.module_types.begin(), consumer.module_types.end());
  }

  if (mods_ == new_mods)
    return;

  mods_ = new_mods;
}

void Producer::SendPoint(const Frame& frame, const Point& point)
{
  if (mods_.empty())
    return;

  Data data;
  data.systems[frame.system].groups[frame.group].points[frame.point] = point;
  SendPoints(data, /*full_point_set*/ false);
}

void Producer::SendPoints(const Data& data, bool full_point_set)
{
  if (mods_.empty() || data.systems.empty())
    return;

  Message::Props props;
  props.type = Message::Type::kTransform;
  props.cid = props_.cid;
  props.name = props_.name;

  for (SystemList::const_iterator sys_iter = data.systems.begin(); sys_iter != data.systems.end(); ++sys_iter)
  {
    SystemNumber system_number = sys_iter->first;

    props.data.systems.clear();
    System& system = props.data.systems[system_number];
    system = sys_iter->second;

    // update points modules based on mods_
    for (GroupList::iterator group_iter = system.groups.begin(); group_iter != system.groups.end();)
    {
      PointList& points = group_iter->second.points;

      for (PointList::iterator point_iter = points.begin(); point_iter != points.end();)
      {
        Point& point = point_iter->second;

        point.modules.applyFilter(mods_);

        if (point.modules.empty())
          points.erase(point_iter++);
        else
          ++point_iter;
      }

      if (points.empty())
        system.groups.erase(group_iter++);
      else
        ++group_iter;
    }

    if (system.groups.empty())
      continue;

    system.full_point_set = full_point_set;

    if (Message(props).ToDatagram(reusable_))
      Send(reusable_, QHostAddress(Message::MulticastTransformIP(system_number)));
  }
}

void Producer::Send(const QByteArray& datagram, const QHostAddress& addr)
{
  for (IPv4UdpSocketList::const_iterator i = recv_sockets_.begin(); i != recv_sockets_.end(); ++i)
  {
    const Udp& udp = i->second;

    if (udp.sock->writeDatagram(datagram, addr, kPort) < 0)
      Log(LogMsg(QtCriticalMsg, QStringLiteral("interface %1 send to %2:%3 failed with error %4").arg(NetInterfaces::toString(i->first)).arg(addr.toString()).arg(kPort).arg(udp.sock->errorString())));
  }
}
}  // namespace otp
