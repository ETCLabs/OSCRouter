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
#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#ifndef QT_INCLUDE_H
#include "QtInclude.h"
#endif

#ifndef EOS_LOG_H
#include "EosLog.h"
#endif

#ifndef EOS_TIMER_H
#include "EosTimer.h"
#endif

#ifndef OSC_PARSER_H
#include "OSCParser.h"
#endif

#ifndef ITEM_STATE_H
#include "ItemState.h"
#endif

#ifndef ROUTER_H
#include "Router.h"
#endif

class EosPlatform;

////////////////////////////////////////////////////////////////////////////////

class TableScrollArea
	: public QScrollArea
{
	Q_OBJECT
	
public:
	TableScrollArea(QWidget *parent)
		: QScrollArea(parent)
	{}
	
	virtual QSize sizeHint() const {return QSize(1000,1000);}
	
signals:
	void resized(int w, int h);
	
protected:
	virtual void resizeEvent(QResizeEvent *event)
	{
		QScrollArea::resizeEvent(event);
		emit resized(viewport()->width(), height());
	}
};

////////////////////////////////////////////////////////////////////////////////

class FileUtils
{
public:
	static QString QuotedString(const QString &str);
	static void GetItemsFromQuotedString(const QString &str, QStringList &items);
};

////////////////////////////////////////////////////////////////////////////////

class Indicator
	: public QWidget
{
	Q_OBJECT
	
public:
	Indicator(QWidget *parent);
	
	virtual void SetColor(const QColor &color);
	virtual void Activate(unsigned int timeoutMS);
	virtual void Deactivate();
	
private slots:
	void onUpdate();
	
protected:
	enum EnumConstants
	{
		MARGIN = 2
	};
	
	QColor			m_Color;
	QImage			m_IconOutline;
	QImage			m_IconFill;
	QTimer			*m_UpdateTimer;
	unsigned int	m_Timeout;
	EosTimer		m_Timer;
	qreal			m_Opacity;
	
	virtual void resizeEvent(QResizeEvent *event);
	virtual void paintEvent(QPaintEvent *event);
	virtual void UpdateIcon();
	virtual void SetOpacity(const qreal &opacity);
};

////////////////////////////////////////////////////////////////////////////////

class TcpTableRow
	: public QWidget
{
	Q_OBJECT

public:
	enum EnumConstants
	{
		COL_STATE = 0,
		COL_ACTIVITY,
		COL_LABEL,
		COL_SERVER,
		COL_FRAME_MODE,
		COL_IP,
		COL_PORT,
		COL_BUTTON,

		NUM_COLS
	};
	
	TcpTableRow(size_t id, QWidget *parent);
	
	virtual QSize sizeHint() const;
	virtual void SetAddRemoveText(const QString &text);
	virtual void UpdateLayout(const int *colSizes, size_t count, int margin);
	virtual void Load(const QString &label, bool server, OSCStream::EnumFrameMode frameMode, const EosAddr &addr);
	virtual bool Save(QString &label, bool &server, OSCStream::EnumFrameMode &frameMode, EosAddr &addr) const;
	virtual QWidget* GetWigetForCol(size_t col);
	virtual int GetWidthHintForCol(size_t col) const;
	virtual void SetItemStateTableId(ItemStateTable::ID itemStateTableId) {m_ItemStateTableId = itemStateTableId;}
	virtual void UpdateItemState(const ItemStateTable &itemStateTable);
	
signals:
	void addRemoveClicked(size_t id);

private slots:
	void onAddRemoveClicked(bool checked);
	
private:
	size_t				m_Id;
	ItemStateTable::ID	m_ItemStateTableId;
	Indicator			*m_State;
	Indicator			*m_Activity;
	QLineEdit			*m_Label;
	QComboBox			*m_Server;
	QComboBox			*m_FrameMode;
	QLineEdit			*m_IP;
	QLineEdit			*m_Port;
	QPushButton			*m_AddRemove;
};

////////////////////////////////////////////////////////////////////////////////

