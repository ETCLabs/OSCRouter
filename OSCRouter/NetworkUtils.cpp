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

#include "NetworkUtils.h"
#include "otp/otp.h"

// must be last include
#include "LeakWatcher.h"

////////////////////////////////////////////////////////////////////////////////

EosPacket::EosPacket()
  : m_Data(0)
  , m_Size(0)
{
}

////////////////////////////////////////////////////////////////////////////////

EosPacket::EosPacket(const EosPacket &other)
  : m_Data(0)
  , m_Size(0)
{
  if (other.m_Data && other.m_Size > 0)
  {
    m_Size = other.m_Size;
    m_Data = new char[m_Size];
    memcpy(m_Data, other.m_Data, m_Size);  // TODO: optmize, too many needless copies of packet data in queues
  }
}

////////////////////////////////////////////////////////////////////////////////

EosPacket::EosPacket(const char *data, int size)
  : m_Data(0)
  , m_Size(0)
{
  if (data && size > 0)
  {
    m_Size = size;
    m_Data = new char[m_Size];
    memcpy(m_Data, data, m_Size);
  }
}

////////////////////////////////////////////////////////////////////////////////

EosPacket &EosPacket::operator=(const EosPacket &other)
{
  if (&other != this)
  {
    if (m_Data)
    {
      delete[] m_Data;
      m_Data = 0;
    }

    m_Size = 0;

    if (other.m_Data && other.m_Size > 0)
    {
      m_Size = other.m_Size;
      m_Data = new char[m_Size];
      memcpy(m_Data, other.m_Data, m_Size);
    }
  }

  return (*this);
}

////////////////////////////////////////////////////////////////////////////////

EosPacket::~EosPacket()
{
  if (m_Data)
  {
    delete[] m_Data;
    m_Data = 0;
  }
}

////////////////////////////////////////////////////////////////////////////////

EosAddr::EosAddr(const QString &Ip, unsigned short Port)
  : ip(Ip.toLower().trimmed())
  , port(Port)
{
}

////////////////////////////////////////////////////////////////////////////////

bool EosAddr::operator==(const EosAddr &other) const
{
  return (ip == other.ip && port == other.port);
}

////////////////////////////////////////////////////////////////////////////////

bool EosAddr::operator!=(const EosAddr &other) const
{
  return (ip != other.ip || port != other.port);
}

////////////////////////////////////////////////////////////////////////////////

bool EosAddr::operator<(const EosAddr &other) const
{
  if (ip == other.ip)
    return (port < other.port);
  return (ip < other.ip);
}

////////////////////////////////////////////////////////////////////////////////

unsigned int EosAddr::toUInt() const
{
  return IPToUInt(ip);
}

////////////////////////////////////////////////////////////////////////////////

void EosAddr::fromUInt(unsigned int n)
{
  UIntToIP(n, ip);
}

////////////////////////////////////////////////////////////////////////////////

unsigned int EosAddr::IPToUInt(const QString &ip)
{
  return static_cast<unsigned int>(QHostAddress(ip).toIPv4Address());
}

////////////////////////////////////////////////////////////////////////////////

void EosAddr::UIntToIP(unsigned int n, QString &ip)
{
  ip = QHostAddress(static_cast<quint32>(n)).toString();
}

////////////////////////////////////////////////////////////////////////////////

bool ValidPort(Protocol protocol, unsigned short port)
{
  switch (protocol)
  {
    case Protocol::kArtNet:
    case Protocol::kMIDI: return true;

    case Protocol::kOTP: return (port >= otp::kMinSystemNumber && port <= otp::kMaxSystemNumber);
  }

  return (port != 0);
}

////////////////////////////////////////////////////////////////////////////////

