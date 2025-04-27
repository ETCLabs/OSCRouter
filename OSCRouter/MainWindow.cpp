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

#include "MainWindow.h"
#include "NetworkUtils.h"
#include "EosPlatform.h"
#include "LogWidget.h"
#include <time.h>

#ifdef WIN32
#include <Windows.h>
#include "resource.h"
#endif

// must be last include
#include "LeakWatcher.h"

////////////////////////////////////////////////////////////////////////////////

#define APP_VERSION "0.20"
#define SETTING_LOG_DEPTH "LogDepth"
#define SETTING_FILE_DEPTH "FileDepth"
#define SETTING_LAST_FILE "LastFile"
#define SETTING_RECONNECT_DELAY "ReconnectDelay"
#define SETTING_DISABLE_SYSTEM_IDLE "DisableSystemIdle"
#define ACTIVITY_TIMEOUT_MS 300

////////////////////////////////////////////////////////////////////////////////

QString FileUtils::QuotedString(const QString& str)
{
  // "test" -> """test"""
  // test,  -> "test,"

  QString quoted(str);
  quoted.replace("\"", "\"\"");
  if (quoted.contains('\"') || quoted.contains(','))
  {
    quoted.prepend("\"");
    quoted.append("\"");
  }

  quoted.replace("\n", "\\n");

  return quoted;
}

