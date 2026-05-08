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
  virtual void SetTcpBadge(bool b);
  QSize sizeHint() const override { return QSize(20, 20); }

private slots:
  void onUpdate();

protected:
  QColor m_Color;
  QImage m_IconOutline;
  QImage m_IconFill;
  QTimer* m_UpdateTimer;
  unsigned int m_Timeout;
  EosTimer m_Timer;
  qreal m_Opacity;
  bool m_TcpBadge;

  virtual void resizeEvent(QResizeEvent* event);
  virtual void paintEvent(QPaintEvent* event);
  virtual void UpdateIcon();
  virtual void SetOpacity(const qreal& opacity);
};

////////////////////////////////////////////////////////////////////////////////

class LineEdit : public QLineEdit
{
  Q_OBJECT

public:
  explicit LineEdit(QWidget* parent = nullptr);
  explicit LineEdit(const QString& contents, QWidget* parent = nullptr);

  QSize sizeHint() const override;
};

////////////////////////////////////////////////////////////////////////////////

class ScriptEdit : public QTextEdit
{
  Q_OBJECT

public:
  ScriptEdit(QWidget* parent = nullptr);

  QSize sizeHint() const;
  void CheckForErrors();
  void SetGlobals(ScriptEdit* globals) { m_Globals = globals; }

private slots:
  void onErrorClicked(bool checked);

protected:
  void resizeEvent(QResizeEvent* event) override;

private:
  QPushButton* m_Error;
  QString m_ErrorText;
  QPointer<ScriptEdit> m_Globals;
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

class RoutingCheckBox : public QAbstractButton
{
  Q_OBJECT

public:
  enum class Style
  {
    Normal,
    Mute
  };

  explicit RoutingCheckBox(size_t id, QWidget* parent = nullptr);
  explicit RoutingCheckBox(Style checkBoxStyle, size_t id, QWidget* parent = nullptr);

  QSize sizeHint() const override { return QSize(24, 24); }
  QSize minimumSizeHint() const override { return sizeHint(); }

  const QIcon& GetIcon(bool checked) const { return GetIcon(m_Style, checked); }
  static const QIcon& GetIcon(Style checkBoxStyle, bool checked);

signals:
  void toggledWithId(size_t id, bool checked);

private slots:
  void onToggled(bool checked);

protected:
  void paintEvent(QPaintEvent* event) override;

private:
  Style m_Style = Style::Normal;
  size_t m_Id = 0;
  QPixmap m_Unchecked;
  QPixmap m_Checked;

  void Construct();
};

////////////////////////////////////////////////////////////////////////////////

class SplitterHandle : public QSplitterHandle
{
  Q_OBJECT

public:
  SplitterHandle(Qt::Orientation orientation, QSplitter* parent);

signals:
  void autoSize(SplitterHandle* splitterHandle);

protected:
  void mouseDoubleClickEvent(QMouseEvent* event) override;
};

////////////////////////////////////////////////////////////////////////////////

class Splitter : public QSplitter
{
  Q_OBJECT

public:
  explicit Splitter(QWidget* parent = nullptr);
  explicit Splitter(Qt::Orientation orientation, QWidget* parent = nullptr);

public slots:
  void autoSize(SplitterHandle* splitterHandle);

protected:
  QSplitterHandle* createHandle() override;
};

////////////////////////////////////////////////////////////////////////////////

class RoutingCol : public QWidget
{
  Q_OBJECT

public:
  enum class Constants
  {
    kSpacing = 4,
    kLastRowGap = 16
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
  void ResetCachedSizeHints();

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
  mutable QSize m_CachedSizeHint;
  mutable QSize m_CachedMinimumSizeHint;
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

signals:
  void refreshTcpBadges();

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
    LineEdit* label = nullptr;
    Indicator* state = nullptr;
    Indicator* activity = nullptr;
    QComboBox* mode = nullptr;
    QComboBox* framing = nullptr;
    LineEdit* ip = nullptr;
    LineEdit* port = nullptr;
    RoutingButton* addRemove = nullptr;
  };

  typedef std::vector<Row> Rows;

  Rows m_Rows;
  QLabel* m_Headers[static_cast<int>(Col::kCount)];
  QScrollArea* m_Scroll = nullptr;
  Splitter* m_Cols = nullptr;
  RoutingCol* m_RoutingCols[static_cast<int>(Col::kCount)];

