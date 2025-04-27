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
class LogWidget;

////////////////////////////////////////////////////////////////////////////////

class FileUtils
{
public:
  static QString QuotedString(const QString& str);
  static void GetItemsFromQuotedString(const QString& str, QStringList& items);
};

////////////////////////////////////////////////////////////////////////////////

class Indicator : public QWidget
{
  Q_OBJECT

public:
  Indicator(QWidget* parent = nullptr);

  virtual void SetColor(const QColor& color);
  virtual void Activate(unsigned int timeoutMS);
  virtual void Deactivate();
  QSize sizeHint() const override { return QSize(16, 16); }

private slots:
  void onUpdate();

protected:
  enum EnumConstants
  {
    MARGIN = 2
  };

  QColor m_Color;
  QImage m_IconOutline;
  QImage m_IconFill;
  QTimer* m_UpdateTimer;
  unsigned int m_Timeout;
  EosTimer m_Timer;
  qreal m_Opacity;

  virtual void resizeEvent(QResizeEvent* event);
  virtual void paintEvent(QPaintEvent* event);
  virtual void UpdateIcon();
  virtual void SetOpacity(const qreal& opacity);
};

////////////////////////////////////////////////////////////////////////////////

class ScriptEdit : public QTextEdit
{
  Q_OBJECT

public:
  ScriptEdit(QWidget* parent = nullptr);

  QSize sizeHint() const;
  void CheckForErrors();

private slots:
  void onErrorClicked(bool checked);

protected:
  void resizeEvent(QResizeEvent* event) override;

private:
  QPushButton* m_Error;
  QString m_ErrorText;
};

////////////////////////////////////////////////////////////////////////////////

class RoutingButton : public QPushButton
{
  Q_OBJECT

public:
  RoutingButton(const QString& text, size_t id, QWidget* parent = nullptr);

signals:
  void clickedWithId(size_t id);

private slots:
  void onClicked(bool checked);

private:
  size_t m_Id = 0;
};

////////////////////////////////////////////////////////////////////////////////

class RoutingCheckBox : public QCheckBox
{
  Q_OBJECT

public:
  RoutingCheckBox(size_t id, QWidget* parent = nullptr);

signals:
  void toggledWithId(size_t id, bool checked);

private slots:
  void onToggled(bool checked);

private:
  size_t m_Id = 0;
};

////////////////////////////////////////////////////////////////////////////////

class RoutingCol : public QWidget
{
  Q_OBJECT

public:
  enum class Constants
  {
    kSpacing = 4
  };

  typedef std::vector<QWidget*> Widgets;

  RoutingCol(QWidget* parent = nullptr);

  void clear();
  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;
  void AddWidgets(const Widgets& widgets);
  void SetHeight(size_t index, int height);
  int UpdateLayout();
  bool empty() const { return m_Rows.empty(); }

protected:
  void resizeEvent(QResizeEvent* event) override;

private:
  struct Row
  {
    int height;
    Widgets widgets;
  };

  typedef std::vector<Row> Rows;
  Rows m_Rows;
};

////////////////////////////////////////////////////////////////////////////////

class TcpWidget : public QWidget
{
  Q_OBJECT

public:
  TcpWidget(QWidget* parent = nullptr);

  QSize sizeHint() const { return QSize(1000, 1000); }
  void Clear();
  void Load(const QStringList& lines);
  void LoadConnections(const Router::CONNECTIONS& connections);
  void Save(QTextStream& stream);
  void SaveConnections(Router::CONNECTIONS& connections, ItemStateTable* itemStateTable);
  void UpdateItemState(const ItemStateTable& itemStateTable);

protected:
  void resizeEvent(QResizeEvent* event) override;
  void showEvent(QShowEvent* event) override;
  void paintEvent(QPaintEvent* event) override;

private slots:
  void updateHeaders();
  void onAddRemoveClicked(size_t id);

private:
  enum class Col
  {
    kLabel = 0,
    kState,
    kActivity,
    kMode,
    kFraming,
    kIP,
    kPort,
    kButton,