class TcpTable
	: public QWidget
{
	Q_OBJECT
	
public:
	TcpTable(QWidget *parent);
	
	virtual void Clear();
	virtual void Load(const Router::CONNECTIONS &connections);
	virtual void Save(Router::CONNECTIONS &connections, ItemStateTable *itemStateTable) const;
	virtual void LoadFromFile(const QStringList &lines);
	virtual bool SaveToFile(QFile &f) const;
	virtual void Apply();
	virtual void UpdateItemState(const ItemStateTable &itemStateTable);
	
	static bool HasConnection(const Router::CONNECTIONS &connections, const EosAddr &addr);
	
public slots:
	void autoSize(int w, int /*h*/) { UpdateLayout(w,/*forResize*/false); }
	void onAddRemoveClicked(size_t id);
	
protected:
	typedef std::vector<TcpTableRow*> ROWS;
	
	QLabel			*m_Header[TcpTableRow::NUM_COLS];
	ROWS			m_Rows;
	unsigned int	m_UpdatingLayout;
	
	virtual void UpdateLayout(int w, bool forResize);
	virtual TcpTableRow* CreateRow(size_t id, bool remove, const QString &label, bool server, OSCStream::EnumFrameMode frameMode, const EosAddr &addr);
	virtual void LoadLineFromFile(const QString &line, Router::CONNECTIONS &connections);
	virtual void resizeEvent(QResizeEvent *event);
};

////////////////////////////////////////////////////////////////////////////////

class TcpTableWindow
	: public QWidget
{
public:
	TcpTableWindow(QWidget *parent);

	QSize sizeHint() const {return QSize(450,300);}
	TcpTable& GetTcpTable() {return *m_TcpTable;}

protected:
	TableScrollArea	*m_TableScrollArea;
	TcpTable		*m_TcpTable;

	virtual void resizeEvent(QResizeEvent *event);
};

////////////////////////////////////////////////////////////////////////////////

class RoutingTableRow
	: public QWidget
{
	Q_OBJECT

public:
	enum EnumConstants
	{
		COL_IN_STATE = 0,
		COL_IN_ACTIVITY,
		COL_LABEL,
		COL_IN_IP,
		COL_IN_PORT,
		COL_IN_PATH,
		COL_IN_MIN,
		COL_IN_MAX,
		COL_DIVIDER,
		COL_OUT_STATE,
		COL_OUT_ACTIVITY,
		COL_OUT_IP,
		COL_OUT_PORT,
		COL_OUT_PATH,
		COL_OUT_MIN,
		COL_OUT_MAX,
		COL_BUTTON,

		NUM_COLS
	};
	
	RoutingTableRow(size_t id, QWidget *parent);
	
	virtual QSize sizeHint() const;
	virtual int GetDividerSize() const {return m_Divider->sizeHint().width();}
	virtual void SetAddRemoveText(const QString &text);
	virtual void UpdateLayout(const int *colSizes, size_t count, int margin);
	virtual void Load(const QString &label, const EosRouteSrc &src, const EosRouteDst &dst);
	virtual bool Save(QString &label, EosRouteSrc &src, EosRouteDst &dst) const;
	virtual QWidget* GetWigetForCol(size_t col);
	virtual int GetWidthHintForCol(size_t col) const;
	virtual void SetInItemStateTableId(ItemStateTable::ID itemStateTableId) {m_InItemStateTableId = itemStateTableId;}
	virtual void SetOutItemStateTableId(ItemStateTable::ID itemStateTableId) {m_OutItemStateTableId = itemStateTableId;}
	virtual void UpdateItemState(const ItemStateTable &itemStateTable);

	static void StringToTransform(const QString &str, EosRouteDst::sTransform &transform);
	static void TransformToString(const EosRouteDst::sTransform &transform, QString &str);
	
signals:
	void addRemoveClicked(size_t id);

private slots:
	void onAddRemoveClicked(bool checked);
	
private:
	size_t				m_Id;
	ItemStateTable::ID	m_InItemStateTableId;
	Indicator			*m_InState;
	Indicator			*m_InActivity;
	QLineEdit			*m_Label;
	QLineEdit			*m_InIP;
	QLineEdit			*m_InPort;
	QLineEdit			*m_InPath;
	QLineEdit			*m_InMin;
	QLineEdit			*m_InMax;
	QLabel				*m_Divider;
	ItemStateTable::ID	m_OutItemStateTableId;
	Indicator			*m_OutState;
	Indicator			*m_OutActivity;
	QLineEdit			*m_OutIP;
	QLineEdit			*m_OutPort;
	QLineEdit			*m_OutPath;
	QLineEdit			*m_OutMin;
	QLineEdit			*m_OutMax;
	QPushButton			*m_AddRemove;

protected:
	virtual void UpdateItemState(const ItemState *itemState, Indicator &stateIndicator, Indicator &activityIndicator);
};