  void LoadLine(const QString& line, Router::CONNECTIONS& connections);
  void AddRow(size_t id, bool remove, const Router::sConnection& connection);
  void AddCol(int index, QWidget* w, int fixedW = -1);
  void UpdateLayout();
  QRect RectForCol(Col col) const;

  static QString HeaderForCol(Col col);
  static bool HasConnection(const Router::CONNECTIONS& connections, const EosAddr& addr);
};

////////////////////////////////////////////////////////////////////////////////

class SettingsWidget : public QWidget
{
  Q_OBJECT

public:
  SettingsWidget(QSettings& settings, QWidget* parent = nullptr);

  void Clear();
  void Load(const QStringList& lines);
  void LoadSettings(const Router::Settings& settings);
  void Save(QTextStream& stream);
  void SaveSettings(Router::Settings& settings);
  ScriptEdit* GetScript() const { return m_Script; }

protected:
  void showEvent(QShowEvent* event) override;

private slots:
  void onAutoStartToggled(bool checked);
  void onCurrentIndexChanged(int index);
  void refreshMIDIDevices();

private:
  struct OTP
  {
    QCheckBox* pos = nullptr;
    QCheckBox* posVelAccel = nullptr;
    QCheckBox* rot = nullptr;
    QCheckBox* rotVelAccel = nullptr;
    QCheckBox* scale = nullptr;
    QCheckBox* rf = nullptr;
  };

  enum class MIDIProp
  {
    kType = 0,
    kName,
    kPort,

    kCount
  };

  struct MIDIDevice
  {
    std::array<QString, static_cast<size_t>(MIDIProp::kCount)> props;
    QColor color;
  };

  typedef std::vector<MIDIDevice> MIDIDeviceList;

  QSettings& m_Settings;
  QScrollArea* m_Scroll = nullptr;
  QComboBox* m_sACNInterface = nullptr;
  QComboBox* m_ArtNetInterface = nullptr;
  QCheckBox* m_LevelChangesOnly = nullptr;
  QComboBox* m_OTPInterface = nullptr;
  std::array<QCheckBox*, static_cast<size_t>(otp::ModuleType::kCount)> m_OTPModules;
  ScriptEdit* m_Script = nullptr;
  QTableWidget* m_MIDI = nullptr;

  void LoadLine(const QString& line, Router::Settings& settings);

  static void PopulateInterfaces(QComboBox* combo, const QString& defaultText);
  static QString GetInterface(QComboBox* combo);
  static void SetInterface(QComboBox* combo, const QString& ip);
  static QString MIDIPropName(MIDIProp prop);
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
  void LoadRoutes(const Router::ROUTES& routes, const ItemStateTable& itemStateTable);
  void Save(QTextStream& stream);
  void SaveRoutes(Router::ROUTES& routes, ItemStateTable& itemStateTable);
  void UpdateItemState(const ItemStateTable& itemStateTable, bool tcpBadge = false);
  void SetGlobals(ScriptEdit* globals) { m_Globals = globals; }

  static void StringToTransform(const QString& str, EosRouteDst::sTransform& transform);
  static void TransformToString(const EosRouteDst::sTransform& transform, QString& str);
  static QString GetHelpText(bool incoming) { return GetHelpText(incoming ? Col::kInPath : Col::kOutPath, Protocol::kInvalid, Protocol::kInvalid, /*script*/ true); }

signals:
  void muteToggled(size_t id, bool checked);
  void muteRouteToggled(size_t id, bool checked);
  void refreshTcpBadges();

private slots:
  void updateHeaders();
  void onEnableToggled(size_t, bool checked);
  void onMuteToggled(size_t id, bool checked);
  void onMuteRouteToggled(size_t row, bool checked);
  void onOutScriptToggled(size_t id, bool checked);
  void onAddRemoveClicked(size_t id);
  void onInProtocolChanged(size_t row, Protocol protocol);
  void onOutProtocolChanged(size_t row, Protocol protocol);
  void onHeaderHelpClicked(size_t id);

protected:
  void resizeEvent(QResizeEvent* event) override;
  void showEvent(QShowEvent* event) override;
  void paintEvent(QPaintEvent* event) override;

private:
  enum class Col
  {
    kEnable = 0,
    kMute,

    kLabel,

    kInState,
    kInActivity,
    kInProtocol,
    kInIP,
    kInPort,
    kInPath,
    kInMin,
    kInMax,

    kDivider,

    kOutState,
    kOutActivity,
    kOutProtocol,
    kOutIP,
    kOutPort,
    kOutPath,
    kOutScript,
    kOutMin,
    kOutMax,
    kMaxRateHz,