    kCount
  };

  struct Row
  {
    size_t id = 0;
    ItemStateTable::ID itemStateTableId = ItemStateTable::sm_Invalid_Id;
    QLineEdit* label = nullptr;
    Indicator* state = nullptr;
    Indicator* activity = nullptr;
    QComboBox* mode = nullptr;
    QComboBox* framing = nullptr;
    QLineEdit* ip = nullptr;
    QLineEdit* port = nullptr;
    RoutingButton* addRemove = nullptr;
  };

  typedef std::vector<Row> Rows;

  Rows m_Rows;
  QLabel* m_Headers[static_cast<int>(Col::kCount)];
  QScrollArea* m_Scroll = nullptr;
  QSplitter* m_Cols = nullptr;

  void LoadLine(const QString& line, Router::CONNECTIONS& connections);
  void AddRow(size_t id, bool remove, const Router::sConnection& connection);
  void AddCol(int index, QWidget* w, int fixedW = -1);
  void UpdateLayout();
  QRect RectForCol(Col col) const;

  static QString HeaderForCol(Col col);
  static bool HasConnection(const Router::CONNECTIONS& connections, const EosAddr& addr);
};

////////////////////////////////////////////////////////////////////////////////

class ProtocolComboBox : public QComboBox
{
  Q_OBJECT

public:
  ProtocolComboBox(size_t row, Protocol protocol, QWidget* parent = nullptr);

  Protocol GetProtocol() const;

  static QString ProtocolName(Protocol protocol);
  static Protocol SanitizedProtocol(int protocol);

signals:
  void protocolChanged(size_t row, Protocol protocol);

private slots:
  void onCurrentIndexChanged(int index);

private:
  size_t m_Row = 0;
};

////////////////////////////////////////////////////////////////////////////////

class RoutingWidget : public QWidget
{
  Q_OBJECT

public:
  RoutingWidget(QWidget* parent = nullptr);

  QSize sizeHint() const { return QSize(1000, 1000); }
  void Clear();
  void Load(const QStringList& lines);
  void LoadRoutes(const Router::ROUTES& routes);
  void Save(QTextStream& stream);
  void SaveRoutes(Router::ROUTES& routes, ItemStateTable* itemStateTable);
  void UpdateItemState(const ItemStateTable& itemStateTable);

  static void StringToTransform(const QString& str, EosRouteDst::sTransform& transform);
  static void TransformToString(const EosRouteDst::sTransform& transform, QString& str);

protected:
  void resizeEvent(QResizeEvent* event) override;
  void showEvent(QShowEvent* event) override;
  void paintEvent(QPaintEvent* event) override;

private slots:
  void updateHeaders();
  void onOutScriptToggled(size_t id, bool checked);
  void onAddRemoveClicked(size_t id);
  void onInProtocolChanged(size_t row, Protocol protocol);
  void onOutProtocolChanged(size_t row, Protocol protocol);

private:
  enum class Col
  {
    kLabel = 0,

    kInState,
    kInActivity,
    kInIP,
    kInPort,
    kInProtocol,
    kInPath,
    kInMin,
    kInMax,

    kDivider,

    kOutState,
    kOutActivity,
    kOutIP,
    kOutPort,
    kOutProtocol,
    kOutPath,
    kOutScript,
    kOutMin,
    kOutMax,

    kButton,

    kCount
  };