////////////////////////////////////////////////////////////////////////////////

class RoutingTable
	: public QWidget
{
	Q_OBJECT
	
public:
	RoutingTable(QWidget *parent);
	
	virtual void Clear();
	virtual void LoadRoutes(const Router::ROUTES &routes);
	virtual void SaveRoutes(Router::ROUTES &routes, ItemStateTable *itemStateTable) const;
	virtual void LoadTcpConnections(const Router::CONNECTIONS &tcpConnections);
	virtual void SaveTcpConnections(Router::CONNECTIONS &tcpConnections, ItemStateTable *itemStateTable);
	virtual bool LoadFromFile(const QString &path);
	virtual bool SaveToFile(const QString &path) const;
	virtual void UpdateItemState(const ItemStateTable &itemStateTable);
	
	static bool HasRoute(const Router::ROUTES &routes, const EosRouteSrc &src, const EosRouteDst &dst);
	static bool HasTcpConnection(const Router::CONNECTIONS &tcpConnections, const EosAddr &addr);
	
signals:
	void changed();
	
public slots:
	void autoSize(int w, int /*h*/) { UpdateLayout(w,/*forResize*/false); }
	void onAddRemoveClicked(size_t id);
	void onTcpClicked(bool checked);
	void onApplyClicked(bool checked);
	
protected:
	typedef std::vector<RoutingTableRow*> ROWS;
	typedef std::map<EosAddr,ItemStateTable::ID> ADDR_STATES;
	
	QLabel			*m_Header[RoutingTableRow::NUM_COLS];
	ROWS			m_Rows;
	QPushButton		*m_Tcp;
	QPushButton		*m_Apply;	
	TcpTableWindow	*m_TcpTableWindow;
	unsigned int	m_UpdatingLayout;
	
	virtual void UpdateLayout(int w, bool forResize);
	virtual RoutingTableRow* CreateRow(size_t id, bool remove, const QString &label, const EosRouteSrc &src, const EosRouteDst &dst);
	virtual void LoadLineFromFile(const QString &line, Router::ROUTES &routes);
	virtual void resizeEvent(QResizeEvent *event);	
};

////////////////////////////////////////////////////////////////////////////////

class MainWindow
	: public QWidget
{
	Q_OBJECT

public:
	MainWindow(EosPlatform *platform, QWidget *parent=0, Qt::WindowFlags f=0);
	virtual ~MainWindow();
	
	virtual QSize sizeHint() const {return QSize(900,500);}
	virtual void FlushLogQ(EosLog::LOG_Q &logQ);
	virtual bool BuildRoutes();

protected:
	virtual void closeEvent(QCloseEvent *event);

private slots:
	void onTick();
	void onRoutingTableChanged();
	void buildRoutes();
	void onNewFileClicked(bool checked);
	void onOpenFileClicked(bool checked);
	void onSaveFileClicked(bool checked);
	void onSaveAsFileClicked(bool checked);
	void onClearLogClicked(bool checked);
	void onOpenLogClicked(bool checked);

private:
	EosLog			m_Log;
	EosLog::LOG_Q	m_TempLogQ;
	ItemStateTable	m_ItemStateTable;
	QListWidget		*m_LogWidget;
	QSettings		m_Settings;
	EosPlatform		*m_pPlatform;
	int				m_LogDepth;
	int				m_FileDepth;
	int				m_FileLineCount;
	unsigned int	m_ReconnectDelay;
	QFile			m_LogFile;
	QTextStream		m_LogStream;
	RoutingTable	*m_RoutingTable;
	RouterThread	*m_RouterThread;
	QString			m_FilePath;
	bool			m_Unsaved;
	bool			m_DisableSystemIdle;

	virtual void Shutdown();
	virtual void GetPersistentSavePath(QString &path) const;
	virtual void UpdateWindowTitle();
	virtual void RestoreLastFile();
	virtual bool LoadFile(const QString &path);
	virtual bool SaveFile(const QString &path);
	virtual void InitLogFile();
	virtual void ShutdownLogFile();
	virtual void FlushRouterThread(bool logsOnly);
	
	static void GetDefaultIP(QString &ip);
};

////////////////////////////////////////////////////////////////////////////////

#endif
