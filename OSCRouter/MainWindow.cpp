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

#include "MainWindow.h"
#include "NetworkUtils.h"
#include "EosPlatform.h"
#include <time.h>

#ifdef WIN32
	#include <Windows.h>
	#include "resource.h"
#endif

// must be last include
#include "LeakWatcher.h"

////////////////////////////////////////////////////////////////////////////////

#define APP_VERSION					"0.9"
#define SETTING_LOG_DEPTH			"LogDepth"
#define SETTING_FILE_DEPTH			"FileDepth"
#define SETTING_LAST_FILE			"LastFile"
#define SETTING_RECONNECT_DELAY		"ReconnectDelay"
#define SETTING_DISABLE_SYSTEM_IDLE	"DisableSystemIdle"
#define ACTIVITY_TIMEOUT_MS			300

////////////////////////////////////////////////////////////////////////////////

QString FileUtils::QuotedString(const QString &str)
{
	// "test" -> """test"""
	// test,  -> "test,"

	QString quoted(str);
	quoted.replace("\"", "\"\"");
	if(quoted.contains('\"') || quoted.contains(','))
	{
		quoted.prepend("\"");
		quoted.append("\"");
	}
	return quoted;
}

////////////////////////////////////////////////////////////////////////////////

void FileUtils::GetItemsFromQuotedString(const QString &str, QStringList &items)
{
	items.clear();

	int len = str.size();
	int index = 0;
	bool quoted = false;
	for(int i=0; i<=len; i++)
	{
		if(i>=len || (str[i]==QChar(',') && !quoted))
		{
			int itemLen = (i - index);
			if(itemLen > 0)
			{
				QString item( str.mid(index,itemLen).trimmed() );

				// remove quotes
				if(item.startsWith('\"') && item.endsWith('\"'))
				{
					itemLen = (item.size() - 2);
					if(itemLen > 0)
						item = item.mid(1, itemLen);
					else
						item.clear();
				}

				// fix quoted quotes
				item.replace("\"\"", "\"");

				items.push_back(item);
			}
			else
				items.push_back( QString() );

			index = (i+1);
		}
		else if(str[i] == QChar('\"'))
		{
			if( !quoted )
				quoted = true;
			else if((i+1)>=len || str[i+1]!=QChar('\"'))
				quoted = false;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

Indicator::Indicator(QWidget *parent)
	: QWidget(parent)
	, m_Color(MUTED_COLOR)
	, m_UpdateTimer(0)
	, m_Timeout(0)
	, m_Opacity(0)
{
}

////////////////////////////////////////////////////////////////////////////////

void Indicator::Activate(unsigned int timeoutMS)
{
	SetOpacity(1.0);
	
	m_Timeout = timeoutMS;
	
	if(m_Timeout == 0)
	{
		if( m_UpdateTimer )
			m_UpdateTimer->stop();
	}
	else
	{
		if( !m_UpdateTimer )
		{
			m_UpdateTimer = new QTimer(this);
			connect(m_UpdateTimer, SIGNAL(timeout()), this, SLOT(onUpdate()));
		}
		
		m_Timer.Start();
		m_UpdateTimer->start(30);
	}
}

////////////////////////////////////////////////////////////////////////////////

void Indicator::Deactivate()
{
	if( m_UpdateTimer )
		m_UpdateTimer->stop();
	
	SetOpacity(0);
}

////////////////////////////////////////////////////////////////////////////////

void Indicator::SetOpacity(const qreal &opacity)
{
	if(m_Opacity != opacity)
	{
		m_Opacity = opacity;
		update();
	}
}

////////////////////////////////////////////////////////////////////////////////

void Indicator::SetColor(const QColor &color)
{
	if(m_Color != color)
	{
		m_Color = color;
		UpdateIcon();
	}
}

////////////////////////////////////////////////////////////////////////////////

void Indicator::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);
	UpdateIcon();
}

////////////////////////////////////////////////////////////////////////////////

void Indicator::paintEvent(QPaintEvent* /*event*/)
{
	if(!m_IconOutline.isNull() && !m_IconFill.isNull())
	{
		QPainter painter(this);
		if(m_Opacity > 0)
		{
			painter.setOpacity(m_Opacity);
			painter.drawImage((width()-m_IconFill.width())*0.5, (height()-m_IconFill.height())*0.5, m_IconFill);
			painter.setOpacity(1.0);
		}
		painter.drawImage((width()-m_IconOutline.width())*0.5, (height()-m_IconOutline.height())*0.5, m_IconOutline);
	}
}

////////////////////////////////////////////////////////////////////////////////

void Indicator::UpdateIcon()
{
	m_IconOutline = QImage();
	m_IconFill = QImage();

	if(m_Color.alpha() > 0)
	{
		QRect r( rect() );
		r.adjust(MARGIN, MARGIN, -MARGIN, -MARGIN);
		int size = qMin(r.width(), r.height());
		
		if(size > 2)
		{
			m_IconOutline = QImage(size, size, QImage::Format_ARGB32);
			m_IconOutline.fill(0);
		
			QPainter painter;
			if( painter.begin(&m_IconOutline) )
			{
				painter.setRenderHint(QPainter::Antialiasing);
				painter.setPen(m_Color);
				painter.setBrush(Qt::NoBrush);
				painter.drawEllipse( QRect(1,1,size-2,size-2) );
				painter.end();
		
				m_IconFill = QImage(size, size, QImage::Format_ARGB32);
				m_IconFill.fill(0);
				if( painter.begin(&m_IconFill) )
				{
					painter.setRenderHint(QPainter::Antialiasing);
					painter.setPen(Qt::NoPen);
					painter.setBrush(m_Color);
					painter.drawEllipse( QRect(1,1,size-2,size-2) );
					painter.end();
				}
				else
				{
					m_IconOutline = QImage();
					m_IconFill = QImage();
				}
			}
			else
				m_IconOutline = QImage();
		}
	}
	
	update();
}

////////////////////////////////////////////////////////////////////////////////

void Indicator::onUpdate()
{
	unsigned int elapsed = m_Timer.GetElapsed();
	if(elapsed >= m_Timeout)
	{
		SetOpacity(0);
		if( m_UpdateTimer )
			m_UpdateTimer->stop();
	}
	else
		SetOpacity(1.0 - elapsed/static_cast<qreal>(m_Timeout));
}

////////////////////////////////////////////////////////////////////////////////

TcpTableRow::TcpTableRow(size_t id, QWidget *parent)
	: QWidget(parent)
	, m_Id(id)
	, m_ItemStateTableId(ItemStateTable::sm_Invalid_Id)
{
	m_State = new Indicator(this);
	m_State->setToolTip( tr("Status") );
	
	m_Activity = new Indicator(this);
	m_Activity->setToolTip( tr("Activity") );
	
	m_Label = new QLineEdit(this);
	m_Label->setToolTip( tr("Just a text label for this TCP connection") );

	m_Server = new QComboBox(this);
	m_Server->setToolTip( tr("Server: create a server and accept incoming TCP connections\n\nClient: connect to a TCP server") );
	m_Server->addItem( tr("Server") );
	m_Server->addItem( tr("Client") );
	
	m_FrameMode = new QComboBox(this);
	m_FrameMode->setToolTip( tr("OSC 1.0: packets framed by 4-byte packet size header\n\nOSC 1.1: packets framed by SLIP (RFC 1055)") );
	for(int i=0; i<OSCStream::FRAME_MODE_COUNT; i++)
	{
		QString name;
		switch( i )
		{
			case OSCStream::FRAME_MODE_1_0:	name=tr("OSC 1.0"); break;
			case OSCStream::FRAME_MODE_1_1:	name=tr("OSC 1.1"); break;
		}
		
		m_FrameMode->addItem(name);
	}

	m_IP = new QLineEdit(this);
	m_IP->setToolTip( tr("Server: local network interface for TPC server to run on\n\nClient: IP address of TCP server to connect to") );
	m_Port = new QLineEdit(this);
	m_Port->setToolTip( tr("Server: local network interface port for TPC server to run on\n\nClient: port of TCP server to connect to") );

	m_AddRemove = new QPushButton(this);
	m_AddRemove->setToolTip( tr("Add/Remove this TCP connection") );
	connect(m_AddRemove, SIGNAL(clicked(bool)), this, SLOT(onAddRemoveClicked(bool)));
}

////////////////////////////////////////////////////////////////////////////////

QWidget* TcpTableRow::GetWigetForCol(size_t col)
{
	switch( col )
	{
		case COL_STATE:			return m_State;
		case COL_ACTIVITY:		return m_Activity;
		case COL_LABEL:			return m_Label;
		case COL_SERVER:		return m_Server;
		case COL_FRAME_MODE:	return m_FrameMode;
		case COL_IP:			return m_IP;
		case COL_PORT:			return m_Port;
		case COL_BUTTON:		return m_AddRemove;
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////////////

int TcpTableRow::GetWidthHintForCol(size_t col) const
{
	switch( col )
	{
		case COL_STATE:
		case COL_ACTIVITY:
			return 16;
		
		case COL_IP:
			return 100;

		case COL_SERVER:
			return m_Server->sizeHint().width();
			
		case COL_FRAME_MODE:
			return m_FrameMode->sizeHint().width();

		case COL_PORT:
			return 40;

		case COL_BUTTON:
			return m_AddRemove->sizeHint().height();
	}

	return -1;
}

////////////////////////////////////////////////////////////////////////////////

QSize TcpTableRow::sizeHint() const
{
	return m_Label->sizeHint();
}

////////////////////////////////////////////////////////////////////////////////

void TcpTableRow::SetAddRemoveText(const QString &text)
{
	m_AddRemove->setText(text);
}

////////////////////////////////////////////////////////////////////////////////

void TcpTableRow::UpdateLayout(const int *colSizes, size_t count, int margin)
{
	if( colSizes )
	{
		int h = sizeHint().height();
		int x = margin;
		if(count > NUM_COLS)
			count = NUM_COLS;
		
		for(size_t i=0; i<count; i++)
		{
			QWidget *w = GetWigetForCol(i);
			if( w )
			{
				w->setGeometry(x, 0, colSizes[i], h);
				x += (w->width() + margin);
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void TcpTableRow::Load(const QString &label, bool server, OSCStream::EnumFrameMode frameMode, const EosAddr &addr)
{
	m_State->SetColor(MUTED_COLOR);
	m_State->Deactivate();
	
	m_Activity->SetColor(MUTED_COLOR);
	m_Activity->Deactivate();
	
	m_Label->setText(label);

	m_Server->setCurrentIndex(server ? 0 : 1);
	
	m_FrameMode->setCurrentIndex(frameMode);

	m_IP->setText( addr.ip );
	m_Port->setText((addr.port==0) ? QString() : QString::number(addr.port));
}

////////////////////////////////////////////////////////////////////////////////

bool TcpTableRow::Save(QString &label, bool &server, OSCStream::EnumFrameMode &frameMode, EosAddr &addr) const
{
	label = m_Label->text();

	server = (m_Server->currentIndex() == 0);
	
	int n = m_FrameMode->currentIndex();
	frameMode = ((n>=0 && n<OSCStream::FRAME_MODE_COUNT)
		? static_cast<OSCStream::EnumFrameMode>(n)
		: OSCStream::FRAME_MODE_DEFAULT);

	addr.ip = m_IP->text();
	if(addr.ip == "0.0.0.0")
		addr.ip.clear();
	addr.port = m_Port->text().toUShort();
	
	// must have ip and port
	return (addr.port != 0);
}

////////////////////////////////////////////////////////////////////////////////

void TcpTableRow::UpdateItemState(const ItemStateTable &itemStateTable)
{
	const ItemState *itemState = itemStateTable.GetItemState(m_ItemStateTableId);
	if(itemState && itemState->dirty)
	{
		QColor color;
		ItemState::GetStateColor(itemState->state, color);
		m_State->SetColor(color);

		QString name;
		ItemState::GetStateName(itemState->state, name);
		m_State->setToolTip(name);

		if(itemState->state != ItemState::STATE_UNINITIALIZED)
			m_State->Activate(0);

		if( itemState->activity )
		{
			m_Activity->SetColor(ACTIVITY_COLOR);
			m_Activity->Activate(ACTIVITY_TIMEOUT_MS);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void TcpTableRow::onAddRemoveClicked(bool /*checked*/)
{
	emit addRemoveClicked(m_Id);
}

////////////////////////////////////////////////////////////////////////////////

TcpTable::TcpTable(QWidget *parent)
	: QWidget(parent)
	, m_UpdatingLayout(0)
{
	for(size_t i=0; i<TcpTableRow::NUM_COLS; i++)
	{
		m_Header[i] = new QLabel(this);
		m_Header[i]->setAlignment(Qt::AlignCenter);
		
		switch( i )
		{
			case TcpTableRow::COL_LABEL:		m_Header[i]->setText( tr("Label") ); break;
			case TcpTableRow::COL_SERVER:		m_Header[i]->setText( tr("Mode") ); break;
			case TcpTableRow::COL_FRAME_MODE:	m_Header[i]->setText( tr("Framing") ); break;
			case TcpTableRow::COL_IP:			m_Header[i]->setText( tr("IP") ); break;
			case TcpTableRow::COL_PORT:			m_Header[i]->setText( tr("Port") ); break;
		}
	}

	Router::CONNECTIONS noConnections;
	Load(noConnections);
}

////////////////////////////////////////////////////////////////////////////////

void TcpTable::Clear()
{
	for(ROWS::iterator i=m_Rows.begin(); i!=m_Rows.end(); i++)
	{
		(*i)->hide();
		(*i)->deleteLater();
	}
	m_Rows.clear();
}

////////////////////////////////////////////////////////////////////////////////

TcpTableRow* TcpTable::CreateRow(size_t id, bool remove, const QString &label, bool server, OSCStream::EnumFrameMode frameMode, const EosAddr &addr)
{
	TcpTableRow *row = new TcpTableRow(id, this);
	row->SetAddRemoveText(remove ? "-" : "+");
	row->Load(label, server, frameMode, addr);
	row->show();
	connect(row, SIGNAL(addRemoveClicked(size_t)), this, SLOT(onAddRemoveClicked(size_t)));
	return row;
}

////////////////////////////////////////////////////////////////////////////////

void TcpTable::Load(const Router::CONNECTIONS &connections)
{
	Clear();
	
	size_t id = 0;
	for(Router::CONNECTIONS::const_iterator i=connections.begin(); i!=connections.end(); i++)
		m_Rows.push_back( CreateRow(id++,/*remove*/true,i->label,i->server,i->frameMode,i->addr) );
	
	m_Rows.push_back( CreateRow(id++,/*remove*/false,QString(),/*server*/false,/*frameMode*/OSCStream::FRAME_MODE_DEFAULT,EosAddr()) );
	
	UpdateLayout(width(), /*forResize*/false);
}

////////////////////////////////////////////////////////////////////////////////

void TcpTable::Save(Router::CONNECTIONS &connections, ItemStateTable *itemStateTable) const
{
	connections.clear();
	
	for(size_t i=0; i<m_Rows.size(); i++)
	{
		Router::sConnection connection;
		if(m_Rows[i]->Save(connection.label,connection.server,connection.frameMode,connection.addr) )
		{
			if( !HasConnection(connections,connection.addr) )
			{
				if( itemStateTable )
				{
					connection.itemStateTableId = itemStateTable->Register();
					m_Rows[i]->SetItemStateTableId( connection.itemStateTableId );
				}
				else
					connection.itemStateTableId = ItemStateTable::sm_Invalid_Id;

				connections.push_back(connection);
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void TcpTable::LoadFromFile(const QStringList &lines)
{
	Router::CONNECTIONS connections;

	for(QStringList::const_iterator i=lines.begin(); i!=lines.end(); i++)
		LoadLineFromFile(*i, connections);

	// populate UI from file
	Load(connections);

	// save connections from UI and perform error checking
	Save(connections, /*itemStateTable*/0);

	// load saved connections (that have been error checked)
	Load(connections);
}

////////////////////////////////////////////////////////////////////////////////

void TcpTable::LoadLineFromFile(const QString &line, Router::CONNECTIONS &connections)
{
	QStringList items;
	FileUtils::GetItemsFromQuotedString(line, items);

	if(items.size() > 4)
	{
		Router::sConnection connection;

		bool ok = false;

		connection.label = items[0];
		int n = items[1].toInt( &ok );
		connection.server = (ok && n!=0);
		n = items[2].toInt( &ok );
		connection.frameMode = ((ok && n>=0 && n<OSCStream::FRAME_MODE_COUNT)
			? static_cast<OSCStream::EnumFrameMode>(n)
			: OSCStream::FRAME_MODE_INVALID );
		connection.addr.ip = items[3];
		connection.addr.port = items[4].toUShort();

		connections.push_back(connection);
	}
}

////////////////////////////////////////////////////////////////////////////////

bool TcpTable::SaveToFile(QFile &f) const
{
	bool success = true;

	if( f.isOpen() )
	{
		Router::CONNECTIONS connections;
		Save(connections, /*itemStateTable*/0);

		QString line;
		for(Router::CONNECTIONS::const_iterator i=connections.begin(); i!=connections.end(); i++)
		{
			const Router::sConnection &connection = *i;

			line.clear();
			line.append( QString("%1").arg(FileUtils::QuotedString(connection.label)) );
			line.append( QString(", %1").arg(static_cast<int>(connection.server ? 1 : 0)));
			line.append( QString(", %1").arg(static_cast<int>(connection.frameMode)));
			line.append( QString(", %1").arg(FileUtils::QuotedString(connection.addr.ip)) );
			line.append( QString(", %1").arg(connection.addr.port) );
			line.append("\n");

			QByteArray ba( line.toUtf8() );
			if(f.write(ba) != static_cast<qint64>(ba.size()))
				success = false;
		}
	}
	else
		success = false;

	return success;
}

////////////////////////////////////////////////////////////////////////////////

bool TcpTable::HasConnection(const Router::CONNECTIONS &connections, const EosAddr &addr)
{
	for(Router::CONNECTIONS::const_iterator i=connections.begin(); i!=connections.end(); i++)
	{
		if(i->addr == addr)
			return true;
	}
	
	return false;
}

////////////////////////////////////////////////////////////////////////////////

void TcpTable::UpdateLayout(int w, bool forResize)
{
	if(m_UpdatingLayout==0 && !m_Rows.empty())
	{
		m_UpdatingLayout++;

		TcpTableRow *firstRow = m_Rows.front();

		int margin = 4;
		int availWidth = (w - (TcpTableRow::NUM_COLS+1)*margin);
		int flexWidthCols = 0;
		for(size_t i=0; i<TcpTableRow::NUM_COLS; i++)
		{
			int fixedWidth = firstRow->GetWidthHintForCol(i);
			if(fixedWidth > 0)
				availWidth -= fixedWidth;
			else
				flexWidthCols++;
		}

		const int MIN_COL_SIZE = 40;

		int flexWidth = ((flexWidthCols>0 && availWidth>0)
			? (availWidth/flexWidthCols)
			: MIN_COL_SIZE);

		if(flexWidth < MIN_COL_SIZE)
			flexWidth = MIN_COL_SIZE;
		
		int colSizes[TcpTableRow::NUM_COLS];
		for(size_t i=0; i<TcpTableRow::NUM_COLS; i++)
		{
			int fixedWidth = firstRow->GetWidthHintForCol(i);
			if(fixedWidth > 0)
				colSizes[i] = fixedWidth;
			else
				colSizes[i] = flexWidth;
		}
	
		int y = margin;
		int rowHeight = firstRow->sizeHint().height();
	
		int x = margin;
		for(size_t i=0; i<TcpTableRow::NUM_COLS; i++)
		{
			m_Header[i]->setGeometry(x, y, colSizes[i], rowHeight);
			x += (m_Header[i]->width() + margin);
		}
		y += (rowHeight + margin);
		
		for(ROWS::const_iterator i=m_Rows.begin(); i!=m_Rows.end(); i++)
		{
			TcpTableRow *row = *i;
			row->UpdateLayout(colSizes, TcpTableRow::NUM_COLS, /*margin*/4);
			row->setGeometry(0, y, x, rowHeight);
			y += (row->height() + margin);
		}

		if( !forResize )
			resize(x, y);
		
		m_UpdatingLayout--;
	}
}

////////////////////////////////////////////////////////////////////////////////

void TcpTable::Apply()
{
	Router::CONNECTIONS connections;
	Save(connections, /*itemStateTable*/0);
	Load(connections);
}

////////////////////////////////////////////////////////////////////////////////

void TcpTable::UpdateItemState(const ItemStateTable &itemStateTable)
{
	for(size_t i=0; i<m_Rows.size(); i++)
		m_Rows[i]->UpdateItemState(itemStateTable);
}

////////////////////////////////////////////////////////////////////////////////

void TcpTable::onAddRemoveClicked(size_t id)
{
	if(id < m_Rows.size())
	{
		if(id == (m_Rows.size()-1))
		{
			// add new, blank connection
			m_Rows.push_back( CreateRow(m_Rows.size()-1,/*remove*/false,QString(),/*server*/false,OSCStream::FRAME_MODE_DEFAULT,EosAddr()) );
		}
		else
		{
			// remove existing connection
			m_Rows[id]->hide();
			m_Rows[id]->deleteLater();
			m_Rows.erase(m_Rows.begin() + id);
		}

		Router::CONNECTIONS connections;
		Save(connections, /*itemStateTable*/0);
		Load(connections);
	}
}

////////////////////////////////////////////////////////////////////////////////

void TcpTable::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);
	UpdateLayout(width(), /*forResize*/true);
}

////////////////////////////////////////////////////////////////////////////////

TcpTableWindow::TcpTableWindow(QWidget *parent)
	: QWidget(parent, Qt::Tool)
{
	setWindowTitle( tr("TCP Connections") );

	m_TableScrollArea = new TableScrollArea(this);
	
	m_TcpTable = new TcpTable(0);
	m_TableScrollArea->setWidget(m_TcpTable);
	connect(m_TableScrollArea, SIGNAL(resized(int,int)), m_TcpTable, SLOT(autoSize(int,int)));
}

////////////////////////////////////////////////////////////////////////////////

void TcpTableWindow::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);
	QMargins m = contentsMargins();
	m_TableScrollArea->setGeometry(m.left(), m.top(), width()-m.left()-m.right(), height()-m.top()-m.bottom());
}

////////////////////////////////////////////////////////////////////////////////

RoutingTableRow::RoutingTableRow(size_t id, QWidget *parent)
	: QWidget(parent)
	, m_Id(id)
	, m_InItemStateTableId(ItemStateTable::sm_Invalid_Id)
	, m_OutItemStateTableId(ItemStateTable::sm_Invalid_Id)
{
	m_InState = new Indicator(this);
	m_InState->setToolTip( tr("Status") );
	
	m_InActivity = new Indicator(this);
	m_InActivity->setToolTip( tr("Activity") );
	
	m_Label = new QLineEdit(this);
	m_Label->setToolTip( tr("Just a text label for this route") );

	m_InIP = new QLineEdit(this);
	m_InIP->setToolTip( tr("Only route packets received from this specific IP address\n\nLeave blank to route packets received from any IP address") );

	m_InPort = new QLineEdit(this);
	m_InPort->setToolTip( tr("Route packets received on this port (REQUIRED)") );

	m_InPath = new QLineEdit(this);
	m_InPath->setToolTip( tr("Only route received OSC commands with this specific OSC command path\n(use * for wildcard matching, ex: /eos/out/event/*)\n\nLeave blank to route received packets with any OSC command path (or non-OSC packets)") );

	m_InMin = new QLineEdit(this);
	m_InMin->setToolTip( tr("Clip first outgoing OSC argument\n\nScale first outgoing OSC argument when all min/max fields populated") );

	m_InMax = new QLineEdit(this);
	m_InMax->setToolTip( tr("Clip first outgoing OSC argument\n\nScale first outgoing OSC argument when all min/max fields populated") );

	m_Divider = new QLabel(">", this);
	m_Divider->setAlignment(Qt::AlignCenter);

	m_OutState = new Indicator(this);
	m_OutState->setToolTip( tr("Status") );
	
	m_OutActivity = new Indicator(this);
	m_OutActivity->setToolTip( tr("Activity") );

	m_OutIP = new QLineEdit(this);
	m_OutIP->setToolTip( tr("Route received packets to this IP address\n\nLeave blank to route packets to the same IP address they were sent from") );

	m_OutPort = new QLineEdit(this);
	m_OutPort->setToolTip( tr("Route received packets to this port\n\nLeave blank to route packets to the same port they were received on") );

	m_OutPath = new QLineEdit(this);
	m_OutPath->setToolTip( tr("Route received OSC commands to this OSC command path\n\nUse %n to insert specific sections from the received OSC command path\nex:\nOSC Command Path:/eos/out/event/cue/1/25/fire\nIn Path:/eos/out/event/cue/1/*\nOutPath:/cue/%6/start\nResult:/cue/25/start\n\nLeave blank to use the same the OSC command path as it was received with") );

	m_OutMin = new QLineEdit(this);
	m_OutMin->setToolTip( tr("Clip first outgoing OSC argument\n\nScale first outgoing OSC argument when all min/max fields populated") );

	m_OutMax = new QLineEdit(this);	
	m_OutMax->setToolTip( tr("Clip first outgoing OSC argument\n\nScale first outgoing OSC argument when all min/max fields populated") );

	m_AddRemove = new QPushButton(this);
	m_AddRemove->setToolTip( tr("Add/Remove this route") );
	connect(m_AddRemove, SIGNAL(clicked(bool)), this, SLOT(onAddRemoveClicked(bool)));
}

////////////////////////////////////////////////////////////////////////////////

QWidget* RoutingTableRow::GetWigetForCol(size_t col)
{
	switch( col )
	{
		case COL_IN_STATE:		return m_InState;
		case COL_IN_ACTIVITY:	return m_InActivity;
		case COL_LABEL:			return m_Label;
		case COL_IN_IP:			return m_InIP;
		case COL_IN_PORT:		return m_InPort;
		case COL_IN_PATH:		return m_InPath;
		case COL_IN_MIN:		return m_InMin;
		case COL_IN_MAX:		return m_InMax;
		case COL_DIVIDER:		return m_Divider;
		case COL_OUT_STATE:		return m_OutState;
		case COL_OUT_ACTIVITY:	return m_OutActivity;
		case COL_OUT_IP:		return m_OutIP;
		case COL_OUT_PORT:		return m_OutPort;
		case COL_OUT_PATH:		return m_OutPath;
		case COL_OUT_MIN:		return m_OutMin;
		case COL_OUT_MAX:		return m_OutMax;
		case COL_BUTTON:		return m_AddRemove;
	}

	return 0;
}

////////////////////////////////////////////////////////////////////////////////

int RoutingTableRow::GetWidthHintForCol(size_t col) const
{
	switch( col )
	{
		case COL_IN_STATE:
		case COL_IN_ACTIVITY:
		case COL_OUT_STATE:
		case COL_OUT_ACTIVITY:
			return 16;
			
		case COL_IN_IP:
		case COL_OUT_IP:
			return 100;

		case COL_IN_PORT:
		case COL_OUT_PORT:
		case COL_IN_MIN:
		case COL_IN_MAX:
		case COL_OUT_MIN:
		case COL_OUT_MAX:
			return 40;

		case COL_DIVIDER:
			return 30;

		case COL_BUTTON:
			return m_AddRemove->sizeHint().height();
	}

	return -1;
}

////////////////////////////////////////////////////////////////////////////////

QSize RoutingTableRow::sizeHint() const
{
	return m_Label->sizeHint();
}

////////////////////////////////////////////////////////////////////////////////

void RoutingTableRow::SetAddRemoveText(const QString &text)
{
	m_AddRemove->setText(text);
}

////////////////////////////////////////////////////////////////////////////////

void RoutingTableRow::UpdateLayout(const int *colSizes, size_t count, int margin)
{
	if( colSizes )
	{
		int h = sizeHint().height();
		int x = margin;
		if(count > NUM_COLS)
			count = NUM_COLS;
		
		for(size_t i=0; i<count; i++)
		{
			QWidget *w = GetWigetForCol(i);
			if( w )
			{
				w->setGeometry(x, 0, colSizes[i], h);
				x += (w->width() + margin);
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void RoutingTableRow::Load(const QString &label, const EosRouteSrc &src, const EosRouteDst &dst)
{
	m_InState->SetColor(MUTED_COLOR);
	m_InState->Deactivate();
	
	m_InActivity->SetColor(MUTED_COLOR);
	m_InActivity->Deactivate();
	
	m_Label->setText(label);

	m_InIP->setText( src.addr.ip );
	m_InPort->setText((src.addr.port==0) ? QString() : QString::number(src.addr.port));
	m_InPath->setText( src.path );

	m_OutState->SetColor(MUTED_COLOR);
	m_OutState->Deactivate();
	
	m_OutActivity->SetColor(MUTED_COLOR);
	m_OutActivity->Deactivate();

	m_OutIP->setText( dst.addr.ip );
	m_OutPort->setText((dst.addr.port==0) ? QString() : QString::number(dst.addr.port));
	m_OutPath->setText( dst.path );
	
	QString transformStr;

	TransformToString(dst.inMin, transformStr);
	m_InMin->setText(transformStr);

	TransformToString(dst.inMax, transformStr);
	m_InMax->setText(transformStr);

	TransformToString(dst.outMin, transformStr);
	m_OutMin->setText(transformStr);

	TransformToString(dst.outMax, transformStr);
	m_OutMax->setText(transformStr);
}

////////////////////////////////////////////////////////////////////////////////

bool RoutingTableRow::Save(QString &label, EosRouteSrc &src, EosRouteDst &dst) const
{
	label = m_Label->text();

	src.addr.ip = m_InIP->text();	
	src.addr.port = m_InPort->text().toUShort();
	src.path = m_InPath->text();
	
	dst.addr.ip = m_OutIP->text();
	dst.addr.port = m_OutPort->text().toUShort();
	dst.path = m_OutPath->text();

	StringToTransform(m_InMin->text(), dst.inMin);
	StringToTransform(m_InMax->text(), dst.inMax);
	StringToTransform(m_OutMin->text(), dst.outMin);
	StringToTransform(m_OutMax->text(), dst.outMax);
	
	// must have src port
	return (src.addr.port != 0);
}

////////////////////////////////////////////////////////////////////////////////

void RoutingTableRow::UpdateItemState(const ItemStateTable &itemStateTable)
{
	UpdateItemState(itemStateTable.GetItemState(m_InItemStateTableId), *m_InState, *m_InActivity);
	UpdateItemState(itemStateTable.GetItemState(m_OutItemStateTableId), *m_OutState, *m_OutActivity);
}

////////////////////////////////////////////////////////////////////////////////

void RoutingTableRow::UpdateItemState(const ItemState *itemState, Indicator &stateIndicator, Indicator &activityIndicator)
{
	if(itemState && itemState->dirty)
	{
		QColor color;
		ItemState::GetStateColor(itemState->state, color);
		stateIndicator.SetColor(color);

		QString name;
		ItemState::GetStateName(itemState->state, name);
		stateIndicator.setToolTip(name);

		if(itemState->state != ItemState::STATE_UNINITIALIZED)
			stateIndicator.Activate(0);

		if( itemState->activity )
		{
			activityIndicator.SetColor(ACTIVITY_COLOR);
			activityIndicator.Activate(ACTIVITY_TIMEOUT_MS);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void RoutingTableRow::StringToTransform(const QString &str, EosRouteDst::sTransform &transform)
{
	if( str.isEmpty() )
	{
		transform.enabled = false;
		transform.value = 0;
	}
	else
	{
		transform.value = str.toFloat(&transform.enabled);
		if( !transform.enabled )
			transform.value = 0;
	}
}

////////////////////////////////////////////////////////////////////////////////

void RoutingTableRow::TransformToString(const EosRouteDst::sTransform &transform, QString &str)
{
	str = (transform.enabled ? QString::number(transform.value) : QString());
}

////////////////////////////////////////////////////////////////////////////////

void RoutingTableRow::onAddRemoveClicked(bool /*checked*/)
{
	emit addRemoveClicked(m_Id);
}

////////////////////////////////////////////////////////////////////////////////

RoutingTable::RoutingTable(QWidget *parent)
	: QWidget(parent)
	, m_UpdatingLayout(0)
{
	for(size_t i=0; i<RoutingTableRow::NUM_COLS; i++)
	{
		m_Header[i] = new QLabel(this);
		m_Header[i]->setAlignment(Qt::AlignCenter);

		switch( i )
		{
			case RoutingTableRow::COL_LABEL:	m_Header[i]->setText( tr("Label") ); break;
			case RoutingTableRow::COL_IN_IP:	m_Header[i]->setText( tr("Incoming IP") ); break;
			case RoutingTableRow::COL_IN_PORT:	m_Header[i]->setText( tr("Port") ); break;
			case RoutingTableRow::COL_IN_PATH:	m_Header[i]->setText( tr("Path") ); break;
			case RoutingTableRow::COL_IN_MIN:	m_Header[i]->setText( tr("Min") ); break;
			case RoutingTableRow::COL_IN_MAX:	m_Header[i]->setText( tr("Max") ); break;
			case RoutingTableRow::COL_OUT_IP:	m_Header[i]->setText( tr("Outgoing IP") ); break;
			case RoutingTableRow::COL_OUT_PORT:	m_Header[i]->setText( tr("Port") ); break;
			case RoutingTableRow::COL_OUT_PATH:	m_Header[i]->setText( tr("Path") ); break;
			case RoutingTableRow::COL_OUT_MIN:	m_Header[i]->setText( tr("Min") ); break;
			case RoutingTableRow::COL_OUT_MAX:	m_Header[i]->setText( tr("Max") ); break;
		}
	}

	m_Tcp = new QPushButton(tr("TCP Settings..."), this);
	connect(m_Tcp, SIGNAL(clicked(bool)), this, SLOT(onTcpClicked(bool)));

	m_Apply = new QPushButton(tr("Apply"), this);
	connect(m_Apply, SIGNAL(clicked(bool)), this, SLOT(onApplyClicked(bool)));

	m_TcpTableWindow = new TcpTableWindow(this);
	m_TcpTableWindow->hide();

	Router::ROUTES noRoutes;
	LoadRoutes(noRoutes);

	Router::CONNECTIONS noTcpConnections;
	LoadTcpConnections(noTcpConnections);
}

////////////////////////////////////////////////////////////////////////////////

void RoutingTable::Clear()
{
	for(ROWS::iterator i=m_Rows.begin(); i!=m_Rows.end(); i++)
	{
		(*i)->hide();
		(*i)->deleteLater();
	}
	m_Rows.clear();
}

////////////////////////////////////////////////////////////////////////////////

RoutingTableRow* RoutingTable::CreateRow(size_t id, bool remove, const QString &label, const EosRouteSrc &src, const EosRouteDst &dst)
{
	RoutingTableRow *row = new RoutingTableRow(id, this);
	row->SetAddRemoveText(remove ? "-" : "+");
	row->Load(label, src, dst);
	row->show();
	connect(row, SIGNAL(addRemoveClicked(size_t)), this, SLOT(onAddRemoveClicked(size_t)));
	return row;
}

////////////////////////////////////////////////////////////////////////////////

void RoutingTable::LoadRoutes(const Router::ROUTES &routes)
{
	Clear();
	
	size_t id = 0;
	for(Router::ROUTES::const_iterator i=routes.begin(); i!=routes.end(); i++)
		m_Rows.push_back( CreateRow(id++,/*remove*/true,i->label,i->src,i->dst) );
	
	m_Rows.push_back( CreateRow(id++,/*remove*/false,QString(),EosRouteSrc(),EosRouteDst()) );
	
	UpdateLayout(width(), /*forResize*/false);
}

////////////////////////////////////////////////////////////////////////////////

void RoutingTable::SaveRoutes(Router::ROUTES &routes, ItemStateTable *itemStateTable) const
{
	routes.clear();

	// show state/activity per EosAddr
	ADDR_STATES srcAddrStates;
	ADDR_STATES dstAddrStates;
	
	for(size_t i=0; i<m_Rows.size(); i++)
	{
		Router::sRoute route;
		if(m_Rows[i]->Save(route.label,route.src,route.dst) )
		{
			if( !HasRoute(routes,route.src,route.dst) )
			{
				if( itemStateTable )
				{
					ADDR_STATES::const_iterator j = srcAddrStates.find( route.src.addr );
					if(j == srcAddrStates.end())
						srcAddrStates[ route.src.addr ] = route.srcItemStateTableId = itemStateTable->Register();
					else
						route.srcItemStateTableId = j->second;
					m_Rows[i]->SetInItemStateTableId( route.srcItemStateTableId );

					j = dstAddrStates.find( route.dst.addr );
					if(j == dstAddrStates.end())
						dstAddrStates[ route.dst.addr ] = route.dstItemStateTableId = itemStateTable->Register();
					else
						route.dstItemStateTableId = j->second;
					m_Rows[i]->SetOutItemStateTableId( route.dstItemStateTableId );
				}
				else
					route.srcItemStateTableId = route.dstItemStateTableId = ItemStateTable::sm_Invalid_Id;

				routes.push_back(route);
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void RoutingTable::LoadTcpConnections(const Router::CONNECTIONS &tcpConnections)
{
	m_TcpTableWindow->GetTcpTable().Load(tcpConnections);
}

////////////////////////////////////////////////////////////////////////////////

void RoutingTable::SaveTcpConnections(Router::CONNECTIONS &tcpConnections, ItemStateTable *itemStateTable)
{
	m_TcpTableWindow->GetTcpTable().Save(tcpConnections, itemStateTable);
}

////////////////////////////////////////////////////////////////////////////////

bool RoutingTable::LoadFromFile(const QString &path)
{
	QFile f(path);
	if( f.open(QIODevice::ReadOnly|QIODevice::Text) )
	{
		Router::ROUTES routes;

		QStringList lines = QString( f.readAll() ).split('\n',QString::SkipEmptyParts);
		for(QStringList::iterator i=lines.begin(); i!=lines.end(); i++)
			i->remove('\r');

		for(QStringList::iterator i=lines.begin(); i!=lines.end(); i++)
			LoadLineFromFile(*i, routes);

		m_TcpTableWindow->GetTcpTable().LoadFromFile(lines);

		f.close();

		// populate UI from file
		LoadRoutes(routes);

		// save routes from UI and perform error checking
		SaveRoutes(routes, /*itemStateTable*/0);

		// load saved routes (that have been error checked)
		LoadRoutes(routes);

		return true;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

void RoutingTable::LoadLineFromFile(const QString &line, Router::ROUTES &routes)
{
	QStringList items;
	FileUtils::GetItemsFromQuotedString(line, items);

	if(items.size() > 10)
	{
		Router::sRoute route;

		route.label = items[0];
		route.src.addr.ip = items[1];
		route.src.addr.port = items[2].toUShort();
		route.src.path = items[3];
		RoutingTableRow::StringToTransform(items[4], route.dst.inMin);
		RoutingTableRow::StringToTransform(items[5], route.dst.inMax);

		route.dst.addr.ip = items[6];
		route.dst.addr.port = items[7].toUShort();
		route.dst.path = items[8];
		RoutingTableRow::StringToTransform(items[9], route.dst.outMin);
		RoutingTableRow::StringToTransform(items[10], route.dst.outMax);

		routes.push_back(route);
	}
}

////////////////////////////////////////////////////////////////////////////////

bool RoutingTable::SaveToFile(const QString &path) const
{
	bool success = true;

	QFile f(path);
	if( f.open(QIODevice::WriteOnly|QIODevice::Truncate|QIODevice::Text) )
	{
		Router::ROUTES routes;
		SaveRoutes(routes, /*itemStateTable*/0);

		QString line;
		for(Router::ROUTES::const_iterator i=routes.begin(); i!=routes.end(); i++)
		{
			const Router::sRoute &route = *i;

			QString inMinStr;
			RoutingTableRow::TransformToString(route.dst.inMin, inMinStr);
			QString inMaxStr;
			RoutingTableRow::TransformToString(route.dst.inMax, inMaxStr);
			QString outMinStr;
			RoutingTableRow::TransformToString(route.dst.outMin, outMinStr);
			QString outMaxStr;
			RoutingTableRow::TransformToString(route.dst.outMax, outMaxStr);

			line.clear();
			line.append( QString("%1").arg(FileUtils::QuotedString(route.label)) );
			line.append( QString(", %1").arg(FileUtils::QuotedString(route.src.addr.ip)) );
			line.append( QString(", %1").arg(route.src.addr.port) );
			line.append( QString(", %1").arg(FileUtils::QuotedString(route.src.path)) );
			line.append( QString(", %1").arg(inMinStr) );
			line.append( QString(", %1").arg(inMaxStr) );
			line.append( QString(", %1").arg(FileUtils::QuotedString(route.dst.addr.ip)) );
			line.append( QString(", %1").arg(route.dst.addr.port) );
			line.append( QString(", %1").arg(FileUtils::QuotedString(route.dst.path)) );
			line.append( QString(", %1").arg(outMinStr) );
			line.append( QString(", %1").arg(outMaxStr) );
			line.append("\n");

			QByteArray ba( line.toUtf8() );
			if(f.write(ba) != static_cast<qint64>(ba.size()))
				success = false;
		}

		m_TcpTableWindow->GetTcpTable().SaveToFile(f);

		f.close();
	}
	else
		success = false;

	return success;
}

////////////////////////////////////////////////////////////////////////////////

bool RoutingTable::HasRoute(const Router::ROUTES &routes, const EosRouteSrc &src, const EosRouteDst &dst)
{
	for(Router::ROUTES::const_iterator i=routes.begin(); i!=routes.end(); i++)
	{
		if(i->src==src && i->dst==dst)
			return true;
	}
	
	return false;
}

////////////////////////////////////////////////////////////////////////////////

void RoutingTable::UpdateLayout(int w, bool forResize)
{
	if(m_UpdatingLayout==0 && !m_Rows.empty())
	{
		m_UpdatingLayout++;

		RoutingTableRow *firstRow = m_Rows.front();

		int margin = 4;
		int availWidth = (w - (RoutingTableRow::NUM_COLS+1)*margin);
		int flexWidthCols = 0;
		for(size_t i=0; i<RoutingTableRow::NUM_COLS; i++)
		{
			int fixedWidth = firstRow->GetWidthHintForCol(i);
			if(fixedWidth > 0)
				availWidth -= fixedWidth;
			else
				flexWidthCols++;
		}

		const int MIN_COL_SIZE = 40;

		int flexWidth = ((flexWidthCols>0 && availWidth>0)
			? (availWidth/flexWidthCols)
			: MIN_COL_SIZE);

		if(flexWidth < MIN_COL_SIZE)
			flexWidth = MIN_COL_SIZE;
		
		int colSizes[RoutingTableRow::NUM_COLS];
		for(size_t i=0; i<RoutingTableRow::NUM_COLS; i++)
		{
			int fixedWidth = firstRow->GetWidthHintForCol(i);
			if(fixedWidth > 0)
				colSizes[i] = fixedWidth;
			else
				colSizes[i] = flexWidth;
		}
	
		int y = margin;
		int rowHeight = firstRow->sizeHint().height();
	
		int x = margin;
		for(size_t i=0; i<RoutingTableRow::NUM_COLS; i++)
		{
			m_Header[i]->setGeometry(x, y, colSizes[i], rowHeight);
			x += (m_Header[i]->width() + margin);
		}
		y += (rowHeight + margin);
		
		for(ROWS::const_iterator i=m_Rows.begin(); i!=m_Rows.end(); i++)
		{
			RoutingTableRow *row = *i;
			row->UpdateLayout(colSizes, RoutingTableRow::NUM_COLS, /*margin*/4);
			row->setGeometry(0, y, x, rowHeight);
			y += (row->height() + margin);
		}

		m_Tcp->move(w-m_Tcp->width()-margin, y);
		y += (m_Tcp->height() + margin);

		m_Apply->move(w-m_Apply->width()-margin, y);
		y += (m_Apply->height() + margin);		
		
		if( !forResize )
			resize(x, y);
		
		m_UpdatingLayout--;
	}
}

////////////////////////////////////////////////////////////////////////////////

void RoutingTable::UpdateItemState(const ItemStateTable &itemStateTable)
{
	for(size_t i=0; i<m_Rows.size(); i++)
		m_Rows[i]->UpdateItemState(itemStateTable);

	m_TcpTableWindow->GetTcpTable().UpdateItemState(itemStateTable);
}

////////////////////////////////////////////////////////////////////////////////

void RoutingTable::onAddRemoveClicked(size_t id)
{
	if(id < m_Rows.size())
	{
		if(id == (m_Rows.size()-1))
		{
			// add new, blank route
			m_Rows.push_back( CreateRow(m_Rows.size()-1,/*remove*/false,QString(),EosRouteSrc(),EosRouteDst()) );
		}
		else
		{
			// remove existing route
			m_Rows[id]->hide();
			m_Rows[id]->deleteLater();
			m_Rows.erase(m_Rows.begin() + id);
		}

		Router::ROUTES routes;
		SaveRoutes(routes, /*itemStateTable*/0);
		LoadRoutes(routes);
	}
}

////////////////////////////////////////////////////////////////////////////////

void RoutingTable::onTcpClicked(bool /*checked*/)
{
	m_TcpTableWindow->setVisible( !m_TcpTableWindow->isVisible() );
}

////////////////////////////////////////////////////////////////////////////////

void RoutingTable::onApplyClicked(bool /*checked*/)
{
	m_TcpTableWindow->GetTcpTable().Apply();

	Router::ROUTES routes;
	SaveRoutes(routes, /*itemStateTable*/0);
	LoadRoutes(routes);
	emit changed();
}

////////////////////////////////////////////////////////////////////////////////

void RoutingTable::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);
	UpdateLayout(width(), /*forResize*/true);
}

////////////////////////////////////////////////////////////////////////////////

MainWindow::MainWindow(QWidget* parent/*=0*/, Qt::WindowFlags f/*=0*/)
	: QWidget(parent, f)
	, m_Settings("ETC", "OSCRouter")
	, m_LogDepth(200)
	, m_FileDepth(10000)
	, m_Unsaved(false)
	, m_RouterThread(0)
	, m_FileLineCount(0)
	, m_ReconnectDelay(5000)
{
#ifdef WIN32
	QIcon icon;

	const int iconSizes[] = {512, 256, 128, 64, 32, 16};
	const size_t numIcons = sizeof(iconSizes)/sizeof(iconSizes[0]);
	for(size_t i=0; i<numIcons; i++)
	{
		HICON hIcon = static_cast<HICON>( LoadImage(GetModuleHandle(0),MAKEINTRESOURCE(IDI_ICON1),IMAGE_ICON,iconSizes[i],iconSizes[i],LR_LOADTRANSPARENT) );
		if( hIcon )
		{
			icon.addPixmap( QtWin::fromHICON(hIcon) );
			DestroyIcon(hIcon);
		}
	}

	setWindowIcon(icon);
#endif

	m_LogDepth = m_Settings.value(SETTING_LOG_DEPTH, m_LogDepth).toInt();
	if(m_LogDepth < 1)
		m_LogDepth = 1;
	m_Settings.setValue(SETTING_LOG_DEPTH, m_LogDepth);

	m_FileDepth = m_Settings.value(SETTING_FILE_DEPTH, m_FileDepth).toInt();
	m_Settings.setValue(SETTING_FILE_DEPTH, m_FileDepth);

	int n = m_Settings.value(SETTING_RECONNECT_DELAY, static_cast<int>(m_ReconnectDelay)).toInt();
	m_ReconnectDelay = ((n>0) ? static_cast<unsigned int>(n) : 0);
	m_Settings.setValue(SETTING_RECONNECT_DELAY, m_ReconnectDelay);
	
	n = m_Settings.value(SETTING_DISABLE_SYSTEM_IDLE, 1).toInt();
	m_DisableSystemIdle = (n != 0);
	m_Settings.setValue(SETTING_DISABLE_SYSTEM_IDLE, static_cast<int>(m_DisableSystemIdle?1:0));

	InitLogFile();
	
	m_Platform = EosPlatform::Create();
	if( m_Platform )
	{
		std::string error;
		if( !m_Platform->Initialize(error) )
		{
			m_Log.AddError("platform initialization failed");
			m_Platform->Shutdown();
			m_Platform = 0;
		}
	}

	QGridLayout *layout = new QGridLayout(this);

	QSplitter *splitter = new QSplitter(Qt::Vertical, this);
	layout->addWidget(splitter, 0, 0);

	TableScrollArea *tableScrollArea = new TableScrollArea(splitter);
	splitter->addWidget(tableScrollArea);
	
	m_RoutingTable = new RoutingTable(0);
	connect(m_RoutingTable, SIGNAL(changed()), this, SLOT(onRoutingTableChanged()));
	tableScrollArea->setWidget(m_RoutingTable);
	connect(tableScrollArea, SIGNAL(resized(int,int)), m_RoutingTable, SLOT(autoSize(int,int)));
	
	QWidget *logBase = new QWidget(splitter);
	QGridLayout *logLayout = new QGridLayout(logBase);
	splitter->addWidget(logBase);

	m_LogWidget = new QListWidget(logBase);
	QPalette logPal( m_LogWidget->palette() );
	logPal.setColor(QPalette::Base, BG_COLOR);
	m_LogWidget->setPalette(logPal);
	m_LogWidget->setSelectionMode(QAbstractItemView::NoSelection);
	m_LogWidget->setMovement(QListView::Static);
	QFont fnt("Monospace");
	fnt.setStyleHint(QFont::TypeWriter);
	m_LogWidget->setFont(fnt);
	logLayout->addWidget(m_LogWidget, 0, 0, 1, 6);

	QPushButton *button = new QPushButton("New", logBase);
	connect(button, SIGNAL(clicked(bool)), this, SLOT(onNewFileClicked(bool)));
	logLayout->addWidget(button, 1, 0);

	button = new QPushButton("Open", logBase);
	connect(button, SIGNAL(clicked(bool)), this, SLOT(onOpenFileClicked(bool)));
	logLayout->addWidget(button, 1, 1);

	button = new QPushButton("Save", logBase);
	connect(button, SIGNAL(clicked(bool)), this, SLOT(onSaveFileClicked(bool)));
	logLayout->addWidget(button, 1, 2);

	button = new QPushButton("Save As...", logBase);
	connect(button, SIGNAL(clicked(bool)), this, SLOT(onSaveAsFileClicked(bool)));
	logLayout->addWidget(button, 1, 3);

	button = new QPushButton("Clear Log", logBase);
	connect(button, SIGNAL(clicked(bool)), this, SLOT(onClearLogClicked(bool)));
	logLayout->addWidget(button, 1, 4);

	button = new QPushButton("View Log", logBase);
	connect(button, SIGNAL(clicked(bool)), this, SLOT(onOpenLogClicked(bool)));
	logLayout->addWidget(button, 1, 5);
	
	m_Log.AddInfo( QString("OSCRouter v%1").arg(APP_VERSION).toUtf8().constData() );
	
	QTimer *timer = new QTimer(this);
	connect(timer, SIGNAL(timeout()), this, SLOT(onTick()));
	timer->start(60);

	RestoreLastFile();
	UpdateWindowTitle();
}

////////////////////////////////////////////////////////////////////////////////

MainWindow::~MainWindow()
{
	if( m_Platform )
	{
		m_Platform->Shutdown();
		delete m_Platform;
		m_Platform = 0;
	}

	Shutdown();
	ShutdownLogFile();
}

////////////////////////////////////////////////////////////////////////////////

void MainWindow::InitLogFile()
{
	if(m_FileDepth > 0)
	{
		m_LogFile.setFileName( QDir(QDir::tempPath()).absoluteFilePath("OSCRouter.txt") );
		if( m_LogFile.open(QIODevice::WriteOnly|QIODevice::Truncate|QIODevice::Text) )
		{
			m_LogStream.setDevice( &m_LogFile );
			m_LogStream.setCodec("UTF-8");
			m_LogStream.setGenerateByteOrderMark(true);
		}
	}

	m_FileLineCount = 0;
}

////////////////////////////////////////////////////////////////////////////////

void MainWindow::ShutdownLogFile()
{
	if( m_LogFile.isOpen() )
	{
		m_LogStream.flush();
		m_LogFile.close();
	}

	m_FileLineCount = 0;
}

////////////////////////////////////////////////////////////////////////////////

void MainWindow::FlushLogQ(EosLog::LOG_Q &logQ)
{
	if( !logQ.empty() )
	{
		m_LogWidget->setUpdatesEnabled(false);

		QScrollBar *vs = m_LogWidget->verticalScrollBar();
		bool dontAutoScroll = (vs && vs->isEnabled() && vs->isVisible() && vs->value()<vs->maximum());

		for(EosLog::LOG_Q::const_iterator i=logQ.begin(); i!=logQ.end(); i++)
		{
			const EosLog::sLogMsg logMsg = *i;

			tm *t = localtime( &logMsg.timestamp );

			QString msgText;
			if( logMsg.text.c_str() )
				msgText = QString::fromUtf8( logMsg.text.c_str() );

			QString itemText = QString("[ %1:%2:%3 ]  %4")
				.arg(t->tm_hour, 2)
				.arg(t->tm_min, 2, 10, QChar('0'))
				.arg(t->tm_sec, 2, 10, QChar('0'))
				.arg( msgText );

			if( m_LogFile.isOpen() )
			{
				m_LogStream << itemText;
				m_LogStream << "\n";

				if(++m_FileLineCount > m_FileDepth)
				{
					ShutdownLogFile();
					InitLogFile();
				}
			}

			QListWidgetItem *item = new QListWidgetItem(itemText);
			switch( logMsg.type )
			{
				case EosLog::LOG_MSG_TYPE_DEBUG:
					item->setForeground(MUTED_COLOR);
					break;

				case EosLog::LOG_MSG_TYPE_WARNING:
					item->setForeground(WARNING_COLOR);
					break;

				case EosLog::LOG_MSG_TYPE_ERROR:
					item->setForeground(ERROR_COLOR);
					break;

				case EosLog::LOG_MSG_TYPE_RECV:
					item->setForeground(RECV_COLOR);
					break;

				case EosLog::LOG_MSG_TYPE_SEND:
					item->setForeground(SEND_COLOR);
					break;
			}

			m_LogWidget->addItem(item);

			while(m_LogWidget->count() > m_LogDepth)
			{
				QListWidgetItem *itemToRemove = m_LogWidget->takeItem(0);
				if( itemToRemove )
					delete itemToRemove;
			}
		}		

		m_LogWidget->setUpdatesEnabled(true);
	
		if(	!dontAutoScroll )
			m_LogWidget->setCurrentRow(m_LogWidget->count() - 1);
	}
}

////////////////////////////////////////////////////////////////////////////////

void MainWindow::Shutdown()
{
	if( m_RouterThread )
	{
		m_RouterThread->Stop();
		FlushRouterThread(/*logsOnly*/true);
		delete m_RouterThread;
		m_RouterThread = 0;
		
		if(m_Platform && m_DisableSystemIdle)
		{
			std::string error;
			if( m_Platform->SetSystemIdleAllowed(true,"routing stopped",error) )
			{
				m_Log.AddInfo("routing stopped, system idle allowed");
			}
			else
			{
				error.insert(0, "failed to allow system idle, ");
				m_Log.AddDebug(error);
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

bool MainWindow::BuildRoutes()
{
	Shutdown();

	m_ItemStateTable.Clear();

	Router::ROUTES routes;
	m_RoutingTable->SaveRoutes(routes, &m_ItemStateTable);

	Router::CONNECTIONS tcpConnections;
	m_RoutingTable->SaveTcpConnections(tcpConnections, &m_ItemStateTable);

	if( !routes.empty() )
	{
		if(m_Platform && m_DisableSystemIdle)
		{
			std::string error;
			if( m_Platform->SetSystemIdleAllowed(false,"routing started",error) )
			{
				m_Log.AddInfo("routing started, system idle disabled");
			}
			else
			{
				error.insert(0, "failed to disable system idle, ");
				m_Log.AddDebug(error);
			}
		}
	
		m_RouterThread = new RouterThread(routes, tcpConnections, m_ItemStateTable, m_ReconnectDelay);
		m_RouterThread->start();
		return true;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

void MainWindow::GetDefaultIP(QString &ip)
{
	QHostAddress localHostAddr(QHostAddress::LocalHost);

	QList<QNetworkInterface> nics = QNetworkInterface::allInterfaces();
	for(QList<QNetworkInterface>::const_iterator i=nics.begin(); i!=nics.end(); i++)
	{
		const QNetworkInterface &net = *i;
		if( net.isValid() &&
			net.flags().testFlag(QNetworkInterface::IsUp) &&
			net.flags().testFlag(QNetworkInterface::IsRunning) )
		{
			QList<QNetworkAddressEntry>	addrs = net.addressEntries();
			for(QList<QNetworkAddressEntry>::const_iterator j=addrs.begin(); j!=addrs.end(); j++)
			{
				QHostAddress addr = j->ip();
				if(	!addr.isNull() &&
					addr.protocol()==QAbstractSocket::IPv4Protocol &&
					addr!=localHostAddr )
				{
					ip = addr.toString();
					return;
				}
			}
		}
	}
	
	ip = localHostAddr.toString();
}

////////////////////////////////////////////////////////////////////////////////

void MainWindow::GetPersistentSavePath(QString &path) const
{
	path = QDir(QStandardPaths::writableLocation(QStandardPaths::DataLocation)).absoluteFilePath("save.osc.txt");
}

////////////////////////////////////////////////////////////////////////////////

void MainWindow::UpdateWindowTitle()
{
	QString title( tr("OSCRouter") );
	if( !m_FilePath.isEmpty() )
	{
		title.append(" :: ");
		if( m_Unsaved )
			title.append("*");
		title.append( QDir::toNativeSeparators(m_FilePath) );
	}
	else if( m_Unsaved )
		title.append("*");
	setWindowTitle(title);
}

////////////////////////////////////////////////////////////////////////////////

bool MainWindow::LoadFile(const QString &path)
{
	m_FilePath = path;
	m_Settings.setValue(SETTING_LAST_FILE, m_FilePath);

	if( m_RoutingTable->LoadFromFile(path) )
	{
		m_Unsaved = false;
		UpdateWindowTitle();
		BuildRoutes();
		return true;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool MainWindow::SaveFile(const QString &path)
{
	m_FilePath = path;
	m_Settings.setValue(SETTING_LAST_FILE, m_FilePath);

	if( m_RoutingTable->SaveToFile(path) )
	{
		m_Unsaved = false;
		UpdateWindowTitle();
		return true;
	}
	else
		QMessageBox::critical(this, tr("OSCRouter"), tr("Unable to save file \"%1\"").arg(path));

	return false;
}

////////////////////////////////////////////////////////////////////////////////

void MainWindow::RestoreLastFile()
{
	QString path = m_Settings.value(SETTING_LAST_FILE).toString();
	if( !path.isEmpty() )
	{
		if( LoadFile(path) )
			return;	// success
	}

	// fall-back to loading persistent file
	GetPersistentSavePath(path);
	if( m_RoutingTable->LoadFromFile(path) )
	{
		m_FilePath = m_Settings.value(SETTING_LAST_FILE).toString();
		if( BuildRoutes() )
			m_Unsaved = true;
	}
}

////////////////////////////////////////////////////////////////////////////////

void MainWindow::FlushRouterThread(bool logsOnly)
{
	if( m_RouterThread )
	{
		m_RouterThread->Flush(m_TempLogQ, m_ItemStateTable);
		m_Log.AddQ(m_TempLogQ);
	}

	m_Log.Flush(m_TempLogQ);
	FlushLogQ(m_TempLogQ);
	m_TempLogQ.clear();
	
	if( !logsOnly )
	{
		if( m_ItemStateTable.GetDirty() )
		{
			m_RoutingTable->UpdateItemState(m_ItemStateTable);
			m_ItemStateTable.Reset();
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void MainWindow::onTick()
{
	FlushRouterThread(/*logsOnly*/false);
}

////////////////////////////////////////////////////////////////////////////////

void MainWindow::onRoutingTableChanged()
{
	if( !m_Unsaved )
	{
		m_Unsaved = true;
		UpdateWindowTitle();
	}

	QTimer::singleShot(1, this, SLOT(buildRoutes()));
}

////////////////////////////////////////////////////////////////////////////////

void MainWindow::buildRoutes()
{
	BuildRoutes();
}

////////////////////////////////////////////////////////////////////////////////

void MainWindow::onNewFileClicked(bool /*checked*/)
{
	Router::ROUTES noRoutes;
	m_RoutingTable->LoadRoutes(noRoutes);
	Router::CONNECTIONS noTcpConnections;
	m_RoutingTable->LoadTcpConnections(noTcpConnections);
	m_FilePath.clear();
	m_Settings.setValue(SETTING_LAST_FILE, m_FilePath);
	QString path;
	GetPersistentSavePath(path);
	QFile::setPermissions(path, QFile::WriteOwner);
	QFile::remove(path);
	m_Unsaved = false;
	BuildRoutes();
	UpdateWindowTitle();
}

////////////////////////////////////////////////////////////////////////////////

void MainWindow::onOpenFileClicked(bool /*checked*/)
{
	QString dir;
	QString lastFile = m_Settings.value(SETTING_LAST_FILE).toString();	
	if( !lastFile.isEmpty() )
		dir = QFileInfo(lastFile).absolutePath();
	if(dir.isEmpty() || !QFileInfo(dir).exists())
		dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
	QString path = QFileDialog::getOpenFileName(this, tr("Open"), dir, tr("OSCRouter File (*.osc.txt)"));
	if( !path.isEmpty() )
	{
		if( !LoadFile(path) )
			QMessageBox::critical(this, tr("OSCRouter"), tr("Unable to open file \"%1\"").arg(path));
	}
}

////////////////////////////////////////////////////////////////////////////////

void MainWindow::onSaveFileClicked(bool /*checked*/)
{
	if( m_FilePath.isEmpty() )
		onSaveAsFileClicked(false);
	else
		SaveFile(m_FilePath);
}

////////////////////////////////////////////////////////////////////////////////

void MainWindow::onSaveAsFileClicked(bool /*checked*/)
{
	QString dir;
	QString lastFile = m_Settings.value(SETTING_LAST_FILE).toString();	
	if( !lastFile.isEmpty() )
		dir = QFileInfo(lastFile).absolutePath();
	if(dir.isEmpty() || !QFileInfo(dir).exists())
		dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
	QString path = QFileDialog::getSaveFileName(this, tr("Save"), dir, tr("OSCRouter File (*.osc.txt)"));
	if( !path.isEmpty() )
		SaveFile(path);
}

////////////////////////////////////////////////////////////////////////////////

void MainWindow::onClearLogClicked(bool /*checked*/)
{
	m_LogWidget->clear();
}

////////////////////////////////////////////////////////////////////////////////

void MainWindow::onOpenLogClicked(bool /*checked*/)
{
	if( m_LogFile.exists() )
	{
		if( m_LogFile.isOpen() )
			m_LogStream.flush();
		QDesktopServices::openUrl( QUrl::fromLocalFile(m_LogFile.fileName()) );
	}
}

////////////////////////////////////////////////////////////////////////////////

void MainWindow::closeEvent(QCloseEvent *event)
{
	if( m_Unsaved )
	{
		QMessageBox mb(QMessageBox::Question, tr("OSCRouter"), tr("Do you want to save changes?"), QMessageBox::NoButton, this);
		QPushButton *saveButton = mb.addButton(tr("Save"), QMessageBox::AcceptRole);
		mb.addButton(tr("Don't Save"), QMessageBox::DestructiveRole);
		QPushButton *cancelButton = mb.addButton(tr("Cancel"), QMessageBox::RejectRole);

		mb.exec();

		if(mb.clickedButton() == saveButton)
		{
			onSaveFileClicked(false);
			if( m_Unsaved )
			{
				event->ignore();
				return;	// error saving, do not close
			}
		}
		else if(mb.clickedButton() == cancelButton)
		{
			event->ignore();
			return;
		}
	}

	QString path;
	GetPersistentSavePath(path);
	m_RoutingTable->SaveToFile(path);

	QWidget::closeEvent(event);
}

////////////////////////////////////////////////////////////////////////////////