  struct Row
  {
    size_t id = 0;
    ItemStateTable::ID inItemStateTableId = ItemStateTable::sm_Invalid_Id;
    QLineEdit* label = nullptr;
    Indicator* inState = nullptr;
    Indicator* inActivity = nullptr;
    QLineEdit* inIP = nullptr;
    QLineEdit* inPort = nullptr;
    ProtocolComboBox* inProtocol = nullptr;
    QLineEdit* inPath = nullptr;
    QLineEdit* inMin = nullptr;
    QLineEdit* inMax = nullptr;
    QLabel* divider = nullptr;
    ItemStateTable::ID outItemStateTableId = ItemStateTable::sm_Invalid_Id;
    Indicator* outState = nullptr;
    Indicator* outActivity = nullptr;
    QLineEdit* outIP = nullptr;
    QLineEdit* outPort = nullptr;
    ProtocolComboBox* outProtocol = nullptr;
    QLineEdit* outPath = nullptr;
    ScriptEdit* outScriptText = nullptr;
    RoutingCheckBox* outScript = nullptr;
    QLineEdit* outMin = nullptr;
    QLineEdit* outMax = nullptr;
    RoutingButton* addRemove = nullptr;
  };

  typedef std::vector<Row> Rows;
  typedef std::map<EosAddr, ItemStateTable::ID> AddrStates;

  Rows m_Rows;
  QLabel* m_Incoming = nullptr;
  QLabel* m_Outgoing = nullptr;
  QLabel* m_Headers[static_cast<int>(Col::kCount)];
  QScrollArea* m_Scroll = nullptr;
  QSplitter* m_Cols = nullptr;

  void LoadLine(const QString& line, Router::ROUTES& routes);
  void AddRow(size_t id, bool remove, const QString& label, const EosRouteSrc& src, const EosRouteDst& dst);
  void AddCol(int index, QWidget* w, bool fixed = false);
  void AddCol(int index, const RoutingCol::Widgets& w, bool fixed = false);
  void UpdateItemState(const ItemState* itemState, Indicator& stateIndicator, Indicator& activityIndicator);
  void UpdateLayout();
  QRect RectForCol(Col col) const;

  static QString HeaderForCol(Col col);
  static bool HasRoute(const Router::ROUTES& routes, const EosRouteSrc& src, const EosRouteDst& dst);
};

////////////////////////////////////////////////////////////////////////////////

class MainWindow : public QWidget
{
  Q_OBJECT

public:
  MainWindow(EosPlatform* platform, QWidget* parent = 0, Qt::WindowFlags f = Qt::WindowFlags());
  virtual ~MainWindow();

  QSize sizeHint() const override { return QSize(1280, 640); }
  void FlushLogQ(EosLog::LOG_Q& logQ);
  bool BuildRoutes();

protected:
  void closeEvent(QCloseEvent* event) override;

private slots:
  void onTick();
  void buildRoutes();
  void onNewFile();
  void onOpenFile();
  void onSaveFile();
  void onSaveAsFile();
  void onClearLog();
  void onOpenLog();
  void onApplyClicked(bool checked);

private:
  EosLog m_Log;
  EosLog::LOG_Q m_TempLogQ;
  ItemStateTable m_ItemStateTable;
  LogWidget* m_LogWidget;
  QSettings m_Settings;
  EosPlatform* m_pPlatform;
  int m_FileDepth;
  int m_FileLineCount;
  unsigned int m_ReconnectDelay;
  QFile m_LogFile;
  QTextStream m_LogStream;
  RoutingWidget* m_RoutingWidget;
  TcpWidget* m_TcpWidget;
  RouterThread* m_RouterThread;
  QString m_FilePath;
  bool m_Unsaved;
  bool m_DisableSystemIdle;

  void Shutdown();
  void GetPersistentSavePath(QString& path) const;
  void UpdateWindowTitle();
  void RestoreLastFile();
  bool LoadFile(const QString& path);
  bool SaveFile(const QString& path);
  void InitLogFile();
  void ShutdownLogFile();
  void FlushRouterThread(bool logsOnly);
  bool Load(const QString& path);
  bool Save(const QString& path);
  bool ResolveUnsaved();

  static void GetDefaultIP(QString& ip);
};

////////////////////////////////////////////////////////////////////////////////