    kButton,

    kCount
  };

  struct Header
  {
    QWidget* base = nullptr;
    RoutingCheckBox* mute = nullptr;
  };

  struct Row
  {
    size_t id = 0;
    ItemStateTable::ID inItemStateTableId = ItemStateTable::sm_Invalid_Id;
    RoutingCheckBox* enable = nullptr;
    RoutingCheckBox* mute = nullptr;
    LineEdit* label = nullptr;
    Indicator* inState = nullptr;
    Indicator* inActivity = nullptr;
    LineEdit* inIP = nullptr;
    LineEdit* inPort = nullptr;
    ProtocolComboBox* inProtocol = nullptr;
    LineEdit* inPath = nullptr;
    LineEdit* inMin = nullptr;
    LineEdit* inMax = nullptr;
    QLabel* divider = nullptr;
    ItemStateTable::ID outItemStateTableId = ItemStateTable::sm_Invalid_Id;
    Indicator* outState = nullptr;
    Indicator* outActivity = nullptr;
    LineEdit* outIP = nullptr;
    LineEdit* outPort = nullptr;
    ProtocolComboBox* outProtocol = nullptr;
    LineEdit* outPath = nullptr;
    ScriptEdit* outScriptText = nullptr;
    RoutingCheckBox* outScript = nullptr;
    LineEdit* outMin = nullptr;
    LineEdit* outMax = nullptr;
    LineEdit* maxRateHz = nullptr;
    RoutingButton* addRemove = nullptr;
  };

  struct HelpDialog
  {
    QWidget* dialog = nullptr;
    QTextEdit* edit = nullptr;
  };

  typedef std::vector<Row> Rows;
  typedef std::map<EosAddr, ItemStateTable::ID> AddrStates;

  Rows m_Rows;
  Header m_Incoming;
  Header m_Outgoing;
  QWidget* m_Headers[static_cast<int>(Col::kCount)];
  QScrollArea* m_Scroll = nullptr;
  Splitter* m_Cols = nullptr;
  RoutingCol* m_RoutingCols[static_cast<int>(Col::kCount)];
  HelpDialog m_Help;
  QPointer<ScriptEdit> m_Globals;

  void LoadLine(const QString& line, Router::ROUTES& routes, ItemStateTable& itemStateTable);
  void AddRow(size_t id, bool remove, const QString& label, const Router::sRoute& route);
  void AddCol(int index, QWidget* w, bool fixed = false, bool fixedHeight = false);
  void AddCol(int index, const RoutingCol::Widgets& w, bool fixed = false, bool fixedHeight = false);
  void UpdateItemState(const ItemState* itemState, Indicator& stateIndicator, Indicator& activityIndicator, bool tcpBadge);
  void UpdateLayout();
  QRect RectForCol(Col col) const;
  void UpdateEnableState();
  void UpdateMuteState();

  static QString HeaderForCol(Col col);
  static bool HasRoute(const Router::ROUTES& routes, const EosRouteSrc& src, const EosRouteDst& dst);
  static QString GetHelpText(Col col, Protocol inProtocol, Protocol outProtocol, bool script);
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
  void onOpenLog();
  void onViewHelp();
  void onAboutHelp();
  void onStartClicked(bool checked);
  void onStopClicked(bool checked);
  void onMuteToggled(bool incoming, bool checked);
  void onMuteRouteToggled(size_t id, bool checked);
  void refreshTcpBadges();

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
  SettingsWidget* m_SettingsWidget;
  QPushButton* m_StartButton = nullptr;
  QPushButton* m_StopButton = nullptr;
  RouterThread* m_RouterThread;
  QString m_FilePath;
  bool m_Unsaved;
  QByteArray m_FileContents;
  bool m_DisableSystemIdle;
  QWidget* m_Help = nullptr;
  QWidget* m_About = nullptr;

  void Shutdown();
  void GetPersistentSavePath(QString& path) const;
  void UpdateWindowTitle();
  void RestoreLastFile();
  bool LoadFile(const QString& path);
  bool SaveFile(const QString& path);
  void InitLogFile();
  void ShutdownLogFile();
  void SyncRouterThread(bool logsOnly);
  bool Load(const QString& path);
  bool SaveToDevice(QIODevice& device);
  bool SaveToBuffer(QByteArray& buffer);
  bool SaveToFile(const QString& path);
  bool ResolveUnsaved();
  void SetUnsaved(bool unsaved);
  void UpdateUnsaved();
};

////////////////////////////////////////////////////////////////////////////////