unsigned char MSCCmdValue(MSCCmd cmd)
{
  switch (cmd)
  {
    case MSCCmd::kGo: return 0x01u;
    case MSCCmd::kPause: return 0x02u;
    case MSCCmd::kResume: return 0x03u;
    case MSCCmd::kTimedGo: return 0x04u;
    case MSCCmd::kSet: return 0x05u;
    case MSCCmd::kFader: return 0x06u;
    case MSCCmd::kMacro: return 0x07u;
    case MSCCmd::kOff: return 0x0au;
  }

  return static_cast<unsigned char>(cmd);
}

MSCCmd ValueMSCCmd(unsigned char value)
{
  switch (value)
  {
    case 0x01u: return MSCCmd::kGo;
    case 0x02u: return MSCCmd::kPause;
    case 0x03u: return MSCCmd::kResume;
    case 0x04u: return MSCCmd::kTimedGo;
    case 0x05u: return MSCCmd::kSet;
    case 0x06u: return MSCCmd::kFader;
    case 0x07u: return MSCCmd::kMacro;
    case 0x0au: return MSCCmd::kOff;
  }

  return MSCCmd::kCount;
}

QString MSCCmdName(MSCCmd cmd)
{
  switch (cmd)
  {
    case MSCCmd::kGo: return QLatin1String("go");
    case MSCCmd::kPause: return QLatin1String("pause");
    case MSCCmd::kResume: return QLatin1String("resume");
    case MSCCmd::kTimedGo: return QLatin1String("timedGo");
    case MSCCmd::kSet: return QLatin1String("set");
    case MSCCmd::kFader: return QLatin1String("fader");
    case MSCCmd::kMacro: return QLatin1String("macro");
    case MSCCmd::kOff: return QLatin1String("off");
  }

  return QString::number(static_cast<int>(cmd));
}

bool MSCCmdStrings(MSCCmd cmd)
{
  switch (cmd)
  {
    case MSCCmd::kGo:
    case MSCCmd::kPause:
    case MSCCmd::kResume:
    case MSCCmd::kTimedGo: return true;

    default: break;
  }

  return false;
}

std::optional<MSCCmd> MSCCmdForName(const QString &name)
{
  if (name.isEmpty())
    return std::nullopt;

  for (int i = 0; i < static_cast<int>(MSCCmd::kCount); ++i)
  {
    if (name.compare(MSCCmdName(static_cast<MSCCmd>(i)), Qt::CaseInsensitive) == 0)
      return static_cast<MSCCmd>(i);
  }

  return std::nullopt;
}

////////////////////////////////////////////////////////////////////////////////

EosRouteSrc::EosRouteSrc(const EosAddr &Addr, Protocol Protocol, const QString &Path)
  : addr(Addr)
  , protocol(Protocol)
  , path(Path)
{
}

////////////////////////////////////////////////////////////////////////////////

bool EosRouteSrc::operator==(const EosRouteSrc &other) const
{
  return (addr == other.addr && multicastIP == other.multicastIP && protocol == other.protocol && path == other.path);
}

////////////////////////////////////////////////////////////////////////////////

bool EosRouteSrc::operator!=(const EosRouteSrc &other) const
{
  return (addr != other.addr || multicastIP != other.multicastIP || protocol != other.protocol || path != other.path);
}

////////////////////////////////////////////////////////////////////////////////

bool EosRouteSrc::operator<(const EosRouteSrc &other) const
{
  if (addr != other.addr)
    return (addr < other.addr);
  if (multicastIP != other.multicastIP)
    return (multicastIP < other.multicastIP);
  if (protocol != other.protocol)
    return (protocol < other.protocol);
  return (path < other.path);
}

////////////////////////////////////////////////////////////////////////////////

bool EosRouteDst::operator==(const EosRouteDst &other) const
{
  return (addr == other.addr && protocol == other.protocol && path == other.path && script == other.script && scriptText == other.scriptText && inMin == other.inMin && inMax == other.inMax &&
          outMin == other.outMin && outMax == other.outMax);
}

////////////////////////////////////////////////////////////////////////////////