void FileUtils::GetItemsFromQuotedString(const QString& str, QStringList& items)
{
  items.clear();

  int len = str.size();
  int index = 0;
  bool quoted = false;
  for (int i = 0; i <= len; i++)
  {
    if (i >= len || (str[i] == QChar(',') && !quoted))
    {
      int itemLen = (i - index);
      if (itemLen > 0)
      {
        QString item(str.mid(index, itemLen).trimmed());

        // remove quotes
        if (item.startsWith('\"') && item.endsWith('\"'))
        {
          itemLen = (item.size() - 2);
          if (itemLen > 0)
            item = item.mid(1, itemLen);
          else
            item.clear();
        }

        // fix quoted quotes
        item.replace("\"\"", "\"");

        // replace newlines
        item.replace("\\n", "\n");

        items.push_back(item);
      }
      else
        items.push_back(QString());

      index = (i + 1);
    }
    else if (str[i] == QChar('\"'))
    {
      if (!quoted)
        quoted = true;
      else if ((i + 1) >= len || str[i + 1] != QChar('\"'))
        quoted = false;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////

Indicator::Indicator(QWidget* parent /*= nullptr*/)
  : QWidget(parent)
  , m_Color(MUTED_COLOR)
  , m_UpdateTimer(0)
  , m_Timeout(0)
  , m_Opacity(0)
{
}

void Indicator::Activate(unsigned int timeoutMS)
{
  SetOpacity(1.0);

  m_Timeout = timeoutMS;

  if (m_Timeout == 0)
  {
    if (m_UpdateTimer)
      m_UpdateTimer->stop();
  }
  else
  {
    if (!m_UpdateTimer)
    {
      m_UpdateTimer = new QTimer(this);
      connect(m_UpdateTimer, &QTimer::timeout, this, &Indicator::onUpdate);
    }

    m_Timer.Start();
    m_UpdateTimer->start(16);
  }
}

void Indicator::Deactivate()
{
  if (m_UpdateTimer)
    m_UpdateTimer->stop();

  SetOpacity(0);
}

void Indicator::SetOpacity(const qreal& opacity)
{
  if (m_Opacity != opacity)
  {
    m_Opacity = opacity;
    update();
  }
}

void Indicator::SetColor(const QColor& color)
{
  if (m_Color != color)
  {
    m_Color = color;
    UpdateIcon();
  }
}

void Indicator::resizeEvent(QResizeEvent* event)
{
  QWidget::resizeEvent(event);
  UpdateIcon();
}

void Indicator::paintEvent(QPaintEvent* /*event*/)
{
  if (!m_IconOutline.isNull() && !m_IconFill.isNull())
  {
    qreal dpr = devicePixelRatioF();
    if (dpr > 0 && !qFuzzyCompare(m_IconOutline.devicePixelRatioF(), dpr))
      UpdateIcon();

    QPainter painter(this);

    QSize iconSize = m_IconOutline.size();
    if (dpr > 0)
      iconSize /= dpr;

    if (m_Opacity > 0)
    {
      painter.setOpacity(m_Opacity);
      painter.drawImage((width() - iconSize.width()) * 0.5, (height() - iconSize.height()) * 0.5, m_IconFill);
      painter.setOpacity(1.0);
    }

    painter.drawImage((width() - iconSize.width()) * 0.5, (height() - iconSize.height()) * 0.5, m_IconOutline);
  }
}

void Indicator::UpdateIcon()
{
  m_IconOutline = QImage();
  m_IconFill = QImage();

  if (m_Color.alpha() > 0)
  {
    QRect r(rect());
    r.adjust(MARGIN, MARGIN, -MARGIN, -MARGIN);
    int size = qMin(r.width(), r.height());

    qreal dpr = devicePixelRatioF();
    if (dpr <= 0)
      dpr = 1;
    size = qRound(size * dpr);

    if (size > 2)
    {
      m_IconOutline = QImage(size, size, QImage::Format_ARGB32);
      m_IconOutline.setDevicePixelRatio(1);
      m_IconOutline.fill(0);

      QPainter painter;
      if (painter.begin(&m_IconOutline))
      {
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(m_Color, 1.5));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(QRect(1, 1, size - 2, size - 2));
        painter.end();

        m_IconOutline.setDevicePixelRatio(dpr);

        m_IconFill = QImage(size, size, QImage::Format_ARGB32);
        m_IconFill.fill(0);
        if (painter.begin(&m_IconFill))
        {
          painter.setRenderHint(QPainter::Antialiasing);
          painter.setPen(Qt::NoPen);
          painter.setBrush(m_Color);
          painter.drawEllipse(QRect(1, 1, size - 2, size - 2));
          painter.end();

          m_IconFill.setDevicePixelRatio(dpr);
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

void Indicator::onUpdate()
{
  unsigned int elapsed = m_Timer.GetElapsed();
  if (elapsed >= m_Timeout)
  {
    SetOpacity(0);
    if (m_UpdateTimer)
      m_UpdateTimer->stop();
  }
  else
    SetOpacity(1.0 - elapsed / static_cast<qreal>(m_Timeout));
}

////////////////////////////////////////////////////////////////////////////////

ScriptEdit::ScriptEdit(QWidget* parent /*= nullptr*/)
  : QTextEdit(parent)
{
  setAcceptRichText(false);
  setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  setWordWrapMode(QTextOption::NoWrap);
  setLineWrapMode(QTextEdit::NoWrap);
  setMaximumHeight(70);

  m_Error = new QPushButton(tr("!"), this);
  int s = m_Error->sizeHint().height();
  m_Error->resize(s, s);
  m_Error->setStyleSheet(QLatin1String("QPushButton {background-color: #ff244f; color: #ffffff; font-weight: bold;}"));
  m_Error->hide();
  connect(m_Error, &QPushButton::clicked, this, &ScriptEdit::onErrorClicked);
}

QSize ScriptEdit::sizeHint() const
{
  static QSize kSizeHint;
  if (kSizeHint.isEmpty())
  {
    QLineEdit* temp = new QLineEdit();
    kSizeHint = temp->sizeHint();
    kSizeHint.setHeight(kSizeHint.height() * 3);
    temp->deleteLater();
  }

  return kSizeHint;
}

void ScriptEdit::CheckForErrors()
{
  m_ErrorText = ScriptEngine().evaluate(toPlainText());
  m_Error->setVisible(!m_ErrorText.isEmpty());
}

void ScriptEdit::onErrorClicked(bool /*checked*/)
{
  CheckForErrors();

  if (!m_ErrorText.isEmpty())
    QMessageBox::critical(this, tr("JavaScript Error"), m_ErrorText);
}

void ScriptEdit::resizeEvent(QResizeEvent* event)
{
  QTextEdit::resizeEvent(event);

  const int kMargin = 4;
  m_Error->move(width() - m_Error->width() - kMargin, kMargin);
}

////////////////////////////////////////////////////////////////////////////////

RoutingButton::RoutingButton(const QString& text, size_t id, QWidget* parent /*= nullptr*/)
  : QPushButton(text, parent)
  , m_Id(id)
{
  connect(this, &QPushButton::clicked, this, &RoutingButton::onClicked);
}

void RoutingButton::onClicked(bool /*checked*/)
{
  emit clickedWithId(m_Id);
}

////////////////////////////////////////////////////////////////////////////////

RoutingCheckBox::RoutingCheckBox(size_t id, QWidget* parent /*= nullptr*/)
  : QCheckBox(parent)
  , m_Id(id)
{
  connect(this, &QCheckBox::toggled, this, &RoutingCheckBox::onToggled);
}

void RoutingCheckBox::onToggled(bool checked)
{
  emit toggledWithId(m_Id, checked);
}

////////////////////////////////////////////////////////////////////////////////

RoutingCol::RoutingCol(QWidget* parent /*= nullptr*/)
  : QWidget(parent)
{
}

void RoutingCol::clear()
{
  for (size_t row = 0; row < m_Rows.size(); ++row)
  {
    const Widgets& widgets = m_Rows[row].widgets;
    for (size_t w = 0; w < widgets.size(); ++w)
    {
      widgets[w]->hide();
      widgets[w]->deleteLater();
    }
  }

  m_Rows.clear();
}

QSize RoutingCol::sizeHint() const
{
  QSize sh;

  for (size_t row = 0; row < m_Rows.size(); ++row)
  {
    const Widgets& widgets = m_Rows[row].widgets;
    for (size_t w = 0; w < widgets.size(); ++w)
    {
      if (!widgets[w]->isHidden())
      {
        sh.setWidth(qMax(sh.width(), widgets[w]->sizeHint().width()));
        sh.setHeight(sh.height() + static_cast<int>(Constants::kSpacing) + widgets[w]->sizeHint().height());
      }
    }
  }

  return sh;
}

QSize RoutingCol::minimumSizeHint() const
{
  QSize sh;

  for (size_t row = 0; row < m_Rows.size(); ++row)
  {
    const Widgets& widgets = m_Rows[row].widgets;
    for (size_t w = 0; w < widgets.size(); ++w)
    {
      if (!widgets[w]->isHidden())
      {
        sh.setWidth(qMax(sh.width(), widgets[w]->minimumSizeHint().width()));
        sh.setHeight(sh.height() + static_cast<int>(Constants::kSpacing) + widgets[w]->minimumSizeHint().height());
      }
    }
  }

  return sh;
}

void RoutingCol::AddWidgets(const Widgets& widgets)
{
  for (size_t i = 0; i < widgets.size(); ++i)
    widgets[i]->show();

  m_Rows.push_back({0, widgets});
}

void RoutingCol::SetHeight(size_t index, int height)
{
  if (index < m_Rows.size())
    m_Rows[index].height = height;
}

void RoutingCol::resizeEvent(QResizeEvent* /*event*/)
{
  UpdateLayout();
}

int RoutingCol::UpdateLayout()
{
  int y = static_cast<int>(Constants::kSpacing);

  for (size_t row = 0; row < m_Rows.size(); ++row)
  {
    Widgets& widgets = m_Rows[row].widgets;
    for (size_t w = 0; w < widgets.size(); ++w)
      widgets[w]->setGeometry(0, y, width(), widgets[w]->height());

    y += m_Rows[row].height;
  }

  return y;
}

////////////////////////////////////////////////////////////////////////////////

TcpWidget::TcpWidget(QWidget* parent /*= nullptr*/)
  : QWidget(parent)
{
  for (int i = 0; i < static_cast<int>(Col::kCount); ++i)
  {
    m_Headers[i] = new QLabel(HeaderForCol(static_cast<Col>(i)), this);
    m_Headers[i]->setAlignment(Qt::AlignCenter);
  }

  m_Scroll = new QScrollArea(this);
  m_Scroll->setWidgetResizable(true);

  m_Cols = new QSplitter(m_Cols);
  m_Scroll->setWidget(m_Cols);
  m_Cols->show();
  for (int i = 0; i < static_cast<int>(Col::kCount); ++i)
  {
    RoutingCol* col = new RoutingCol(m_Cols);
    m_Cols->addWidget(col);
    m_Cols->setCollapsible(i, false);
    m_Cols->setStretchFactor(i, 1);
    col->show();

    switch (static_cast<Col>(i))
    {
      case Col::kLabel:
      case Col::kIP: m_Cols->setStretchFactor(i, 3); break;
    }
  }

  connect(m_Cols, &QSplitter::splitterMoved, this, &TcpWidget::updateHeaders);

  Clear();
  UpdateLayout();
}

void TcpWidget::Clear()
{
  m_Rows.clear();

  for (int i = 0; i < m_Cols->count(); ++i)
  {
    RoutingCol* col = qobject_cast<RoutingCol*>(m_Cols->widget(i));
    if (col)
      col->clear();
  }
}

QString TcpWidget::HeaderForCol(Col col)
{
  switch (col)
  {
    case Col::kLabel: return tr("Name");
    case Col::kMode: return tr("Mode");
    case Col::kFraming: return tr("Framing");
    case Col::kIP: return tr("IP");
    case Col::kPort: return tr("Port");
  }

  return QString();
}

void TcpWidget::LoadConnections(const Router::CONNECTIONS& connections)
{
  Clear();

  size_t id = 0;
  m_Rows.reserve(connections.size());
  for (Router::CONNECTIONS::const_iterator i = connections.begin(); i != connections.end(); i++)
    AddRow(id++, /*remove*/ true, *i);

  Router::sConnection empty;
  AddRow(id++, /*remove*/ false, empty);

  UpdateLayout();
}

void TcpWidget::AddRow(size_t id, bool remove, const Router::sConnection& connection)
{
  int col = 0;

  Row row;
  row.id = id;
  row.label = new QLineEdit(connection.label, m_Cols->widget(col));
  row.label->setToolTip(tr("Text label for this TCP connection"));
  AddCol(col++, row.label);

  row.state = new Indicator(m_Cols->widget(col));
  row.state->setToolTip(tr("Status"));
  row.state->SetColor(MUTED_COLOR);
  row.state->Deactivate();
  row.state->setMinimumSize(16, 16);
  row.state->setMaximumWidth(16);
  AddCol(col++, row.state, row.state->sizeHint().height());

  row.activity = new Indicator(m_Cols->widget(col));
  row.activity->setToolTip(tr("Activity"));
  row.activity->SetColor(MUTED_COLOR);
  row.activity->Deactivate();
  AddCol(col++, row.activity, row.activity->sizeHint().height());

  row.mode = new QComboBox(m_Cols->widget(col));
  row.mode->setToolTip(tr("Server: create a server and accept incoming TCP connections\n\nClient: connect to a TCP server"));
  row.mode->addItem(tr("Server"));
  row.mode->addItem(tr("Client"));
  row.mode->setCurrentIndex(connection.server ? 0 : 1);
  AddCol(col++, row.mode, row.mode->sizeHint().width());

  row.framing = new QComboBox(m_Cols->widget(col));
  row.framing->setToolTip(tr("OSC 1.0: packets framed by 4-byte packet size header\n\nOSC 1.1: packets framed by SLIP (RFC 1055)"));
  for (int i = 0; i < OSCStream::FRAME_MODE_COUNT; i++)
  {
    QString name;
    switch (i)
    {
      case OSCStream::FRAME_MODE_1_0: name = tr("OSC 1.0"); break;
      case OSCStream::FRAME_MODE_1_1: name = tr("OSC 1.1"); break;
    }

    row.framing->addItem(name);
  }
  row.framing->setCurrentIndex(connection.frameMode);
  AddCol(col++, row.framing, row.framing->sizeHint().width());

  row.ip = new QLineEdit(m_Cols->widget(col));
  row.ip->setToolTip(tr("Server: local network interface for TCP server to run on\n\nClient: IP address of TCP server to connect to"));
  row.ip->setText(connection.addr.ip);
  AddCol(col++, row.ip);

  row.port = new QLineEdit(m_Cols->widget(col));
  row.port->setToolTip(tr("Server: local network interface for TCP server to run on\n\nClient: IP address of TCP server to connect to"));
  row.port->setText((connection.addr.port == 0) ? QString() : QString::number(connection.addr.port));
  AddCol(col++, row.port);

  row.addRemove = new RoutingButton(remove ? QLatin1String("-") : QLatin1String("+"), id, m_Cols->widget(col));
  row.addRemove->setToolTip(tr("Add/Remove this route"));
  connect(row.addRemove, &RoutingButton::clickedWithId, this, &TcpWidget::onAddRemoveClicked);
  AddCol(col++, row.addRemove, row.addRemove->sizeHint().height());

  m_Rows.push_back(row);
}

void TcpWidget::AddCol(int index, QWidget* w, int fixedW /*= -1*/)
{
  RoutingCol* col = qobject_cast<RoutingCol*>(m_Cols->widget(index));
  if (!col)
    return;

  if (fixedW > 0 && col->empty())
  {
    col->setMinimumWidth(fixedW);
    col->setMaximumWidth(fixedW);
  }

  col->AddWidgets({w});
}

void TcpWidget::Load(const QStringList& lines)
{
  Router::CONNECTIONS connections;
  for (QStringList::const_iterator i = lines.begin(); i != lines.end(); i++)
    LoadLine(*i, connections);

  // populate UI
  LoadConnections(connections);

  // save connections from UI and perform error checking
  SaveConnections(connections, /*itemStateTable*/ 0);

  // load saved connections (that have been error checked)
  LoadConnections(connections);
}

void TcpWidget::LoadLine(const QString& line, Router::CONNECTIONS& connections)
{
  QStringList items;
  FileUtils::GetItemsFromQuotedString(line, items);

  if (items.size() == 5)
  {
    Router::sConnection connection;

    connection.label = items[0];

    bool ok = false;
    int n = items[1].toInt(&ok);
    connection.server = (ok && n != 0);

    n = items[2].toInt(&ok);
    connection.frameMode = ((ok && n >= 0 && n < OSCStream::FRAME_MODE_COUNT) ? static_cast<OSCStream::EnumFrameMode>(n) : OSCStream::FRAME_MODE_INVALID);

    connection.addr.ip = items[3];
    connection.addr.port = items[4].toUShort();

    connections.push_back(connection);
  }
}

void TcpWidget::Save(QTextStream& stream)
{
  Router::CONNECTIONS connections;
  SaveConnections(connections, /*itemStateTable*/ nullptr);

  for (Router::CONNECTIONS::const_iterator i = connections.begin(); i != connections.end(); i++)
  {
    const Router::sConnection& connection = *i;

    stream << FileUtils::QuotedString(connection.label);
    stream << QStringLiteral(",%1").arg(static_cast<int>(connection.server ? 1 : 0));
    stream << QStringLiteral(",%1").arg(static_cast<int>(connection.frameMode));
    stream << QStringLiteral(",%1").arg(FileUtils::QuotedString(connection.addr.ip));
    stream << QStringLiteral(",%1").arg(connection.addr.port);
    stream << QLatin1Char('\n');
  }
}

void TcpWidget::SaveConnections(Router::CONNECTIONS& connections, ItemStateTable* itemStateTable)
{
  connections.clear();

  for (size_t i = 0; i < m_Rows.size(); i++)
  {
    Row& row = m_Rows[i];

    Router::sConnection connection;
    connection.addr.port = row.port->text().toUShort();
    if (connection.addr.port == 0)
      continue;  // port required

    connection.label = row.label->text();
    connection.server = (row.mode->currentIndex() == 0);

    int n = row.framing->currentIndex();
    connection.frameMode = ((n >= 0 && n < OSCStream::FRAME_MODE_COUNT) ? static_cast<OSCStream::EnumFrameMode>(n) : OSCStream::FRAME_MODE_DEFAULT);

    connection.addr.ip = row.ip->text();
    if (connection.addr.ip == QLatin1String("0.0.0.0"))
      connection.addr.ip.clear();

    if (HasConnection(connections, connection.addr))
      continue;

    if (itemStateTable)
    {
      connection.itemStateTableId = itemStateTable->Register();
      row.itemStateTableId = connection.itemStateTableId;
    }
    else
      connection.itemStateTableId = row.itemStateTableId = ItemStateTable::sm_Invalid_Id;

    connections.push_back(connection);
  }
}

void TcpWidget::UpdateItemState(const ItemStateTable& itemStateTable)
{
  for (size_t i = 0; i < m_Rows.size(); ++i)
  {
    Row& row = m_Rows[i];

    const ItemState* itemState = itemStateTable.GetItemState(row.itemStateTableId);
    if (!(itemState && itemState->dirty))
      return;

    QColor color;
    ItemState::GetStateColor(itemState->state, color);
    row.state->SetColor(color);

    QString name;
    ItemState::GetStateName(itemState->state, name);
    row.state->setToolTip(name);

    if (itemState->state != ItemState::STATE_UNINITIALIZED)
      row.state->Activate(0);

    if (itemState->activity)
    {
      row.activity->SetColor(ACTIVITY_COLOR);
      row.activity->Activate(ACTIVITY_TIMEOUT_MS);
    }
  }
}

void TcpWidget::resizeEvent(QResizeEvent* /*event*/)
{
  UpdateLayout();
}

void TcpWidget::showEvent(QShowEvent* /*event*/)
{
  UpdateLayout();
}

void TcpWidget::paintEvent(QPaintEvent* /*event*/)
{
  QPainter painter(this);
  painter.fillRect(rect(), BG_COLOR);
}

void TcpWidget::UpdateLayout()
{
  bool b = m_Cols->blockSignals(true);
  int y = m_Headers[0]->sizeHint().height() + static_cast<int>(RoutingCol::Constants::kSpacing);

  m_Scroll->setGeometry(static_cast<int>(RoutingCol::Constants::kSpacing), y, width() - static_cast<int>(RoutingCol::Constants::kSpacing) * 2, height() - y);

  for (size_t row = 0; row < m_Rows.size(); ++row)
  {
    int h = m_Rows[row].label->sizeHint().height();
    for (int col = 0; col < m_Cols->count(); ++col)
    {
      RoutingCol* routingCol = qobject_cast<RoutingCol*>(m_Cols->widget(col));
      if (routingCol)
        routingCol->SetHeight(row, h);
    }
  }

  int maxHeight = 0;

  for (int col = 0; col < m_Cols->count(); ++col)
  {
    RoutingCol* routingCol = qobject_cast<RoutingCol*>(m_Cols->widget(col));
    if (!routingCol)
      continue;

    int h = routingCol->UpdateLayout();
    maxHeight = qMax(maxHeight, h);
  }

  m_Cols->setGeometry(0, 0, m_Scroll->width(), qMax(height(), maxHeight));
  m_Cols->blockSignals(b);

  updateHeaders();
}

void TcpWidget::updateHeaders()
{
  for (int i = 0; i < static_cast<int>(Col::kCount); ++i)
  {
    QRect r = RectForCol(static_cast<Col>(i));
    m_Headers[i]->setGeometry(r.x(), 0, r.width(), m_Headers[0]->sizeHint().height());
  }

  update();
}

QRect TcpWidget::RectForCol(Col col) const
{
  int index = static_cast<int>(col);
  if (index < 0 || index >= m_Cols->count())
    return QRect();

  QWidget* w = m_Cols->widget(index);
  if (!w)
    return QRect();

  return QRect(w->mapTo(this, QPoint(0, 0)), w->mapTo(this, QPoint(w->width() - 1, w->height() - 1)));
}

void TcpWidget::onAddRemoveClicked(size_t id)
{
  if (id >= m_Rows.size())
    return;

  if (id == (m_Rows.size() - 1))
    AddRow(m_Rows.size() - 1, /*remove*/ false, Router::sConnection());  // add new connection
  else
    m_Rows.erase(m_Rows.begin() + id);

  Router::CONNECTIONS connections;
  SaveConnections(connections, /*itemStateTable*/ 0);
  LoadConnections(connections);
}

bool TcpWidget::HasConnection(const Router::CONNECTIONS& connections, const EosAddr& addr)
{
  for (Router::CONNECTIONS::const_iterator i = connections.begin(); i != connections.end(); i++)
  {
    if (i->addr == addr)
      return true;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////

ProtocolComboBox::ProtocolComboBox(size_t row, Protocol protocol, QWidget* parent /*= nullptr*/)
  : QComboBox(parent)
  , m_Row(row)
{
  setToolTip(tr("Protocol"));

  protocol = SanitizedProtocol(static_cast<int>(protocol));
  for (int i = 0; i < static_cast<int>(Protocol::kCount); ++i)
  {
    addItem(ProtocolName(static_cast<Protocol>(i)), i);
    if (static_cast<Protocol>(i) == protocol)
      setCurrentIndex(count() - 1);
  }

  connect(this, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ProtocolComboBox::onCurrentIndexChanged);
}

Protocol ProtocolComboBox::GetProtocol() const
{
  return SanitizedProtocol(currentData().toInt());
}

void ProtocolComboBox::onCurrentIndexChanged(int /*index*/)
{
  emit protocolChanged(m_Row, GetProtocol());
}

QString ProtocolComboBox::ProtocolName(Protocol protocol)
{
  switch (protocol)
  {
    case Protocol::kOSC: return tr("OSC");
    case Protocol::kPSN: return tr("PSN");
  }

  return tr("Unknown(%1)").arg(static_cast<int>(protocol));
}

Protocol ProtocolComboBox::SanitizedProtocol(int protocol)
{
  if (protocol < 0 || protocol >= static_cast<int>(Protocol::kCount))
    return Protocol::kDefault;

  return static_cast<Protocol>(protocol);
}

////////////////////////////////////////////////////////////////////////////////

RoutingWidget::RoutingWidget(QWidget* parent /*= nullptr*/)
  : QWidget(parent)
{
  m_Incoming = new QLabel(tr("Incoming"), this);
  m_Incoming->setAlignment(Qt::AlignCenter);

  m_Outgoing = new QLabel(tr("Outgoing"), this);
  m_Outgoing->setAlignment(Qt::AlignCenter);

  for (int i = 0; i < static_cast<int>(Col::kCount); ++i)
  {
    m_Headers[i] = new QLabel(HeaderForCol(static_cast<Col>(i)), this);
    m_Headers[i]->setAlignment(Qt::AlignCenter);
  }
  m_Headers[static_cast<int>(Col::kOutScript)]->setToolTip(tr("JavaScript"));

  m_Scroll = new QScrollArea(this);
  m_Scroll->setWidgetResizable(true);

  m_Cols = new QSplitter(m_Cols);
  m_Scroll->setWidget(m_Cols);
  m_Cols->show();
  for (int i = 0; i < static_cast<int>(Col::kCount); ++i)
  {
    RoutingCol* col = new RoutingCol(m_Cols);
    m_Cols->addWidget(col);
    m_Cols->setCollapsible(i, false);
    m_Cols->setStretchFactor(i, 1);
    col->show();

    switch (static_cast<Col>(i))
    {
      case Col::kLabel:
      case Col::kInIP:
      case Col::kOutIP: m_Cols->setStretchFactor(i, 3); break;

      case Col::kDivider:
        col->setMinimumWidth(48);
        col->setMaximumWidth(48);
        break;

      case Col::kInPath:
      case Col::kOutPath: m_Cols->setStretchFactor(i, 8); break;
    }
  }

  connect(m_Cols, &QSplitter::splitterMoved, this, &RoutingWidget::updateHeaders);

  Clear();
  UpdateLayout();
}

void RoutingWidget::Clear()
{
  m_Rows.clear();

  for (int i = 0; i < m_Cols->count(); ++i)
  {
    RoutingCol* col = qobject_cast<RoutingCol*>(m_Cols->widget(i));
    if (col)
      col->clear();
  }
}

QString RoutingWidget::HeaderForCol(Col col)
{
  switch (col)
  {
    case Col::kLabel: return tr("Name");

    case Col::kInIP:
    case Col::kOutIP: return tr("IP");

    case Col::kInPort:
    case Col::kOutPort: return tr("Port");

    case Col::kInProtocol:
    case Col::kOutProtocol: return tr("Prot");

    case Col::kInPath:
    case Col::kOutPath: return tr("Path");

    case Col::kInMin:
    case Col::kOutMin: return tr("Min");

    case Col::kInMax:
    case Col::kOutMax: return tr("Max");

    case Col::kOutScript: return tr("JS");
  }

  return QString();
}

void RoutingWidget::LoadRoutes(const Router::ROUTES& routes)
{
  Clear();

  size_t id = 0;
  m_Rows.reserve(routes.size());
  for (Router::ROUTES::const_iterator i = routes.begin(); i != routes.end(); i++)
    AddRow(id++, /*remove*/ true, i->label, i->src, i->dst);

  AddRow(id++, /*remove*/ false, QString(), EosRouteSrc(), EosRouteDst());

  UpdateLayout();
}

void RoutingWidget::AddRow(size_t id, bool remove, const QString& label, const EosRouteSrc& src, const EosRouteDst& dst)
{
  int col = 0;

  Row row;
  row.id = id;
  row.label = new QLineEdit(label, m_Cols->widget(col));
  row.label->setToolTip(tr("Text label for this route"));
  AddCol(col++, row.label);

  row.inState = new Indicator(m_Cols->widget(col));
  row.inState->setToolTip(tr("Status"));
  row.inState->SetColor(MUTED_COLOR);
  row.inState->Deactivate();
  row.inState->setMinimumSize(16, 16);
  row.inState->setMaximumWidth(16);
  AddCol(col++, row.inState, /*fixed*/ true);

  row.inActivity = new Indicator(m_Cols->widget(col));
  row.inActivity->setToolTip(tr("Activity"));
  row.inActivity->SetColor(MUTED_COLOR);
  row.inActivity->Deactivate();
  AddCol(col++, row.inActivity, /*fixed*/ true);

  row.inIP = new QLineEdit(m_Cols->widget(col));
  row.inIP->setToolTip(tr("Only route packets received from this specific IP address\n\nLeave blank to route packets received from any IP address\n\nFor multicast, use 2 comma separated IP addresses\n(the first may be blank)"));
  if (src.multicastIP.isEmpty())
    row.inIP->setText(src.addr.ip);
  else
    row.inIP->setText(src.addr.ip + QLatin1Char(',') + src.multicastIP);
  AddCol(col++, row.inIP);

  row.inPort = new QLineEdit(m_Cols->widget(col));
  row.inPort->setToolTip(tr("Route packets received on this port (REQUIRED)"));
  row.inPort->setText((src.addr.port == 0) ? QString() : QString::number(src.addr.port));
  AddCol(col++, row.inPort);

  row.inProtocol = new ProtocolComboBox(id, src.protocol, m_Cols->widget(col));
  connect(row.inProtocol, &ProtocolComboBox::protocolChanged, this, &RoutingWidget::onInProtocolChanged);
  AddCol(col++, row.inProtocol);

  row.inPath = new QLineEdit(m_Cols->widget(col));
  row.inPath->setToolTip(
      tr("Only route received OSC commands with this specific OSC command path\n"
         "(use * for wildcard matching, ex: /eos/out/event/*)\n"
         "\n"
         "Leave blank to route received packets with any OSC command path (or non-OSC packets)\n"
         "\n"
         "Incoming PSN:\n"
         "  Individual:\n"
         "    /psn/<id>/pos=x,y,z\n"
         "    /psn/<id>/speed=x,y,z\n"
         "    /psn/<id>/orientation=x,y,z\n"
         "    /psn/<id>/acceleration=x,y,z\n"
         "    /psn/<id>/target=x,y,z\n"
         "    /psn/<id>/status=status\n"
         "    /psn/<id>/timestamp=timestamp\n"
         "  Unified:\n"
         "    /psn/<id>/pos/speed/orientation/acceleration/..."));
  row.inPath->setText(src.path);
  AddCol(col++, row.inPath);

  row.inMin = new QLineEdit(m_Cols->widget(col));
  row.inMin->setToolTip(tr("Clip first outgoing OSC argument\n\nScale first outgoing OSC argument when all min/max fields populated"));
  QString transformStr;
  TransformToString(dst.inMin, transformStr);
  row.inMin->setText(transformStr);
  AddCol(col++, row.inMin);

  row.inMax = new QLineEdit(m_Cols->widget(col));
  row.inMax->setToolTip(tr("Clip first outgoing OSC argument\n\nScale first outgoing OSC argument when all min/max fields populated"));
  TransformToString(dst.inMax, transformStr);
  row.inMax->setText(transformStr);
  AddCol(col++, row.inMax);

  row.divider = new QLabel(QString("%1").arg(QChar(0x25B6)), m_Cols->widget(col));
  QPalette p = row.divider->palette();
  p.setColor(QPalette::WindowText, QColor(200, 200, 200));
  row.divider->setPalette(p);
  QFont fnt = row.divider->font();
  fnt.setPointSize(16);
  row.divider->setFont(fnt);
  row.divider->setAlignment(Qt::AlignCenter);
  AddCol(col++, row.divider);

  row.outState = new Indicator(m_Cols->widget(col));
  row.outState->setToolTip(tr("Status"));
  row.outState->SetColor(MUTED_COLOR);
  row.outState->Deactivate();
  AddCol(col++, row.outState, /*fixed*/ true);

  row.outActivity = new Indicator(m_Cols->widget(col));
  row.outActivity->setToolTip(tr("Activity"));
  row.outActivity->SetColor(MUTED_COLOR);
  row.outActivity->Deactivate();
  AddCol(col++, row.outActivity, /*fixed*/ true);

  row.outIP = new QLineEdit(m_Cols->widget(col));
  row.outIP->setToolTip(tr("Route received packets to this IP address\n\nLeave blank to route packets to the same IP address they were sent from"));
  row.outIP->setText(dst.addr.ip);
  AddCol(col++, row.outIP);

  row.outPort = new QLineEdit(m_Cols->widget(col));
  row.outPort->setToolTip(tr("Route received packets to this port\n\nLeave blank to route packets to the same port they were received on"));
  row.outPort->setText((dst.addr.port == 0) ? QString() : QString::number(dst.addr.port));
  AddCol(col++, row.outPort);

  row.outProtocol = new ProtocolComboBox(id, dst.protocol, m_Cols->widget(col));
  connect(row.outProtocol, &ProtocolComboBox::protocolChanged, this, &RoutingWidget::onOutProtocolChanged);
  AddCol(col++, row.outProtocol);

  QString tip =
      tr("Route received OSC commands to this OSC command\n"
         "\n"
         "Use %1, %2, %3, etc... to insert specific sections from the received OSC command\n"
         "\n"
         "For PSN output, see incoming path tool tip for path formatting\n"
         "\n"
         "Ex: Remap path\n"
         "Input:  /eos/out/event/cue/1/25/fire\n"
         "Path:   /cue/%6/start\n"
         "Output: /cue/25/start\n"
         "\n"
         "Ex: Remap path to argument\n"
         "Input:  /cue/25/start\n"
         "Path:   /eos/cue/fire=%2\n"
         "Output: /eos/cue/fire, 25(i)\n"
         "\n"
         "Ex: Remap argument to path\n"
         "Input:  /eos/cue/fire, 25(i)\n"
         "Path:   /eos/%4/start=\n"
         "Output: /cue/25/start");
  row.outPath = new QLineEdit(m_Cols->widget(col));
  row.outPath->setToolTip(tip);
  row.outPath->setText(dst.path);

  row.outScriptText = new ScriptEdit(m_Cols->widget(col));
  tip =
      tr("JavaScript Variables:\n--------------------\n"
         "OSC = outgoing osc path (string)\n"
         "ARGS = array of osc arguments\n"
         "\n"
         "Write your own JavaScript to modify the OSC and ARGS variables\n"
         "\n"
         "Ex:\n"
         "// modify outgoing osc fader from percent to 8-bit value:\n"
         "OSC = OSC + \"/level\";\n"
         "ARGS[0] = Math.round(ARGS[0] * 255);");
  row.outScriptText->setToolTip(tip);
  row.outScriptText->hide();
  row.outScriptText->setText(dst.scriptText);
  row.outScriptText->CheckForErrors();
  AddCol(col++, {row.outPath, row.outScriptText});
  row.outPath->setVisible(!dst.script);
  row.outScriptText->setVisible(dst.script);

  row.outScript = new RoutingCheckBox(id, m_Cols->widget(col));
  row.outScript->setToolTip(tr("JavaScript"));
  row.outScript->setFixedHeight(row.outPath->sizeHint().height());
  row.outScript->setChecked(dst.script);
  connect(row.outScript, &RoutingCheckBox::toggledWithId, this, &RoutingWidget::onOutScriptToggled);
  AddCol(col++, row.outScript, /*fixed*/ true);

  row.outMin = new QLineEdit(m_Cols->widget(col));
  row.outMin->setToolTip(tr("Clip first outgoing OSC argument\n\nScale first outgoing OSC argument when all min/max fields populated"));
  TransformToString(dst.outMin, transformStr);
  row.outMin->setText(transformStr);
  AddCol(col++, row.outMin);

  row.outMax = new QLineEdit(m_Cols->widget(col));
  row.outMax->setToolTip(tr("Clip first outgoing OSC argument\n\nScale first outgoing OSC argument when all min/max fields populated"));
  TransformToString(dst.outMax, transformStr);
  row.outMax->setText(transformStr);
  AddCol(col++, row.outMax);

  row.addRemove = new RoutingButton(remove ? QLatin1String("-") : QLatin1String("+"), id, m_Cols->widget(col));
  row.addRemove->setToolTip(tr("Add/Remove this route"));
  connect(row.addRemove, &RoutingButton::clickedWithId, this, &RoutingWidget::onAddRemoveClicked);
  AddCol(col++, row.addRemove, /*fixed*/ true);

  m_Rows.push_back(row);
}

void RoutingWidget::AddCol(int index, QWidget* w, bool fixed /*= false*/)
{
  RoutingCol::Widgets widgets = {w};
  AddCol(index, widgets, fixed);
}

void RoutingWidget::AddCol(int index, const RoutingCol::Widgets& w, bool fixed /*= false*/)
{
  RoutingCol* col = qobject_cast<RoutingCol*>(m_Cols->widget(index));
  if (!col)
    return;

  if (fixed && !w.empty() && col->empty())
  {
    int fw = qMin(w.front()->sizeHint().width(), w.front()->sizeHint().height());
    col->setMinimumWidth(fw);
    col->setMaximumWidth(fw);
  }

  col->AddWidgets(w);
}

void RoutingWidget::Load(const QStringList& lines)
{
  Router::ROUTES routes;
  for (QStringList::const_iterator i = lines.begin(); i != lines.end(); i++)
    LoadLine(*i, routes);

  // populate UI
  LoadRoutes(routes);

  // save routes from UI and perform error checking
  SaveRoutes(routes, /*itemStateTable*/ 0);

  // load saved routes (that have been error checked)
  LoadRoutes(routes);
}

void RoutingWidget::LoadLine(const QString& line, Router::ROUTES& routes)
{
  QStringList items;
  FileUtils::GetItemsFromQuotedString(line, items);

  if (items.size() > 10)
  {
    Router::sRoute route;

    route.label = items[0];
    route.src.addr.ip = items[1];
    route.src.addr.port = items[2].toUShort();
    route.src.path = items[3];
    StringToTransform(items[4], route.dst.inMin);
    StringToTransform(items[5], route.dst.inMax);

    route.dst.addr.ip = items[6];
    route.dst.addr.port = items[7].toUShort();
    route.dst.path = items[8];
    StringToTransform(items[9], route.dst.outMin);
    StringToTransform(items[10], route.dst.outMax);

    if (items.size() > 11)
    {
      route.dst.scriptText = items[11];
      route.dst.script = !route.dst.scriptText.isEmpty();
    }

    if (items.size() > 12)
      route.src.multicastIP = items[12];

    if (items.size() > 13)
      route.src.protocol = ProtocolComboBox::SanitizedProtocol(items[13].toInt());

    if (items.size() > 14)
      route.dst.protocol = ProtocolComboBox::SanitizedProtocol(items[14].toInt());

    routes.push_back(route);
  }
}

void RoutingWidget::Save(QTextStream& stream)
{
  Router::ROUTES routes;
  SaveRoutes(routes, /*itemStateTable*/ nullptr);

  for (Router::ROUTES::const_iterator i = routes.begin(); i != routes.end(); i++)
  {
    const Router::sRoute& route = *i;

    QString inMinStr;
    TransformToString(route.dst.inMin, inMinStr);
    QString inMaxStr;
    TransformToString(route.dst.inMax, inMaxStr);
    QString outMinStr;
    TransformToString(route.dst.outMin, outMinStr);
    QString outMaxStr;
    TransformToString(route.dst.outMax, outMaxStr);

    stream << FileUtils::QuotedString(route.label);
    stream << QStringLiteral(",%1").arg(FileUtils::QuotedString(route.src.addr.ip));
    stream << QStringLiteral(",%1").arg(route.src.addr.port);
    stream << QStringLiteral(",%1").arg(FileUtils::QuotedString(route.src.path));
    stream << QStringLiteral(",%1").arg(inMinStr);
    stream << QStringLiteral(",%1").arg(inMaxStr);
    stream << QStringLiteral(",%1").arg(FileUtils::QuotedString(route.dst.addr.ip));
    stream << QStringLiteral(",%1").arg(route.dst.addr.port);
    stream << QStringLiteral(",%1").arg(FileUtils::QuotedString(route.dst.path));
    stream << QStringLiteral(",%1").arg(outMinStr);
    stream << QStringLiteral(",%1").arg(outMaxStr);
    stream << QStringLiteral(",%1").arg(route.dst.script ? FileUtils::QuotedString(route.dst.scriptText) : QString());
    stream << QStringLiteral(",%1").arg(FileUtils::QuotedString(route.src.multicastIP));
    stream << QStringLiteral(",%1").arg(static_cast<int>(route.src.protocol));
    stream << QStringLiteral(",%1").arg(static_cast<int>(route.dst.protocol));
    stream << QLatin1Char('\n');
  }
}

void RoutingWidget::SaveRoutes(Router::ROUTES& routes, ItemStateTable* itemStateTable)
{
  routes.clear();

  // show state/activity per EosAddr
  AddrStates srcAddrStates;
  AddrStates dstAddrStates;

  for (size_t i = 0; i < m_Rows.size(); i++)
  {
    Row& row = m_Rows[i];

    Router::sRoute route;
    route.src.addr.port = row.inPort->text().toUShort();
    if (route.src.addr.port == 0)
      continue;  // port required

    route.label = row.label->text();

    QStringList ips = row.inIP->text().split(QLatin1Char(','));
    if (ips.size() > 1)
    {
      route.src.addr.ip = ips[0].trimmed();
      route.src.multicastIP = ips[1].trimmed();
    }
    else
      route.src.addr.ip = row.inIP->text();

    route.src.protocol = row.inProtocol->GetProtocol();
    route.src.path = row.inPath->text();

    route.dst.addr.ip = row.outIP->text();
    route.dst.protocol = row.outProtocol->GetProtocol();
    route.dst.addr.port = row.outPort->text().toUShort();
    route.dst.path = row.outPath->text();
    route.dst.script = row.outScript->isChecked();
    route.dst.scriptText = row.outScriptText->toPlainText();

    StringToTransform(row.inMin->text(), route.dst.inMin);
    StringToTransform(row.inMax->text(), route.dst.inMax);
    StringToTransform(row.outMin->text(), route.dst.outMin);
    StringToTransform(row.outMax->text(), route.dst.outMax);

    if (HasRoute(routes, route.src, route.dst))
      continue;

    if (itemStateTable)
    {
      AddrStates::const_iterator j = srcAddrStates.find(route.src.addr);
      if (j == srcAddrStates.end())
        srcAddrStates[route.src.addr] = route.srcItemStateTableId = itemStateTable->Register();
      else
        route.srcItemStateTableId = j->second;
      row.inItemStateTableId = route.srcItemStateTableId;

      j = dstAddrStates.find(route.dst.addr);
      if (j == dstAddrStates.end())
        dstAddrStates[route.dst.addr] = route.dstItemStateTableId = itemStateTable->Register();
      else
        route.dstItemStateTableId = j->second;
      row.outItemStateTableId = route.dstItemStateTableId;
    }
    else
      route.srcItemStateTableId = route.dstItemStateTableId = ItemStateTable::sm_Invalid_Id;

    routes.push_back(route);
  }
}

void RoutingWidget::UpdateItemState(const ItemStateTable& itemStateTable)
{
  for (size_t i = 0; i < m_Rows.size(); ++i)
  {
    Row& row = m_Rows[i];
    UpdateItemState(itemStateTable.GetItemState(row.inItemStateTableId), *row.inState, *row.inActivity);
    UpdateItemState(itemStateTable.GetItemState(row.outItemStateTableId), *row.outState, *row.outActivity);
  }
}

void RoutingWidget::UpdateItemState(const ItemState* itemState, Indicator& stateIndicator, Indicator& activityIndicator)
{
  if (!(itemState && itemState->dirty))
    return;

  QColor color;
  ItemState::GetStateColor(itemState->state, color);
  stateIndicator.SetColor(color);

  QString name;
  ItemState::GetStateName(itemState->state, name);
  stateIndicator.setToolTip(name);

  if (itemState->state != ItemState::STATE_UNINITIALIZED)
    stateIndicator.Activate(0);

  if (itemState->activity)
  {
    activityIndicator.SetColor(ACTIVITY_COLOR);
    activityIndicator.Activate(ACTIVITY_TIMEOUT_MS);
  }
}

void RoutingWidget::resizeEvent(QResizeEvent* /*event*/)
{
  UpdateLayout();
}

void RoutingWidget::showEvent(QShowEvent* /*event*/)
{
  UpdateLayout();
}

void RoutingWidget::paintEvent(QPaintEvent* /*event*/)
{
  QPainter painter(this);
  painter.fillRect(rect(), BG_COLOR);

  int y = m_Incoming->sizeHint().height() + static_cast<int>(RoutingCol::Constants::kSpacing) / 2;
  int h = height() - y;
  int x1 = RectForCol(Col::kInState).left();
  int x2 = RectForCol(Col::kInMax).right() + static_cast<int>(RoutingCol::Constants::kSpacing);
  painter.fillRect(QRect(x1, y, x2 - x1, h), QColor(45, 45, 45));

  x1 = RectForCol(Col::kOutState).left();
  x2 = RectForCol(Col::kOutMax).right() + static_cast<int>(RoutingCol::Constants::kSpacing);
  painter.fillRect(QRect(x1, y, x2 - x1, h), QColor(45, 45, 45));
}

void RoutingWidget::UpdateLayout()
{
  bool b = m_Cols->blockSignals(true);
  int y = m_Incoming->sizeHint().height() + static_cast<int>(RoutingCol::Constants::kSpacing);
  y += m_Headers[0]->sizeHint().height() + static_cast<int>(RoutingCol::Constants::kSpacing);

  m_Scroll->setGeometry(static_cast<int>(RoutingCol::Constants::kSpacing), y, width() - static_cast<int>(RoutingCol::Constants::kSpacing) * 2, height() - y);

  for (size_t row = 0; row < m_Rows.size(); ++row)
  {
    int h = 0;
    if (m_Rows[row].outScriptText->isHidden())
      h = m_Rows[row].outPath->sizeHint().height();
    else
      h = m_Rows[row].outScriptText->sizeHint().height();

    for (int col = 0; col < m_Cols->count(); ++col)
    {
      RoutingCol* routingCol = qobject_cast<RoutingCol*>(m_Cols->widget(col));
      if (routingCol)
        routingCol->SetHeight(row, h);
    }
  }

  int maxHeight = 0;

  for (int col = 0; col < m_Cols->count(); ++col)
  {
    RoutingCol* routingCol = qobject_cast<RoutingCol*>(m_Cols->widget(col));
    if (!routingCol)
      continue;

    int h = routingCol->UpdateLayout();
    maxHeight = qMax(maxHeight, h);
  }

  m_Cols->setGeometry(0, 0, m_Scroll->width(), qMax(height(), maxHeight));
  m_Cols->blockSignals(b);

  updateHeaders();
}

void RoutingWidget::updateHeaders()
{
  int x1 = RectForCol(Col::kInState).left();
  int x2 = RectForCol(Col::kInMax).right();
  m_Incoming->setGeometry(x1, 0, x2 - x1, m_Incoming->sizeHint().height());

  x1 = RectForCol(Col::kOutState).left();
  x2 = RectForCol(Col::kOutMax).right();
  m_Outgoing->setGeometry(x1, 0, x2 - x1, m_Outgoing->sizeHint().height());

  int y = m_Incoming->height() + static_cast<int>(RoutingCol::Constants::kSpacing);
  for (int i = 0; i < static_cast<int>(Col::kCount); ++i)
  {
    QRect r = RectForCol(static_cast<Col>(i));
    m_Headers[i]->setGeometry(r.x(), y, r.width(), m_Headers[0]->sizeHint().height());
  }

  update();
}

QRect RoutingWidget::RectForCol(Col col) const
{
  int index = static_cast<int>(col);
  if (index < 0 || index >= m_Cols->count())
    return QRect();

  QWidget* w = m_Cols->widget(index);
  if (!w)
    return QRect();

  return QRect(w->mapTo(this, QPoint(0, 0)), w->mapTo(this, QPoint(w->width() - 1, w->height() - 1)));
}

void RoutingWidget::onOutScriptToggled(size_t id, bool checked)
{
  if (id >= m_Rows.size())
    return;

  m_Rows[id].outPath->setVisible(!checked);
  m_Rows[id].outScriptText->setVisible(checked);
  UpdateLayout();
}

void RoutingWidget::onAddRemoveClicked(size_t id)
{
  if (id >= m_Rows.size())
    return;

  if (id == (m_Rows.size() - 1))
    AddRow(m_Rows.size() - 1, /*remove*/ false, QString(), EosRouteSrc(), EosRouteDst());  // add new route
  else
    m_Rows.erase(m_Rows.begin() + id);

  Router::ROUTES routes;
  SaveRoutes(routes, /*itemStateTable*/ 0);
  LoadRoutes(routes);
}

void RoutingWidget::onInProtocolChanged(size_t row, Protocol protocol)
{
  if (row >= m_Rows.size() || protocol != Protocol::kPSN)
    return;

  const Row& r = m_Rows[row];
  if (r.inIP->text().isEmpty())
    r.inIP->setText(QLatin1String(",") + Router::GetDefaultPSNIP());
  if (r.inPort->text().isEmpty())
    r.inPort->setText(QString::number(Router::GetDefaultPSNPort()));
}

void RoutingWidget::onOutProtocolChanged(size_t row, Protocol protocol)
{
  if (row >= m_Rows.size() || protocol != Protocol::kPSN)
    return;

  const Row& r = m_Rows[row];
  if (r.outIP->text().isEmpty())
    r.outIP->setText(QLatin1String(",") + Router::GetDefaultPSNIP());
  if (r.outPort->text().isEmpty())
    r.outPort->setText(QString::number(Router::GetDefaultPSNPort()));
}

void RoutingWidget::StringToTransform(const QString& str, EosRouteDst::sTransform& transform)
{
  if (str.isEmpty())
  {
    transform.enabled = false;
    transform.value = 0;
  }
  else
  {
    transform.value = str.toFloat(&transform.enabled);
    if (!transform.enabled)
      transform.value = 0;
  }
}

void RoutingWidget::TransformToString(const EosRouteDst::sTransform& transform, QString& str)
{
  str = (transform.enabled ? QString::number(transform.value) : QString());
}

bool RoutingWidget::HasRoute(const Router::ROUTES& routes, const EosRouteSrc& src, const EosRouteDst& dst)
{
  for (Router::ROUTES::const_iterator i = routes.begin(); i != routes.end(); i++)
  {
    if (i->src == src && i->dst == dst)
      return true;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////

MainWindow::MainWindow(EosPlatform* platform, QWidget* parent /*=0*/, Qt::WindowFlags f /*=Qt::WindowFlags()*/)
  : QWidget(parent, f)
  , m_Settings("ETC", "OSCRouter")
  , m_FileDepth(10000)
  , m_Unsaved(false)
  , m_RouterThread(0)
  , m_FileLineCount(0)
  , m_ReconnectDelay(5000)
  , m_pPlatform(platform)
{
#ifdef WIN32
  QIcon icon;

  const int iconSizes[] = {512, 256, 128, 64, 32, 16};
  const size_t numIcons = sizeof(iconSizes) / sizeof(iconSizes[0]);
  for (size_t i = 0; i < numIcons; i++)
  {
    HICON hIcon = static_cast<HICON>(LoadImage(GetModuleHandle(0), MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, iconSizes[i], iconSizes[i], LR_LOADTRANSPARENT));
    if (hIcon)
    {
      icon.addPixmap(QPixmap::fromImage(QImage::fromHICON(hIcon)));
      DestroyIcon(hIcon);
    }
  }

  setWindowIcon(icon);
#endif

  setStyleSheet(
      QLatin1String("QTabWidget::pane {border: transparent;}"
                    "QTabBar::tab {background: #232323; border: 1px solid #202020; border-bottom: transparent; padding: 6px;}"
                    "QTabBar::tab:selected {background: #282828;}"
                    "QMenuBar {background: transparent;}"
                    "QScrollArea {background: transparent;}"
                    "QSplitter::handle {image: url(\"\");}"
                    "QSplitter::handle:hover {background: #08ffffff;}"
                    "QSplitter {background: transparent;}"));

  int logDepth = m_Settings.value(SETTING_LOG_DEPTH, 200).toInt();
  if (logDepth < 1)
    logDepth = 1;
  m_Settings.setValue(SETTING_LOG_DEPTH, logDepth);

  m_FileDepth = m_Settings.value(SETTING_FILE_DEPTH, m_FileDepth).toInt();
  m_Settings.setValue(SETTING_FILE_DEPTH, m_FileDepth);

  int n = m_Settings.value(SETTING_RECONNECT_DELAY, static_cast<int>(m_ReconnectDelay)).toInt();
  m_ReconnectDelay = ((n > 0) ? static_cast<unsigned int>(n) : 0);
  m_Settings.setValue(SETTING_RECONNECT_DELAY, m_ReconnectDelay);

  n = m_Settings.value(SETTING_DISABLE_SYSTEM_IDLE, 1).toInt();
  m_DisableSystemIdle = (n != 0);
  m_Settings.setValue(SETTING_DISABLE_SYSTEM_IDLE, static_cast<int>(m_DisableSystemIdle ? 1 : 0));

  InitLogFile();

  QGridLayout* layout = new QGridLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  QMenuBar* menu = new QMenuBar(this);
  layout->addWidget(menu, 0, 0);

  QMenu* file = menu->addMenu(tr("&File"));
  file->addAction(tr("&New"), this, &MainWindow::onNewFile);
  file->addAction(tr("&Open"), this, &MainWindow::onOpenFile);
  file->addSeparator();
  file->addAction(tr("&Save"), this, &MainWindow::onSaveFile);
  file->addAction(tr("Save &As..."), this, &MainWindow::onSaveAsFile);
  file->addSeparator();
  file->addAction(tr("E&xit"), this, &MainWindow::close);

  QMenu* log = menu->addMenu(tr("&Log"));
  log->addAction(tr("&Clear"), this, &MainWindow::onClearLog);
  log->addAction(tr("&Open"), this, &MainWindow::onOpenLog);

  QSplitter* splitter = new QSplitter(Qt::Vertical, this);
  layout->addWidget(splitter, 1, 0);

  QWidget* routingBase = new QWidget(splitter);
  splitter->addWidget(routingBase);
  QVBoxLayout* routingLayout = new QVBoxLayout(routingBase);
  routingLayout->setContentsMargins(4, 0, 4, 0);

  QTabWidget* tabs = new QTabWidget(routingBase);
  routingLayout->addWidget(tabs);

  m_RoutingWidget = new RoutingWidget(tabs);
  tabs->addTab(m_RoutingWidget, tr("Routes"));

  m_TcpWidget = new TcpWidget(tabs);
  tabs->addTab(m_TcpWidget, tr("TCP"));

  QPushButton* button = new QPushButton(tr("Apply"), routingBase);
  connect(button, &QPushButton::clicked, this, &MainWindow::onApplyClicked);
  routingLayout->addWidget(button, 0, Qt::AlignRight);

  QWidget* logBase = new QWidget(splitter);
  QGridLayout* logLayout = new QGridLayout(logBase);
  logLayout->setContentsMargins(4, 0, 4, 0);
  splitter->addWidget(logBase);

  m_LogWidget = new LogWidget(logDepth, logBase);
  QPalette pal = m_LogWidget->palette();
  pal.setColor(QPalette::Window, BG_COLOR.darker(145));
  m_LogWidget->setPalette(pal);
  logLayout->addWidget(m_LogWidget, 0, 0);

  m_Log.AddInfo(QString("OSCRouter v%1").arg(APP_VERSION).toUtf8().constData());

  Router::ROUTES noRoutes;
  m_RoutingWidget->LoadRoutes(noRoutes);
  Router::CONNECTIONS noConnections;
  m_TcpWidget->LoadConnections(noConnections);

  RestoreLastFile();
  UpdateWindowTitle();

  pal = palette();
  pal.setColor(QPalette::Window, BG_COLOR.darker(125));
  setPalette(pal);

  QTimer* timer = new QTimer(this);
  connect(timer, &QTimer::timeout, this, &MainWindow::onTick);
  timer->start(60);
}

MainWindow::~MainWindow()
{
  Shutdown();
  ShutdownLogFile();
}

void MainWindow::InitLogFile()
{
  if (m_FileDepth > 0)
  {
    m_LogFile.setFileName(QDir(QDir::tempPath()).absoluteFilePath("OSCRouter.txt"));
    if (m_LogFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
    {
      m_LogStream.setDevice(&m_LogFile);
      m_LogStream.setEncoding(QStringConverter::Utf8);
    }
  }

  m_FileLineCount = 0;
}

void MainWindow::ShutdownLogFile()
{
  if (m_LogFile.isOpen())
  {
    m_LogStream.flush();
    m_LogFile.close();
  }

  m_FileLineCount = 0;
}

void MainWindow::FlushLogQ(EosLog::LOG_Q& logQ)
{
  for (EosLog::LOG_Q::iterator i = logQ.begin(); i != logQ.end(); i++)
  {
    EosLog::sLogMsg& logMsg = *i;

    QDateTime dt = QDateTime::fromSecsSinceEpoch(logMsg.timestamp);
    QString msgText = dt.toString("ddd dd MMM yyyy [h:mm:ss]") + QLatin1Char(' ') + QString::fromStdString(logMsg.text);
    logMsg.text = msgText.toStdString();

    if (m_LogFile.isOpen())
    {
      m_LogStream << logMsg.text.c_str();
      m_LogStream << "\n";

      if (++m_FileLineCount > m_FileDepth)
      {
        ShutdownLogFile();
        InitLogFile();
      }
    }
  }

  m_LogWidget->Log(logQ);
}

void MainWindow::Shutdown()
{
  if (m_RouterThread)
  {
    m_RouterThread->Stop();
    FlushRouterThread(/*logsOnly*/ true);
    delete m_RouterThread;
    m_RouterThread = 0;

    if (m_pPlatform && m_DisableSystemIdle)
    {
      std::string error;
      if (m_pPlatform->SetSystemIdleAllowed(true, "routing stopped", error))
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

bool MainWindow::BuildRoutes()
{
  Shutdown();

  m_ItemStateTable.Clear();

  Router::ROUTES routes;
  m_RoutingWidget->SaveRoutes(routes, &m_ItemStateTable);

  Router::CONNECTIONS connections;
  m_TcpWidget->SaveConnections(connections, &m_ItemStateTable);

  if (!routes.empty())
  {
    if (m_pPlatform && m_DisableSystemIdle)
    {
      std::string error;
      if (m_pPlatform->SetSystemIdleAllowed(false, "routing started", error))
      {
        m_Log.AddInfo("routing started, system idle disabled");
      }
      else
      {
        error.insert(0, "failed to disable system idle, ");
        m_Log.AddDebug(error);
      }
    }

    m_RouterThread = new RouterThread(routes, connections, m_ItemStateTable, m_ReconnectDelay);
    m_RouterThread->start();
    return true;
  }

  return false;
}

void MainWindow::GetDefaultIP(QString& ip)
{
  QHostAddress localHostAddr(QHostAddress::LocalHost);

  QList<QNetworkInterface> nics = QNetworkInterface::allInterfaces();
  for (QList<QNetworkInterface>::const_iterator i = nics.begin(); i != nics.end(); i++)
  {
    const QNetworkInterface& net = *i;
    if (net.isValid() && net.flags().testFlag(QNetworkInterface::IsUp) && net.flags().testFlag(QNetworkInterface::IsRunning))
    {
      QList<QNetworkAddressEntry> addrs = net.addressEntries();
      for (QList<QNetworkAddressEntry>::const_iterator j = addrs.begin(); j != addrs.end(); j++)
      {
        QHostAddress addr = j->ip();
        if (!addr.isNull() && addr.protocol() == QAbstractSocket::IPv4Protocol && addr != localHostAddr)
        {
          ip = addr.toString();
          return;
        }
      }
    }
  }

  ip = localHostAddr.toString();
}

void MainWindow::GetPersistentSavePath(QString& path) const
{
  path = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).absoluteFilePath("save.osc.txt");
}

void MainWindow::UpdateWindowTitle()
{
  QString title(tr("OSCRouter"));
  if (!m_FilePath.isEmpty())
  {
    title.append(" :: ");
    if (m_Unsaved)
      title.append("*");
    title.append(QDir::toNativeSeparators(m_FilePath));
  }
  else if (m_Unsaved)
    title.append("*");
  setWindowTitle(title);
}

bool MainWindow::LoadFile(const QString& path)
{
  m_FilePath = path;
  m_Settings.setValue(SETTING_LAST_FILE, m_FilePath);

  if (Load(path))
  {
    m_Unsaved = false;
    UpdateWindowTitle();
    BuildRoutes();
    return true;
  }

  return false;
}

bool MainWindow::Load(const QString& path)
{
  QFile file(path);
  QTextStream stream(&file);
  stream.setEncoding(QStringConverter::Utf8);
  if (!file.open(QFile::ReadOnly | QFile::Text))
    return false;

  QString contents = stream.readAll();
  contents.remove(QLatin1Char('\r'));
  QStringList lines = contents.split(QLatin1Char('\n'));

  m_RoutingWidget->Load(lines);
  m_TcpWidget->Load(lines);

  return true;
}

bool MainWindow::SaveFile(const QString& path)
{
  m_FilePath = path;
  m_Settings.setValue(SETTING_LAST_FILE, m_FilePath);

  if (Save(path))
  {
    m_Unsaved = false;
    UpdateWindowTitle();
    return true;
  }
  else
    QMessageBox::critical(this, tr("OSCRouter"), tr("Unable to save file \"%1\"").arg(path));

  return false;
}

bool MainWindow::Save(const QString& path)
{
  QDir().mkpath(QFileInfo(path).absolutePath());

  QFile file(path);
  QTextStream stream(&file);
  stream.setEncoding(QStringConverter::Utf8);
  if (!file.open(QFile::WriteOnly | QFile::Truncate | QFile::Text))
    return false;

  m_RoutingWidget->Save(stream);
  m_TcpWidget->Save(stream);

  return true;
}

void MainWindow::RestoreLastFile()
{
  QString path = m_Settings.value(SETTING_LAST_FILE).toString();
  if (!path.isEmpty())
  {
    if (LoadFile(path))
      return;  // success
  }

  // fall-back to loading persistent file
  GetPersistentSavePath(path);
  if (Load(path))
  {
    m_FilePath = m_Settings.value(SETTING_LAST_FILE).toString();
    if (BuildRoutes())
      m_Unsaved = true;
  }
}

void MainWindow::FlushRouterThread(bool logsOnly)
{
  if (m_RouterThread)
  {
    m_RouterThread->Flush(m_TempLogQ, m_ItemStateTable);
    m_Log.AddQ(m_TempLogQ);
  }

  m_Log.Flush(m_TempLogQ);
  FlushLogQ(m_TempLogQ);
  m_TempLogQ.clear();

  if (!logsOnly)
  {
    if (m_ItemStateTable.GetDirty())
    {
      m_RoutingWidget->UpdateItemState(m_ItemStateTable);
      m_TcpWidget->UpdateItemState(m_ItemStateTable);
      m_ItemStateTable.Reset();
    }
  }
}

void MainWindow::onTick()
{
  FlushRouterThread(/*logsOnly*/ false);
}

void MainWindow::buildRoutes()
{
  BuildRoutes();
}

void MainWindow::onNewFile()
{
  if (!ResolveUnsaved())
    return;

  Router::ROUTES noRoutes;
  m_RoutingWidget->LoadRoutes(noRoutes);
  Router::CONNECTIONS noConnections;
  m_TcpWidget->LoadConnections(noConnections);
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

void MainWindow::onOpenFile()
{
  if (!ResolveUnsaved())
    return;

  QString dir;
  QString lastFile = m_Settings.value(SETTING_LAST_FILE).toString();
  if (!lastFile.isEmpty())
    dir = QFileInfo(lastFile).absolutePath();
  if (dir.isEmpty() || !QFileInfo(dir).exists())
    dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
  QString path = QFileDialog::getOpenFileName(this, tr("Open"), dir, tr("OSCRouter File (*.txt *.osc.txt)"));
  if (!path.isEmpty())
  {
    if (!LoadFile(path))
      QMessageBox::critical(this, tr("OSCRouter"), tr("Unable to open file \"%1\"").arg(path));
  }
}

void MainWindow::onSaveFile()
{
  if (m_FilePath.isEmpty())
    onSaveAsFile();
  else
    SaveFile(m_FilePath);
}

void MainWindow::onSaveAsFile()
{
  QString dir;
  QString lastFile = m_Settings.value(SETTING_LAST_FILE).toString();
  if (!lastFile.isEmpty())
    dir = QFileInfo(lastFile).absolutePath();
  if (dir.isEmpty() || !QFileInfo(dir).exists())
    dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
  QString path = QFileDialog::getSaveFileName(this, tr("Save"), dir, tr("OSCRouter File (*.osc.txt)"));
  if (!path.isEmpty())
    SaveFile(path);
}

void MainWindow::onClearLog()
{
  m_LogWidget->Clear();
}

void MainWindow::onOpenLog()
{
  if (m_LogFile.exists())
  {
    if (m_LogFile.isOpen())
      m_LogStream.flush();
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_LogFile.fileName()));
  }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
  if (!ResolveUnsaved())
  {
    event->ignore();
    return;
  }

  QString path;
  GetPersistentSavePath(path);
  Save(path);
  QApplication::exit(0);
}

bool MainWindow::ResolveUnsaved()
{
  if (m_Unsaved)
  {
    QMessageBox mb(QMessageBox::Question, tr("OSCRouter"), tr("Do you want to save changes?"), QMessageBox::NoButton, this);
    QPushButton* saveButton = mb.addButton(tr("Save"), QMessageBox::AcceptRole);
    mb.addButton(tr("Don't Save"), QMessageBox::DestructiveRole);
    QPushButton* cancelButton = mb.addButton(tr("Cancel"), QMessageBox::RejectRole);

    mb.exec();

    if (mb.clickedButton() == saveButton)
    {
      onSaveFile();
      if (m_Unsaved)
        return false;  // error saving, do not close
    }
    else if (mb.clickedButton() == cancelButton)
      return false;
  }

  return true;
}

void MainWindow::onApplyClicked(bool /*checked*/)
{
  Router::ROUTES routes;
  m_RoutingWidget->SaveRoutes(routes, /*itemStateTable*/ 0);
  m_RoutingWidget->LoadRoutes(routes);

  Router::CONNECTIONS connections;
  m_TcpWidget->SaveConnections(connections, /*itemStateTable*/ 0);
  m_TcpWidget->LoadConnections(connections);

  if (!m_Unsaved)
  {
    m_Unsaved = true;
    UpdateWindowTitle();
  }

  QTimer::singleShot(1, this, &MainWindow::buildRoutes);
}

////////////////////////////////////////////////////////////////////////////////
