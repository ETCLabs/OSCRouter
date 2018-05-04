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
	if(other.m_Data && other.m_Size>0)
	{
		m_Size = other.m_Size;
		m_Data = new char[m_Size];
		memcpy(m_Data, other.m_Data, m_Size);	// TODO: optmize, too many needless copies of packet data in queues
	}
}

////////////////////////////////////////////////////////////////////////////////

EosPacket::EosPacket(const char *data, int size)
	: m_Data(0)
	, m_Size(0)
{
	if(data && size>0)
	{
		m_Size = size;
		m_Data = new char[m_Size];
		memcpy(m_Data, data, m_Size);
	}
}

////////////////////////////////////////////////////////////////////////////////

EosPacket& EosPacket::operator=(const EosPacket &other)
{
	if(&other != this)
	{
		if( m_Data )
		{
			delete[] m_Data;
			m_Data = 0;
		}

		m_Size = 0;

		if(other.m_Data && other.m_Size>0)
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
	if( m_Data )
	{
		delete[] m_Data;
		m_Data = 0;
	}
}

////////////////////////////////////////////////////////////////////////////////

EosAddr::EosAddr(const QString &Ip, unsigned short Port)
	: ip( Ip.toLower().trimmed() )
	, port(Port)
{
}

////////////////////////////////////////////////////////////////////////////////

bool EosAddr::operator==(const EosAddr &other) const
{
	return (ip==other.ip && port==other.port);
}

////////////////////////////////////////////////////////////////////////////////

bool EosAddr::operator!=(const EosAddr &other) const
{
	return (ip!=other.ip || port!=other.port);
}

////////////////////////////////////////////////////////////////////////////////

bool EosAddr::operator<(const EosAddr &other) const
{
	if(ip == other.ip)
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
	return static_cast<unsigned int>( QHostAddress(ip).toIPv4Address() );
}

////////////////////////////////////////////////////////////////////////////////

void EosAddr::UIntToIP(unsigned int n, QString &ip)
{
	ip = QHostAddress( static_cast<quint32>(n) ).toString();
}

////////////////////////////////////////////////////////////////////////////////

EosRouteSrc::EosRouteSrc(const EosAddr &Addr, const QString &Path)
	: addr(Addr)
	, path(Path)
{
}

////////////////////////////////////////////////////////////////////////////////

bool EosRouteSrc::operator==(const EosRouteSrc &other) const
{
	return (addr==other.addr && path==other.path);
}

////////////////////////////////////////////////////////////////////////////////

bool EosRouteSrc::operator!=(const EosRouteSrc &other) const
{
	return (addr!=other.addr || path!=other.path);
}

////////////////////////////////////////////////////////////////////////////////

bool EosRouteSrc::operator<(const EosRouteSrc &other) const
{
	if(addr == other.addr)
		return (addr < other.addr);
	return (path < other.path);
}

////////////////////////////////////////////////////////////////////////////////

bool EosRouteDst::operator==(const EosRouteDst &other) const
{
	return (	addr==other.addr &&
				path==other.path &&
				inMin==other.inMin &&
				inMax==other.inMax &&
				outMin==other.outMin &&
				outMax==other.outMax );
}

////////////////////////////////////////////////////////////////////////////////

bool EosRouteDst::operator<(const EosRouteDst &other) const
{
	if(addr != other.addr)
		return (addr < other.addr);
	if(path != other.path)
		return (path < other.path);
	if(inMin != other.inMin)
		return (inMin < other.inMin);
	if(inMax != other.inMax)
		return (inMax < other.inMax);
	if(outMin != other.outMin)
		return (outMin < other.outMin);
	return (outMax < other.outMax);
}

////////////////////////////////////////////////////////////////////////////////
