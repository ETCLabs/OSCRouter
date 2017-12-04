// Copyright (c) 2015 Electronic Theatre Controls, Inc., http://www.etcconnect.com
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

#ifdef WIN32
	#include <WinSock2.h>
#else
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
#endif

// must be last include
#include "LeakWatcher.h"

////////////////////////////////////////////////////////////////////////////////

#define EPSILLON	0.00001f

////////////////////////////////////////////////////////////////////////////////

void PacketLogger::OSCParserClient_Log(const std::string &message)
{
	m_LogMsg = (m_Prefix + message);
	m_pLog->Add(m_LogType, m_LogMsg);
}

////////////////////////////////////////////////////////////////////////////////

EosUdpInThread::EosUdpInThread()
	: m_Run(false)
	, m_Mutex(QMutex::Recursive)
	, m_ItemStateTableId(ItemStateTable::sm_Invalid_Id)
	, m_State(ItemState::STATE_UNINITIALIZED)
	, m_ReconnectDelay(0)
{
}

////////////////////////////////////////////////////////////////////////////////

EosUdpInThread::~EosUdpInThread()
{
	Stop();
}

////////////////////////////////////////////////////////////////////////////////

void EosUdpInThread::Start(const EosAddr &addr, ItemStateTable::ID itemStateTableId, unsigned int reconnectDelayMS)
{
	Stop();

	m_Addr = addr;
	m_ItemStateTableId = itemStateTableId;
	m_ReconnectDelay = reconnectDelayMS;
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

void EosUdpInThread::run()
{
	QString msg = QString("udp input %1:%2 thread started").arg(m_Addr.ip).arg(m_Addr.port);
	m_PrivateLog.AddInfo( msg.toUtf8().constData() );
	UpdateLog();

	EosTimer reconnectTimer;

	// outer loop for auto-reconnect
	while( m_Run )
	{
		SetState(ItemState::STATE_CONNECTING);

		EosUdpIn *udpIn = EosUdpIn::Create();
		if( udpIn->Initialize(m_PrivateLog,m_Addr.ip.toUtf8().constData(),m_Addr.port) )
		{
			SetState(ItemState::STATE_CONNECTED);

			OSCParser logParser;
			logParser.SetRoot(new OSCMethod());
			PacketLogger packetLogger(EosLog::LOG_MSG_TYPE_RECV, m_PrivateLog);
			std::string logPrefix;
			sockaddr_in addr;

			// run
			while( m_Run )
			{
				int len = 0;
				int addrSize = static_cast<int>( sizeof(addr) );
				const char *data = udpIn->RecvPacket(m_PrivateLog, 100, 0, len, &addr, &addrSize);
				if(data && len>0)
				{
					QHostAddress host( reinterpret_cast<const sockaddr*>(&addr) );
					logPrefix = QString("IN  [%1:%2] ").arg( host.toString() ).arg(m_Addr.port).toUtf8().constData();
					packetLogger.SetPrefix(logPrefix);
					logParser.PrintPacket(packetLogger, data, static_cast<size_t>(len));
					unsigned int ip = static_cast<unsigned int>( host.toIPv4Address() );
					m_Mutex.lock();
					m_Q.push_back( sRecvPacket(data,len,ip) );
					m_Mutex.unlock();
				}
			
				UpdateLog();

				msleep(1);
			}
		}

		delete udpIn;

		SetState(ItemState::STATE_NOT_CONNECTED);		

		if(m_ReconnectDelay == 0)
			break;

		msg = QString("udp input %1:%2 reconnecting in %3...").arg(m_Addr.ip).arg(m_Addr.port).arg(m_ReconnectDelay/1000);
		m_PrivateLog.AddInfo( msg.toUtf8().constData() );
		UpdateLog();

		reconnectTimer.Start();
		while(m_Run && !reconnectTimer.GetExpired(m_ReconnectDelay))
			msleep(10);
	}
	
	msg = QString("udp input %1:%2 thread ended").arg(m_Addr.ip).arg(m_Addr.port);
	m_PrivateLog.AddInfo( msg.toUtf8().constData() );
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
	, m_Mutex(QMutex::Recursive)
	, m_ItemStateTableId(ItemStateTable::sm_Invalid_Id)
	, m_State(ItemState::STATE_UNINITIALIZED)
	, m_ReconnectDelay(0)
{
}

////////////////////////////////////////////////////////////////////////////////

EosUdpOutThread::~EosUdpOutThread()
{
	Stop();
}

////////////////////////////////////////////////////////////////////////////////

void EosUdpOutThread::Start(const EosAddr &addr, ItemStateTable::ID itemStateTableId, unsigned int reconnectDelayMS)
{
	Stop();

	m_Addr = addr;
	m_ItemStateTableId = itemStateTableId;
	m_ReconnectDelay = reconnectDelayMS;
	m_Run = true;
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
	if(GetState() == ItemState::STATE_CONNECTED)
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
	m_State = state;
	m_Mutex.unlock();
}

////////////////////////////////////////////////////////////////////////////////

void EosUdpOutThread::run()
{
	QString msg = QString("udp output %1:%2 thread started").arg(m_Addr.ip).arg(m_Addr.port);
	m_PrivateLog.AddInfo( msg.toUtf8().constData() );
	UpdateLog();
	
	EosTimer reconnectTimer;

	// outer loop for auto-reconnect
	while( m_Run )
	{
		SetState(ItemState::STATE_CONNECTING);

		EosUdpOut *udpOut = EosUdpOut::Create();
		if( udpOut->Initialize(m_PrivateLog,m_Addr.ip.toUtf8().constData(),m_Addr.port) )
		{
			SetState(ItemState::STATE_CONNECTED);

			OSCParser logParser;
			logParser.SetRoot(new OSCMethod());
			PacketLogger packetLogger(EosLog::LOG_MSG_TYPE_SEND, m_PrivateLog);
			packetLogger.SetPrefix( QString("OUT [%1:%2] ").arg(m_Addr.ip).arg(m_Addr.port).toUtf8().constData() );

			// run
			EosPacket::Q q;
			while( m_Run )
			{
				m_Mutex.lock();
				m_Q.swap(q);
				m_Mutex.unlock();
			
				for(EosPacket::Q::iterator i=q.begin(); m_Run && i!=q.end(); i++)
				{
					const char *buf = i->GetData();
					int len = i->GetSize();
					if( udpOut->SendPacket(m_PrivateLog,buf,len) )
						logParser.PrintPacket(packetLogger,buf,static_cast<size_t>(len));
				}
				q.clear();
			
				UpdateLog();

				msleep(1);
			}
		}

		delete udpOut;

		SetState(ItemState::STATE_NOT_CONNECTED);

		if(m_ReconnectDelay == 0)
			break;

		msg = QString("udp output %1:%2 reconnecting in %3...").arg(m_Addr.ip).arg(m_Addr.port).arg(m_ReconnectDelay/1000);
		m_PrivateLog.AddInfo( msg.toUtf8().constData() );
		UpdateLog();

		reconnectTimer.Start();
		while(m_Run && !reconnectTimer.GetExpired(m_ReconnectDelay))
			msleep(10);
	}
	
	msg = QString("udp output %1:%2 thread ended").arg(m_Addr.ip).arg(m_Addr.port);
	m_PrivateLog.AddInfo( msg.toUtf8().constData() );
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
	, m_Mutex(QMutex::Recursive)
	, m_ItemStateTableId(ItemStateTable::sm_Invalid_Id)
	, m_FrameMode(OSCStream::FRAME_MODE_INVALID)
	, m_State(ItemState::STATE_UNINITIALIZED)
	, m_ReconnectDelay(0)
{
}

////////////////////////////////////////////////////////////////////////////////

EosTcpClientThread::~EosTcpClientThread()
{
	Stop();
}

////////////////////////////////////////////////////////////////////////////////

void EosTcpClientThread::Start(const EosAddr &addr, ItemStateTable::ID itemStateTableId, OSCStream::EnumFrameMode frameMode, unsigned int reconnectDelayMS)
{
	Start(0, addr, itemStateTableId, frameMode, reconnectDelayMS);
}

////////////////////////////////////////////////////////////////////////////////

void EosTcpClientThread::Start(EosTcp *tcp, const EosAddr &addr, ItemStateTable::ID itemStateTableId, OSCStream::EnumFrameMode frameMode, unsigned int reconnectDelayMS)
{
	Stop();

	m_AcceptedTcp = tcp;
	m_Addr = addr;
	m_ItemStateTableId = itemStateTableId;
	m_FrameMode = frameMode;
	m_ReconnectDelay = reconnectDelayMS;
	m_Run = true;
	start();
}

////////////////////////////////////////////////////////////////////////////////

void EosTcpClientThread::Stop()
{
	m_Run = false;
	wait();

	if( m_AcceptedTcp )
	{
		delete m_AcceptedTcp;
		m_AcceptedTcp = 0;
	}
}

////////////////////////////////////////////////////////////////////////////////

bool EosTcpClientThread::Send(const EosPacket &packet)
{
	m_Mutex.lock();
	if(GetState() == ItemState::STATE_CONNECTED)
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
	if(GetState() == ItemState::STATE_CONNECTED)
	{
		size_t frameSize = packet.GetSize();
		char *frame = OSCStream::CreateFrame(m_FrameMode, packet.GetDataConst(), frameSize);
		if( frame )
		{
			m_SendQ.push_back( EosPacket(frame,frameSize) );
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
	m_PrivateLog.AddInfo( msg.toUtf8().constData() );
	UpdateLog();
	
	EosTimer reconnectTimer;

	// outer loop for auto-reconnect
	while( m_Run )
	{
		SetState(ItemState::STATE_CONNECTING);

		EosTcp *tcp = (m_AcceptedTcp ? m_AcceptedTcp : EosTcp::Create());
		m_AcceptedTcp = 0;
		if( tcp->Initialize(m_PrivateLog,m_Addr.ip.toUtf8().constData(),m_Addr.port) )
		{
			OSCParser logParser;
			logParser.SetRoot(new OSCMethod());
			PacketLogger inPacketLogger(EosLog::LOG_MSG_TYPE_RECV, m_PrivateLog);
			inPacketLogger.SetPrefix( QString("TCPIN [%1:%2] ").arg(m_Addr.ip).arg(m_Addr.port).toUtf8().constData() );
			PacketLogger outPacketLogger(EosLog::LOG_MSG_TYPE_SEND, m_PrivateLog);
			outPacketLogger.SetPrefix( QString("TCPOUT [%1:%2] ").arg(m_Addr.ip).arg(m_Addr.port).toUtf8().constData() );

			// connect
			while(m_Run && tcp->GetConnectState()==EosTcp::CONNECT_IN_PROGRESS)
			{
				tcp->Tick(m_PrivateLog);
				UpdateLog();
				msleep(10);
			}

			if(tcp->GetConnectState() == EosTcp::CONNECT_CONNECTED)
				SetState(ItemState::STATE_CONNECTED);

			UpdateLog();

			// send/recv while connected
			EosPacket::Q sendQ;
			unsigned int ip = m_Addr.toUInt();
			OSCStream recvStream(m_FrameMode);
			OSCStream sendStream(m_FrameMode);
			while(m_Run && tcp->GetConnectState()==EosTcp::CONNECT_CONNECTED)
			{
				size_t len = 0;
				const char *data = tcp->Recv(m_PrivateLog, 100, len);
				
				recvStream.Add(data, len);
				
				while( m_Run )
				{
					size_t frameSize = 0;
					char *frame = recvStream.GetNextFrame(frameSize);
					if( frame )
					{
						if(frameSize != 0)
						{
							logParser.PrintPacket(inPacketLogger, frame, frameSize);
							m_Mutex.lock();
							m_RecvQ.push_back( EosUdpInThread::sRecvPacket(frame,static_cast<int>(frameSize),ip) );
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
			
				for(EosPacket::Q::iterator i=sendQ.begin(); m_Run && i!=sendQ.end(); i++)
				{
					data = i->GetData();
					len = static_cast<size_t>( i->GetSize() );
					if( tcp->Send(m_PrivateLog,data,len) )
					{
						sendStream.Reset();
						sendStream.Add(data, len);
						for(;;)
						{
							size_t frameSize = 0;
							char *frame = recvStream.GetNextFrame(frameSize);
							if( frame )
							{
								if(frameSize != 0)
									logParser.PrintPacket(outPacketLogger, frame, frameSize);
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

		if(m_ReconnectDelay == 0)
			break;

		msg = QString("tcp client %1:%2 reconnecting in %3...").arg(m_Addr.ip).arg(m_Addr.port).arg(m_ReconnectDelay/1000);
		m_PrivateLog.AddInfo( msg.toUtf8().constData() );
		UpdateLog();

		reconnectTimer.Start();
		while(m_Run && !reconnectTimer.GetExpired(m_ReconnectDelay))
			msleep(10);
	}
	
	msg = QString("tcp client %1:%2 thread ended").arg(m_Addr.ip).arg(m_Addr.port);
	m_PrivateLog.AddInfo( msg.toUtf8().constData() );
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
	, m_Mutex(QMutex::Recursive)
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
	m_PrivateLog.AddInfo( msg.toUtf8().constData() );
	UpdateLog();
	
	EosTimer reconnectTimer;

	// outer loop for auto-reconnect
	while( m_Run )
	{
		SetState(ItemState::STATE_CONNECTING);

		EosTcpServer *tcpServer = EosTcpServer::Create();
		bool initialized = (m_Addr.ip.isEmpty()
			? tcpServer->Initialize(m_PrivateLog,m_Addr.port)
			: tcpServer->Initialize(m_PrivateLog,m_Addr.ip.toUtf8().constData(),m_Addr.port) );
		if( initialized )
		{
			if( tcpServer->GetListening() )
				SetState(ItemState::STATE_CONNECTED);

			sConnection connection;

			while(m_Run && tcpServer->GetListening())
			{
				sockaddr_in addr;
				int addrSize = static_cast<int>( sizeof(addr) );
				connection.tcp = tcpServer->Recv(m_PrivateLog, 100, &addr, &addrSize);
				if( connection.tcp )
				{
					char *ip = inet_ntoa( addr.sin_addr );
					if( ip )
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

		if(m_ReconnectDelay == 0)
			break;

		msg = QString("tcp server %1:%2 reconnecting in %3...").arg(m_Addr.ip).arg(m_Addr.port).arg(m_ReconnectDelay/1000);
		m_PrivateLog.AddInfo( msg.toUtf8().constData() );
		UpdateLog();

		reconnectTimer.Start();
		while(m_Run && !reconnectTimer.GetExpired(m_ReconnectDelay))
			msleep(10);
	}
	
	msg = QString("tcp server %1:%2 thread ended").arg(m_Addr.ip).arg(m_Addr.port);
	m_PrivateLog.AddInfo( msg.toUtf8().constData() );
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

RouterThread::RouterThread(const Router::ROUTES &routes, const Router::CONNECTIONS &tcpConnections, const ItemStateTable &itemStateTable, unsigned int reconnectDelayMS)
	: m_Mutex(QMutex::Recursive)
	, m_Routes(routes)
	, m_TcpConnections(tcpConnections)
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

void RouterThread::Flush(EosLog::LOG_Q &logQ, ItemStateTable &itemStateTable)
{
	m_Mutex.lock();
	m_Log.Flush(logQ);
	itemStateTable.Flush(m_ItemStateTable);
	m_Mutex.unlock();
}

////////////////////////////////////////////////////////////////////////////////

void RouterThread::BuildRoutes(ROUTES_BY_PORT &routesByPort, UDP_IN_THREADS &udpInThreads, UDP_OUT_THREADS &udpOutThreads, TCP_CLIENT_THREADS &tcpClientThreads, TCP_SERVER_THREADS &tcpServerThreads)
{
	m_PrivateLog.AddInfo("Building Routing Table...");
	
	// get a list of add network interface addresses
	std::vector<QNetworkAddressEntry> nics;
	QList<QNetworkInterface> allNics = QNetworkInterface::allInterfaces();
	for(QList<QNetworkInterface>::const_iterator i=allNics.begin(); i!=allNics.end(); i++)
	{
		const QNetworkInterface &nic = *i;
		if(nic.isValid() && nic.flags().testFlag(QNetworkInterface::IsUp))
		{
			QList<QNetworkAddressEntry> addrs = nic.addressEntries();
			for(QList<QNetworkAddressEntry>::const_iterator j=addrs.begin(); j!=addrs.end(); j++)
			{
				QHostAddress addr = j->ip();
				if(!addr.isNull() && addr.protocol()==QAbstractSocket::IPv4Protocol)
					nics.push_back( *j );
			}
		}
	}

	if( !nics.empty() )
	{
		// create TCP threads
		for(Router::CONNECTIONS::const_iterator i=m_TcpConnections.begin(); i!=m_TcpConnections.end(); i++)
		{
			const Router::sConnection &tcpConnection = *i;

			if(	tcpClientThreads.find(tcpConnection.addr)==tcpClientThreads.end() &&
				tcpServerThreads.find(tcpConnection.addr)==tcpServerThreads.end() )
			{
				if( tcpConnection.server )
				{
					EosTcpServerThread *thread = new EosTcpServerThread();
					tcpServerThreads[tcpConnection.addr] = thread;
					thread->Start(tcpConnection.addr, tcpConnection.itemStateTableId, tcpConnection.frameMode, m_ReconnectDelay);
				}
				else
				{
					EosTcpClientThread *thread = new EosTcpClientThread();
					tcpClientThreads[tcpConnection.addr] = thread;
					thread->Start(tcpConnection.addr, tcpConnection.itemStateTableId, tcpConnection.frameMode, m_ReconnectDelay);
				}
			}
		}

		QHostAddress localHost(QHostAddress::LocalHost);
		for(Router::ROUTES::const_iterator i=m_Routes.begin(); i!=m_Routes.end(); i++)
		{
			Router::sRoute route(*i);
			QHostAddress srcAddr(route.src.addr.ip);

			// create udp input thread on each network interface if necessary
			for(std::vector<QNetworkAddressEntry>::const_iterator j=nics.begin(); j!=nics.end(); j++)
			{
				EosAddr inAddr(j->ip().toString(), route.src.addr.port);
				if(udpInThreads.find(inAddr) == udpInThreads.end())
				{
					if(	route.src.addr.ip.isEmpty() ||
						srcAddr==j->ip() ||
						srcAddr.isInSubnet(j->ip(),j->prefixLength()) )
					{
						EosUdpInThread *thread = new EosUdpInThread();
						udpInThreads[inAddr] = thread;
						thread->Start(inAddr, route.srcItemStateTableId, m_ReconnectDelay);
					}
				}
			}

			if(route.dst.addr.port == 0)
				route.dst.addr.port = route.src.addr.port;	// no destination port specified, so assume same port as source

			// create udp output thread if known dst, and not an explicit tcp client
			if(tcpClientThreads.find(route.dst.addr) == tcpClientThreads.end())
				CreateUdpOutThread(route.dst.addr, route.dstItemStateTableId, udpOutThreads);

			// add entry to main routing table...

			// sorted 1st by port
			ROUTES_BY_PORT::iterator portIter = routesByPort.find( route.src.addr.port );
			if(portIter == routesByPort.end())
			{
				ROUTES_BY_IP empty;
				portIter = routesByPort.insert( ROUTES_BY_PORT_PAIR(route.src.addr.port,empty) ).first;
			}

			// sorted 2nd by ip
			unsigned int srcIp = route.src.addr.toUInt();
			ROUTES_BY_IP &routesByIp = portIter->second;
			ROUTES_BY_IP::iterator ipIter = routesByIp.find(srcIp);
			if(ipIter == routesByIp.end())
			{
				sRoutesByIp empty;
				ipIter = routesByIp.insert( ROUTES_BY_IP_PAIR(srcIp,empty) ).first;
			}

			// sorted 3rd by path
			ROUTES_BY_PATH &routesByPath = (route.src.path.contains('*')
				? ipIter->second.routesByWildcardPath
				: ipIter->second.routesByPath);
			ROUTES_BY_PATH::iterator pathIter = routesByPath.find( route.src.path );
			if(pathIter == routesByPath.end())
			{
				ROUTE_DESTINATIONS empty;
				pathIter = routesByPath.insert( ROUTES_BY_PATH_PAIR(route.src.path,empty) ).first;
			}

			// add destination
			ROUTE_DESTINATIONS &destinations = pathIter->second;
			sRouteDst routeDst;
			routeDst.dst = route.dst;
			routeDst.itemStateTableId = route.dstItemStateTableId;
			destinations.push_back(routeDst);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

EosUdpOutThread* RouterThread::CreateUdpOutThread(const EosAddr &addr, ItemStateTable::ID itemStateTableId, UDP_OUT_THREADS &udpOutThreads)
{
	if(!addr.ip.isEmpty() && addr.port!=0)
	{
		UDP_OUT_THREADS::iterator i = udpOutThreads.find(addr);
		if(i == udpOutThreads.end())
		{
			EosUdpOutThread *thread = new EosUdpOutThread();
			udpOutThreads[addr] = thread;
			thread->Start(addr, itemStateTableId, m_ReconnectDelay);
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
	if(isOSC && !path.isEmpty())
	{
		// exact matches
		ROUTES_BY_PATH_RANGE pathsRange = routesByIp.routesByPath.equal_range(path);
		for(; pathsRange.first!=pathsRange.second; pathsRange.first++)
			destinations.push_back( &(pathsRange.first->second) );

		// wildcard matches
		if( !routesByIp.routesByWildcardPath.empty() )
		{
			QRegExp rx;
			rx.setPatternSyntax(QRegExp::Wildcard);
			rx.setCaseSensitivity(Qt::CaseSensitive);
			for(ROUTES_BY_PATH::const_iterator i=routesByIp.routesByWildcardPath.begin(); i!=routesByIp.routesByWildcardPath.end(); i++)
			{
				rx.setPattern(i->first);
				if( rx.exactMatch(path) )
					destinations.push_back( &(i->second) );
			}
		}
	}

	// send to any routes without an explicit path specified
	QString noPath;
	ROUTES_BY_PATH_RANGE pathsRange = routesByIp.routesByPath.equal_range(noPath);
	for(; pathsRange.first!=pathsRange.second; pathsRange.first++)
		destinations.push_back( &(pathsRange.first->second) );
}

////////////////////////////////////////////////////////////////////////////////

void RouterThread::ProcessRecvQ(ROUTES_BY_PORT &routesByPort, DESTINATIONS_LIST &routingDestinationList, UDP_OUT_THREADS &udpOutThreads, TCP_CLIENT_THREADS &tcpClientThreads, const EosAddr &addr, EosUdpInThread::RECV_Q &recvQ)
{
	for(EosUdpInThread::RECV_Q::iterator i=recvQ.begin(); i!=recvQ.end(); i++)
		ProcessRecvPacket(routesByPort, routingDestinationList, udpOutThreads, tcpClientThreads, addr, *i);
	recvQ.clear();
}

////////////////////////////////////////////////////////////////////////////////

void RouterThread::ProcessRecvPacket(ROUTES_BY_PORT &routesByPort, DESTINATIONS_LIST &routingDestinationList, UDP_OUT_THREADS &udpOutThreads, TCP_CLIENT_THREADS &tcpClientThreads, const EosAddr &addr, EosUdpInThread::sRecvPacket &recvPacket)
{
	routingDestinationList.clear();

	// find osc path null terminator
	char *buf = recvPacket.packet.GetData();
	size_t packetSize = ((recvPacket.packet.GetSize()>0) ? static_cast<size_t>(recvPacket.packet.GetSize()) : 0);
	bool isOSC = OSCParser::IsOSCPacket(buf, packetSize);
	QString path;

	if( isOSC )
	{
		// get OSC path
		for(size_t i=0; i<packetSize; i++)
		{
			if(buf[i] == 0)
			{
				if(i != 0)
					path = QString::fromUtf8(buf);
				break;
			}
		}
	}

	// send to matching ports
	ROUTES_BY_PORT_RANGE portsRange = routesByPort.equal_range(addr.port);
	for(; portsRange.first!=portsRange.second; portsRange.first++)
	{
		const ROUTES_BY_IP &routesByIp = portsRange.first->second;

		// send to matching ips
		ROUTES_BY_IP_RANGE ipsRange = routesByIp.equal_range(recvPacket.ip);
		for(; ipsRange.first!=ipsRange.second; ipsRange.first++)
			AddRoutingDestinations(isOSC, path, ipsRange.first->second, routingDestinationList);

		// send to unspecified ips
		if(recvPacket.ip != 0)
		{
			ipsRange = routesByIp.equal_range(0);
			for(; ipsRange.first!=ipsRange.second; ipsRange.first++)
				AddRoutingDestinations(isOSC, path, ipsRange.first->second, routingDestinationList);
		}
	}

	if( !routingDestinationList.empty() )
	{
		size_t argsCount = 0;
		OSCArgument *args = 0;
		if( isOSC )
		{
			argsCount = 0xffffffff;
			args = OSCArgument::GetArgs(buf, packetSize, argsCount);
		}

		for(DESTINATIONS_LIST::const_iterator i=routingDestinationList.begin(); i!=routingDestinationList.end(); i++)
		{
			const ROUTE_DESTINATIONS &destinations = **i;
			for(ROUTE_DESTINATIONS::const_iterator j=destinations.begin(); j!=destinations.end(); j++)
			{
				const sRouteDst &routeDst = *j;

				EosAddr dstAddr(routeDst.dst.addr);
				if( dstAddr.ip.isEmpty() )
					EosAddr::UIntToIP(recvPacket.ip, dstAddr.ip);

				// send UDP or TCP?
				TCP_CLIENT_THREADS::const_iterator k = tcpClientThreads.find(dstAddr);
				if(k != tcpClientThreads.end())
				{
					EosTcpClientThread *thread = k->second;
					if( isOSC )
					{
						EosPacket packet;
						if(MakeOSCPacket(path,routeDst.dst,args,argsCount,packet) && thread->SendFramed(packet))
							SetItemActivity( thread->GetItemStateTableId() );
					}
					else if( thread->Send(recvPacket.packet) )
						SetItemActivity( thread->GetItemStateTableId() );
				}
				else
				{
					EosUdpOutThread *thread = CreateUdpOutThread(dstAddr, routeDst.itemStateTableId, udpOutThreads);
					if( thread )
					{
						if( isOSC )
						{
							EosPacket packet;
							if(MakeOSCPacket(path,routeDst.dst,args,argsCount,packet) && thread->Send(packet))
								SetItemActivity( thread->GetItemStateTableId() );
						}
						else if( thread->Send(recvPacket.packet) )
							SetItemActivity( thread->GetItemStateTableId() );
					}
				}
			}
		}

		if( args )
			delete[] args;
	}

	routingDestinationList.clear();
}

////////////////////////////////////////////////////////////////////////////////

bool RouterThread::MakeOSCPacket(const QString &srcPath, const EosRouteDst &dst, OSCArgument *args, size_t argsCount, EosPacket &packet)
{
	QString sendPath;
	MakeSendPath(srcPath, dst.path, sendPath);
	if( !sendPath.isEmpty() )
	{
		OSCPacketWriter oscPacket( sendPath.toUtf8().constData() );

		if( dst.hasAnyTransforms() )
		{
			if(args && argsCount!=0)
			{
				if( !ApplyTransform(args[0],dst,oscPacket) )
					return false;
			}
			else
				return false;
		}
		else
			oscPacket.AddOSCArgList(args, argsCount);

		size_t oscPacketSize = 0;
		char *oscPacketData = oscPacket.Create(oscPacketSize);
		if(oscPacketData && oscPacketSize)
		{
			packet = EosPacket(oscPacketData, static_cast<int>(oscPacketSize));
			delete[] oscPacketData;
			return true;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

void RouterThread::ProcessTcpConnectionQ(TCP_CLIENT_THREADS &tcpClientThreads, OSCStream::EnumFrameMode frameMode, EosTcpServerThread::CONNECTION_Q &tcpConnectionQ)
{
	for(EosTcpServerThread::CONNECTION_Q::const_iterator i=tcpConnectionQ.begin(); i!=tcpConnectionQ.end(); i++)
	{
		const EosTcpServerThread::sConnection &tcpConnection = *i;

		// check if an existing connection has been replaced
		TCP_CLIENT_THREADS::iterator clientIter = tcpClientThreads.find(tcpConnection.addr);
		if(clientIter != tcpClientThreads.end())
		{
			EosTcpClientThread *thread = clientIter->second;
			delete thread;
			tcpClientThreads.erase(clientIter);
		}

		EosTcpClientThread *thread = new EosTcpClientThread();
		tcpClientThreads[tcpConnection.addr] = thread;
		thread->Start(tcpConnection.tcp, tcpConnection.addr, ItemStateTable::sm_Invalid_Id, frameMode, m_ReconnectDelay);
	}
}

////////////////////////////////////////////////////////////////////////////////

bool RouterThread::ApplyTransform(OSCArgument &arg, const EosRouteDst &dst, OSCPacketWriter &packet)
{
	float f;
	if( arg.GetFloat(f) )
	{
		if(dst.inMin.enabled && dst.inMax.enabled && dst.outMin.enabled && dst.outMax.enabled)
		{
			// scale

			float range = (dst.inMax.value - dst.inMin.value);
			float t = ((range>-EPSILLON && range<EPSILLON) ? 0 : ((f - dst.inMin.value)/range));
			range = (dst.outMax.value - dst.outMin.value);
			f = ((range>-EPSILLON && range<EPSILLON) ? dst.outMin.value : (dst.outMin.value + t*range));
		}
		else
		{
			// just min/max limits

			if(dst.inMin.enabled || dst.outMin.enabled)
			{
				float fMin = (dst.inMin.enabled
					? (dst.outMin.enabled ? qMax(dst.inMin.value,dst.outMin.value) : dst.inMin.value)
					: dst.outMin.value);
				if(f < fMin)
				{
					packet.AddFloat32(fMin);
					return true;
				}
			}

			if(dst.inMax.enabled || dst.outMax.enabled)
			{
				float fMax = (dst.inMax.enabled
					? (dst.outMax.enabled ? qMin(dst.inMax.value,dst.outMax.value) : dst.inMax.value)
					: dst.outMax.value);
				if(f > fMax)
					f = fMax;
			}
		}

		packet.AddFloat32(f);
		return true;
	}
	
	return false;
}

////////////////////////////////////////////////////////////////////////////////

void RouterThread::MakeSendPath(const QString &srcPath, const QString &dstPath, QString &sendPath)
{
	if( dstPath.isEmpty() )
	{
		sendPath = srcPath;
	}
	else
	{
		sendPath = dstPath;

		if(!srcPath.isEmpty() && dstPath.contains('%'))
		{
			// possible in-line path replacements:
			// %1  => srcPath[0]
			// %2  => srcPath[1]
			// %%1 => %1
			// %A  => &A

			QStringList srcPathParts;
			bool srcPathPartsInitialized = false;

			// look for all instances of '%' follow by a number
			int digitCount = 0;
			for(int i=0; i<=sendPath.size(); i++)
			{
				if(i<sendPath.size() && sendPath[i].isDigit())
				{
					digitCount++;
				}
				else if(digitCount > 0)
				{
					// is number preceeded by a '%'?
					int startIndex = (i-digitCount-1);
					if(startIndex>0 && sendPath[startIndex]=='%')
					{
						// is '%' precceed by a '%'?
						if((startIndex-1)>0 && sendPath[startIndex-1]=='%')
						{
							// %%xxx => %xxx
							sendPath.remove(startIndex, 1);
							--i;
						}
						else
						{
							// %xxx => srcPath[xxx-1]

							int srcPathIndex = (sendPath.mid(startIndex+1,digitCount).toInt() - 1);

							if( !srcPathPartsInitialized )
							{
								srcPathParts = srcPath.split(OSC_ADDR_SEPARATOR, QString::SkipEmptyParts);
								if( srcPathParts.isEmpty() )
									srcPathParts << srcPath;
								srcPathPartsInitialized = true;
							}

							if(srcPathIndex < srcPathParts.size())
							{
								int midIndex = (startIndex + digitCount + 1);

								if(midIndex < sendPath.size())
									sendPath = sendPath.left(startIndex) + srcPathParts[srcPathIndex] + sendPath.mid(midIndex);
								else
									sendPath = sendPath.left(startIndex) + srcPathParts[srcPathIndex];

								i = (midIndex - 1);
							}
							else
							{
								QString msg = QString("Unable to remap %1 => %2, invlaid replacement index %3")
									.arg(srcPath)
									.arg(dstPath)
									.arg(srcPathIndex+1);
								m_PrivateLog.AddWarning( msg.toUtf8().constData() );
								sendPath.clear();
								return;
							}
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

void RouterThread::SetItemState(ItemStateTable::ID id, ItemState::EnumState state)
{
	m_Mutex.lock();
	const ItemState *itemState = m_ItemStateTable.GetItemState(id);
	if(itemState && itemState->state!=state)
	{
		ItemState newItemState(*itemState);
		newItemState.state = state;
		m_ItemStateTable.Update(id, newItemState);
	}
	m_Mutex.unlock();
}

////////////////////////////////////////////////////////////////////////////////

void RouterThread::SetItemActivity(ItemStateTable::ID id)
{
	m_Mutex.lock();
	const ItemState *itemState = m_ItemStateTable.GetItemState(id);
	if(itemState && !itemState->activity)
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
	m_PrivateLog.AddInfo("router thread started");
	UpdateLog();
	
	UDP_IN_THREADS						udpInThreads;
	UDP_OUT_THREADS						udpOutThreads;
	TCP_CLIENT_THREADS					tcpClientThreads;
	TCP_SERVER_THREADS					tcpServerThreads;
	ROUTES_BY_PORT						routesByPort;
	DESTINATIONS_LIST					routingDestinationList;
	EosUdpInThread::RECV_Q				recvQ;
	EosTcpServerThread::CONNECTION_Q	tcpConnectionQ;
	EosLog::LOG_Q						tempLogQ;

	BuildRoutes(routesByPort, udpInThreads, udpOutThreads, tcpClientThreads, tcpServerThreads);

	while( m_Run )
	{
		// UDP input
		for(UDP_IN_THREADS::iterator i=udpInThreads.begin(); i!=udpInThreads.end(); )
		{
			EosUdpInThread *thread = i->second;
			bool running = thread->isRunning();
			thread->Flush(tempLogQ, recvQ);
			m_PrivateLog.AddQ(tempLogQ);
			tempLogQ.clear();

			SetItemState(thread->GetItemStateTableId(), thread->GetState());
			if( !recvQ.empty() )
				SetItemActivity( thread->GetItemStateTableId() );

			ProcessRecvQ(routesByPort, routingDestinationList, udpOutThreads, tcpClientThreads, thread->GetAddr(), recvQ);
		
			if( !running )
			{
				delete thread;
				udpInThreads.erase(i++);
			}
			else
				i++;
		}

		// TCP servers
		for(TCP_SERVER_THREADS::iterator i=tcpServerThreads.begin(); i!=tcpServerThreads.end(); )
		{
			EosTcpServerThread *thread = i->second;
			bool running = thread->isRunning();
			thread->Flush(tempLogQ, tcpConnectionQ);
			m_PrivateLog.AddQ(tempLogQ);
			tempLogQ.clear();

			SetItemState(thread->GetItemStateTableId(), thread->GetState());

			if( !tcpConnectionQ.empty() )
			{
				SetItemActivity( thread->GetItemStateTableId() );
				ProcessTcpConnectionQ(tcpClientThreads, thread->GetFrameMode(), tcpConnectionQ);
			}

			if( !running )
			{
				delete thread;
				tcpServerThreads.erase(i++);
			}
			else
				i++;
		}

		// TCP clients
		for(TCP_CLIENT_THREADS::iterator i=tcpClientThreads.begin(); i!=tcpClientThreads.end(); )
		{
			EosTcpClientThread *thread = i->second;
			bool running = thread->isRunning();
			thread->Flush(tempLogQ, recvQ);
			m_PrivateLog.AddQ(tempLogQ);
			tempLogQ.clear();

			SetItemState(thread->GetItemStateTableId(), thread->GetState());
			if( !recvQ.empty() )
				SetItemActivity( thread->GetItemStateTableId() );

			ProcessRecvQ(routesByPort, routingDestinationList, udpOutThreads, tcpClientThreads, thread->GetAddr(), recvQ);

			if( !running )
			{
				delete thread;
				tcpClientThreads.erase(i++);
			}
			else
				i++;
		}
	
		// UDP output
		for(UDP_OUT_THREADS::iterator i=udpOutThreads.begin(); i!=udpOutThreads.end(); )
		{
			EosUdpOutThread *thread = i->second;
			bool running = thread->isRunning();
			thread->Flush(tempLogQ);
			m_PrivateLog.AddQ(tempLogQ);
			tempLogQ.clear();

			SetItemState(thread->GetItemStateTableId(), thread->GetState());
		
			if( !running )
			{
				delete thread;
				udpOutThreads.erase(i++);
			}
			else
				i++;
		}

		UpdateLog();

		msleep(1);
	}

	// shutdown
	for(TCP_SERVER_THREADS::const_iterator i=tcpServerThreads.begin(); i!=tcpServerThreads.end(); i++)
	{
		EosTcpServerThread *thread = i->second;
		thread->Stop();
		thread->Flush(tempLogQ, tcpConnectionQ);
		for(EosTcpServerThread::CONNECTION_Q::const_iterator j=tcpConnectionQ.begin(); j!=tcpConnectionQ.end(); j++)
			delete j->tcp;
		m_PrivateLog.AddQ(tempLogQ);
		tempLogQ.clear();
		delete thread;
	}

	for(TCP_CLIENT_THREADS::const_iterator i=tcpClientThreads.begin(); i!=tcpClientThreads.end(); i++)
	{
		EosTcpClientThread *thread = i->second;
		thread->Stop();
		thread->Flush(tempLogQ, recvQ);
		m_PrivateLog.AddQ(tempLogQ);
		tempLogQ.clear();
		delete thread;
	}

	for(UDP_OUT_THREADS::const_iterator i=udpOutThreads.begin(); i!=udpOutThreads.end(); i++)
	{
		EosUdpOutThread *thread = i->second;
		thread->Stop();
		thread->Flush(tempLogQ);
		m_PrivateLog.AddQ(tempLogQ);
		tempLogQ.clear();
		delete thread;
	}

	for(UDP_IN_THREADS::const_iterator i=udpInThreads.begin(); i!=udpInThreads.end(); i++)
	{
		EosUdpInThread *thread = i->second;
		thread->Stop();
		thread->Flush(tempLogQ, recvQ);
		m_PrivateLog.AddQ(tempLogQ);
		tempLogQ.clear();
		delete thread;
	}

	m_ItemStateTable.Deactivate();
	
	m_PrivateLog.AddInfo("router thread ended");
	UpdateLog();
}

////////////////////////////////////////////////////////////////////////////////
