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
#ifndef NETWORK_UTILS_H
#define NETWORK_UTILS_H

#ifndef QT_INCLUDE_H
#include "QtInclude.h"
#endif

#include <vector>
#include <optional>

////////////////////////////////////////////////////////////////////////////////

class EosPacket
{
public:
  typedef std::vector<EosPacket> Q;

  EosPacket();
  EosPacket(const EosPacket &other);
  EosPacket(const char *data, int size);
  EosPacket &operator=(const EosPacket &other);
  virtual ~EosPacket();
  char *GetData() { return m_Data; }
  const char *GetDataConst() const { return m_Data; }
  int GetSize() const { return m_Size; }
  void Release()
  {
    m_Data = 0;
    m_Size = 0;
  }

private:
  char *m_Data;
  int m_Size;
};

////////////////////////////////////////////////////////////////////////////////

struct EosAddr
{
  EosAddr()
    : port(0)
  {
  }
  EosAddr(const QString &Ip, unsigned short Port);
  bool operator==(const EosAddr &other) const;
  bool operator!=(const EosAddr &other) const;
  bool operator<(const EosAddr &other) const;
  unsigned int toUInt() const;
  void fromUInt(unsigned int n);

  static unsigned int IPToUInt(const QString &ip);
  static void UIntToIP(unsigned int n, QString &ip);

  QString ip;
  unsigned short port;
};

////////////////////////////////////////////////////////////////////////////////

enum class Protocol
{
  kOSC = 0,
  kPSN,
  ksACN,
  kArtNet,
  kMIDI,
  kOTP,

  kCount,
  kDefault = kOSC,
  kInvalid = 0xffff
};

bool ValidPort(Protocol protocol, unsigned short port);

////////////////////////////////////////////////////////////////////////////////

enum class MSCCmd
{
  kGo = 0,
  kPause,
  kResume,
  kTimedGo,
  kSet,
  kFader,
  kMacro,
  kOff,

  kCount
};

enum class MSC
{
  kSysEx = 0xf0,
  kSysExStart = 0x7fu,
  kSysExEnd = 0xf7u,
  kMSC = 0x2u,
  kLightingFormat = 0x01u
};

unsigned char MSCCmdValue(MSCCmd cmd);
MSCCmd ValueMSCCmd(unsigned char value);
QString MSCCmdName(MSCCmd cmd);
bool MSCCmdStrings(MSCCmd cmd);
std::optional<MSCCmd> MSCCmdForName(const QString &name);

////////////////////////////////////////////////////////////////////////////////

struct EosRouteSrc
{
  EosRouteSrc() {}
  EosRouteSrc(const EosAddr &Addr, Protocol Protocol, const QString &Path);
  bool operator==(const EosRouteSrc &other) const;
  bool operator!=(const EosRouteSrc &other) const;
  bool operator<(const EosRouteSrc &other) const;
  EosAddr addr;
  QString multicastIP;
  Protocol protocol = Protocol::kDefault;
  QString path;
};

////////////////////////////////////////////////////////////////////////////////

struct EosRouteDst
{
  struct sTransform
  {
    sTransform()
      : enabled(false)
      , value(0)
    {
    }
    bool operator==(const sTransform &other) const { return (enabled == other.enabled && value == other.value); }
    bool operator!=(const sTransform &other) const { return (enabled != other.enabled || value != other.value); }
    bool operator<(const sTransform &other) const { return ((enabled == other.enabled) ? (value < other.value) : (enabled < other.enabled)); }
    bool enabled;
    float value;
  };

  bool hasAnyTransforms() const { return (inMin.enabled || inMax.enabled || outMin.enabled || outMax.enabled); }
  bool operator==(const EosRouteDst &other) const;
  bool operator!=(const EosRouteDst &other) const { return !((*this) == other); }

  EosAddr addr;
  Protocol protocol = Protocol::kDefault;
  QString path;
  bool script = false;
  QString scriptText;
  sTransform inMin;
  sTransform inMax;
  sTransform outMin;
  sTransform outMax;
};

////////////////////////////////////////////////////////////////////////////////

#endif
