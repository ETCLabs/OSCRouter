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
#include "Version.h"
#include "UI.h"

#ifdef WIN32
#include <Windows.h>
#include "resource.h"
#endif

// must be last include
#include "LeakWatcher.h"

////////////////////////////////////////////////////////////////////////////////

#define SETTING_LOG_DEPTH "LogDepth"
#define SETTING_FILE_DEPTH "FileDepth"
#define SETTING_LAST_FILE "LastFile"
#define SETTING_RECONNECT_DELAY "ReconnectDelay"
#define SETTING_DISABLE_SYSTEM_IDLE "DisableSystemIdle"
#define SETTING_AUTO_START "AutoStart"
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
      else
        ++i;
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
        QRectF glowRect = m_IconOutline.rect();
        QRectF solidRect = glowRect.adjusted(6, 6, -6, -6);

        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(m_Color, 1.5));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(solidRect);
        painter.end();

        m_IconOutline.setDevicePixelRatio(dpr);

        m_IconFill = QImage(size, size, QImage::Format_ARGB32);
        m_IconFill.fill(0);
        if (painter.begin(&m_IconFill))
        {
          painter.setRenderHint(QPainter::Antialiasing);
          painter.setPen(Qt::NoPen);

          QRadialGradient grad(glowRect.center(), glowRect.width() / 2.0);
          QColor gradColor = m_Color;
          gradColor.setAlpha(160);
          grad.setColorAt(0, m_Color);
          gradColor.setAlpha(0);
          grad.setColorAt(1, gradColor);
          painter.setBrush(grad);
          painter.drawEllipse(glowRect);

          painter.setBrush(m_Color);
          painter.drawEllipse(solidRect);
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

LineEdit::LineEdit(QWidget* parent /*= nullptr*/)
  : QLineEdit(parent)
{
}

LineEdit::LineEdit(const QString& contents, QWidget* parent /*= nullptr*/)
  : QLineEdit(contents, parent)
{
}

QSize LineEdit::sizeHint() const
{
  ensurePolished();
  QFontMetrics fm(font());
  const int iconSize = style()->pixelMetric(QStyle::PM_SmallIconSize, nullptr, this);
  const QMargins tm = textMargins();
  const QMargins cm = contentsMargins();
  int h = qMax(fm.height(), qMax(14, iconSize - 2)) + 2 + tm.top() + tm.bottom() + cm.top() + cm.bottom();
  int w = fm.horizontalAdvance(text()) + 4 + tm.left() + tm.right() + cm.left() + cm.right();
  QStyleOptionFrame opt;
  initStyleOption(&opt);
  return style()->sizeFromContents(QStyle::CT_LineEdit, &opt, QSize(w, h), this);
}

////////////////////////////////////////////////////////////////////////////////

ScriptEdit::ScriptEdit(QWidget* parent /*= nullptr*/)
  : QTextEdit(parent)
{
  setAcceptRichText(false);
  setFont(UI::FixedFont());
  setWordWrapMode(QTextOption::NoWrap);
  setLineWrapMode(QTextEdit::NoWrap);
  setMinimumSize(60, 60);

  m_Error = new QPushButton(tr("!"), this);
  int s = m_Error->sizeHint().height();
  m_Error->resize(s, s);
  m_Error->setStyleSheet(QLatin1String("QPushButton {background-color: #ff244f; color: #ffffff; font-weight: bold;}"));
  m_Error->hide();
  connect(m_Error, &QPushButton::clicked, this, &ScriptEdit::onErrorClicked);
}

QSize ScriptEdit::sizeHint() const
{
  QSize sh = document()->size().toSize() + QSize(20, 20);
  return sh.expandedTo(minimumSize());
}

void ScriptEdit::CheckForErrors()
{
  QString script;
  if (m_Globals)
    script = m_Globals->toPlainText() + QLatin1Char('\n');
  script += toPlainText();
  m_ErrorText = ScriptEngine().evaluate(script);
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
  : QAbstractButton(parent)
  , m_Id(id)
{
  Construct();
}

RoutingCheckBox::RoutingCheckBox(Style checkBoxStyle, size_t id, QWidget* parent /*= nullptr*/)
  : QAbstractButton(parent)
  , m_Style(checkBoxStyle)
  , m_Id(id)
{
  Construct();
}

void RoutingCheckBox::Construct()
{
  setCheckable(true);
  setFixedSize(16, 16);
  connect(this, &QAbstractButton::toggled, this, &RoutingCheckBox::onToggled);
}

const QIcon& RoutingCheckBox::GetIcon(Style checkBoxStyle, bool checked)
{
  switch (checkBoxStyle)
  {
    case Style::Mute:
    {
      if (checked)
      {
        static QIcon iconChecked(QLatin1String(":/qt/etc/images/NetworkOff.svg"));
        return iconChecked;
      }
      else
      {
        static QIcon iconUnchecked(QLatin1String(":/qt/etc/images/NetworkOn.svg"));
        return iconUnchecked;
      }
    }
  }

  if (checked)
  {
    static QIcon iconChecked(QLatin1String(":/qt/etc/images/CheckBoxOn.svg"));
    return iconChecked;
  }

  static QIcon iconUnchecked(QLatin1String(":/qt/etc/images/CheckBoxOff.svg"));
  return iconUnchecked;
}

void RoutingCheckBox::onToggled(bool checked)
{
  emit toggledWithId(m_Id, checked);
}

void RoutingCheckBox::paintEvent(QPaintEvent* /*event*/)
{
  QPainter painter(this);

  const QIcon& icon = GetIcon(isChecked());

  QPixmap& pixmap = (isChecked() ? m_Checked : m_Unchecked);

  qreal dpr = devicePixelRatioF();
  if (dpr < 0 || qFuzzyIsNull(dpr))
    dpr = 1;

  int s = qRound(qMin(width(), height()) * dpr);
  QSize ps(s, s);

  if (pixmap.isNull() || pixmap.size() != ps)
  {
    pixmap = icon.pixmap(ps);
    pixmap.setDevicePixelRatio(dpr);
    if (pixmap.size() != ps)
      pixmap = pixmap.scaled(ps, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
  }

  if (pixmap.isNull())
    return;

  ps = pixmap.size();
  if (pixmap.devicePixelRatio() > 0 && !qFuzzyIsNull(pixmap.devicePixelRatio()))
    ps = QSize(qRound(ps.width() / pixmap.devicePixelRatio()), qRound(ps.height() / pixmap.devicePixelRatio()));

  QPoint pos(qRound((width() - ps.width()) / 2.0), qRound((height() - ps.width()) / 2.0));

  if (isEnabled())
  {
    if (underMouse())
      painter.fillRect(QRect(pos, ps), palette().color(QPalette::Window).lighter());
  }
  else
    painter.setOpacity(0.25);

  painter.drawPixmap(pos, pixmap);
}

////////////////////////////////////////////////////////////////////////////////

SplitterHandle::SplitterHandle(Qt::Orientation orientation, QSplitter* parent)
  : QSplitterHandle(orientation, parent)
{
}

void SplitterHandle::mouseDoubleClickEvent(QMouseEvent* event)
{
  QSplitterHandle::mouseDoubleClickEvent(event);
  emit autoSize(nullptr);
}

////////////////////////////////////////////////////////////////////////////////

Splitter::Splitter(QWidget* parent /*= nullptr*/)
  : QSplitter(parent)
{
}

Splitter::Splitter(Qt::Orientation orientation, QWidget* parent /*= nullptr*/)
  : QSplitter(orientation, parent)
{
}

void Splitter::autoSize(SplitterHandle* splitterHandle)
{
  if (splitterHandle)
  {
    for (int i = 1; i < count(); ++i)
    {
      if (handle(i) == splitterHandle)
      {
        QWidget* w = widget(i - 1);
        if (w)
          moveSplitter(w->x() + w->sizeHint().width(), i);

        return;
      }
    }
  }
  else
  {
    QList<int> defaults = sizes();

    for (int i = 0; i < defaults.size(); ++i)
    {
      QWidget* w = widget(i);
      if (w)
        defaults[i] = w->sizeHint().width();
    }

    setSizes(defaults);
  }

  // update headers
  emit splitterMoved(0, 0);
}

QSplitterHandle* Splitter::createHandle()
{
  SplitterHandle* splitterHandle = new SplitterHandle(orientation(), this);
  connect(splitterHandle, &SplitterHandle::autoSize, this, &Splitter::autoSize);
  return splitterHandle;
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
  if (!m_CachedSizeHint.isValid())
  {
    m_CachedSizeHint = minimumSize();

    for (size_t row = 0; row < m_Rows.size(); ++row)
    {
      const Widgets& widgets = m_Rows[row].widgets;
      for (size_t w = 0; w < widgets.size(); ++w)
      {
        if (!widgets[w]->isHidden())
        {
          m_CachedSizeHint.setWidth(qMax(m_CachedSizeHint.width(), widgets[w]->sizeHint().width()));
          m_CachedSizeHint.setHeight(m_CachedSizeHint.height() + static_cast<int>(Constants::kSpacing) + widgets[w]->sizeHint().height());
        }
      }
    }

    if (m_Rows.size() > 1)
      m_CachedSizeHint.rheight() += static_cast<int>(Constants::kLastRowGap);

    m_CachedSizeHint = m_CachedSizeHint.boundedTo(maximumSize());
  }

  return m_CachedSizeHint;
}

QSize RoutingCol::minimumSizeHint() const
{
  if (!m_CachedMinimumSizeHint.isValid())
  {
    m_CachedMinimumSizeHint = minimumSize();

    for (size_t row = 0; row < m_Rows.size(); ++row)
    {
      const Widgets& widgets = m_Rows[row].widgets;
      for (size_t w = 0; w < widgets.size(); ++w)
      {
        if (!widgets[w]->isHidden())
        {
          m_CachedMinimumSizeHint.setWidth(qMax(m_CachedMinimumSizeHint.width(), widgets[w]->minimumSizeHint().width()));
          m_CachedMinimumSizeHint.setHeight(m_CachedMinimumSizeHint.height() + static_cast<int>(Constants::kSpacing) + widgets[w]->minimumSizeHint().height());
        }
      }
    }

    if (m_Rows.size() > 1)
      m_CachedMinimumSizeHint.rheight() += static_cast<int>(Constants::kLastRowGap);

    m_CachedMinimumSizeHint = m_CachedMinimumSizeHint.boundedTo(maximumSize());
  }

  return m_CachedMinimumSizeHint;
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
  int y = 0;

  size_t lastRow = m_Rows.size() - 1;
  for (size_t row = 0; row < m_Rows.size(); ++row)
  {
    Widgets& widgets = m_Rows[row].widgets;

    int rowHeight = m_Rows[row].height;

    // gap before final [+] row
    if (row == lastRow && m_Rows.size() > 1)
      y += static_cast<int>(Constants::kLastRowGap);

    for (size_t w = 0; w < widgets.size(); ++w)
    {
      widgets[w]->resize(width(), widgets[w]->height());
      widgets[w]->move(qRound((width() - widgets[w]->width()) / 2.0), y);
    }

    y += rowHeight + static_cast<int>(Constants::kSpacing);
  }

  return y;
}

void RoutingCol::ResetCachedSizeHints()
{
  m_CachedSizeHint = QSize();
  m_CachedMinimumSizeHint = QSize();
}

////////////////////////////////////////////////////////////////////////////////

TcpWidget::TcpWidget(QWidget* parent /*= nullptr*/)
  : QWidget(parent)
{
  for (int i = 0; i < static_cast<int>(Col::kCount); ++i)
  {
    m_Headers[i] = new QLabel(HeaderForCol(static_cast<Col>(i)), this);
    m_Headers[i]->setAlignment(Qt::AlignLeft);
  }

  m_Scroll = new QScrollArea(this);

  m_Cols = new Splitter(m_Scroll->viewport());
  m_Scroll->setWidget(m_Cols);
  m_Cols->show();
  for (int i = 0; i < static_cast<int>(Col::kCount); ++i)
  {
    m_RoutingCols[i] = new RoutingCol(m_Cols);
    m_Cols->addWidget(m_RoutingCols[i]);
    m_Cols->setCollapsible(i, false);
    m_Cols->setStretchFactor(i, 1);
    m_RoutingCols[i]->show();

    switch (static_cast<Col>(i))
    {
      case Col::kLabel:
      case Col::kIP: m_Cols->setStretchFactor(i, 3); break;
    }
  }

  connect(m_Cols, &QSplitter::splitterMoved, this, &TcpWidget::updateHeaders);
  connect(m_Scroll->horizontalScrollBar(), &QScrollBar::valueChanged, this, &TcpWidget::updateHeaders);

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

  for (int i = 0; i < static_cast<int>(Col::kCount); ++i)
    m_RoutingCols[i]->ResetCachedSizeHints();

  UpdateLayout();
}

void TcpWidget::AddRow(size_t id, bool remove, const Router::sConnection& connection)
{
  int col = 0;

  Row row;
  row.id = id;
  row.label = new LineEdit(connection.label, m_Cols->widget(col));
  row.label->setToolTip(tr("Text label for this TCP connection"));
  AddCol(col++, row.label);

  row.state = new Indicator(m_Cols->widget(col));
  row.state->setToolTip(tr("Status"));
  row.state->SetColor(MUTED_COLOR);
  row.state->Deactivate();
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

  row.ip = new LineEdit(m_Cols->widget(col));
  row.ip->setToolTip(tr("Server: local network interface for TCP server to run on\n\nClient: IP address of TCP server to connect to"));
  row.ip->setText(connection.addr.ip);
  int fh = row.ip->sizeHint().height();
  row.state->setFixedHeight(fh);
  row.activity->setFixedHeight(fh);
  AddCol(col++, row.ip);

  row.port = new LineEdit(m_Cols->widget(col));
  row.port->setToolTip(tr("Server: local network interface for TCP server to run on\n\nClient: IP address of TCP server to connect to"));
  row.port->setText((connection.addr.port == 0) ? QString() : QString::number(connection.addr.port));
  AddCol(col++, row.port);

  row.addRemove = new RoutingButton(remove ? QLatin1String("-") : QLatin1String("+"), id, m_Cols->widget(col));
  row.addRemove->setToolTip(remove ? tr("Remove this route") : tr("Add this route"));
  connect(row.addRemove, &RoutingButton::clickedWithId, this, &TcpWidget::onAddRemoveClicked);
  AddCol(col++, row.addRemove, row.addRemove->sizeHint().height());

  m_Rows.push_back(row);
}

void TcpWidget::AddCol(int index, QWidget* w, int fixedW /*= -1*/)
{
  RoutingCol* col = qobject_cast<RoutingCol*>(m_Cols->widget(index));
  if (!col)
    return;

  if (fixedW > 0)
  {
    if (col->empty())
    {
      if (index >= 0 && index < static_cast<int>(Col::kCount))
        fixedW = qMax(fixedW, m_Headers[index]->sizeHint().width());

      col->setMinimumWidth(fixedW);
      col->setMaximumWidth(fixedW);
    }
  }
  else if (index >= 0 && index < static_cast<int>(Col::kCount))
    col->setMinimumWidth(qMax(w->minimumSizeHint().width(), m_Headers[index]->sizeHint().width()));

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
      connection.itemStateTableId = itemStateTable->Register(/*mute*/ false);
      row.itemStateTableId = connection.itemStateTableId;
    }
    else
      connection.itemStateTableId = ItemStateTable::sm_Invalid_Id;

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

    if (itemState->state == ItemState::STATE_UNINITIALIZED)
    {
      row.state->setToolTip(tr("Status"));
      row.state->Deactivate();
      row.activity->Deactivate();
      row.activity->SetColor(MUTED_COLOR);
    }
    else
    {
      QString name;
      ItemState::GetStateName(itemState->state, name);
      row.state->setToolTip(name);
      row.state->Activate(0);

      if (itemState->activity)
      {
        row.activity->SetColor(ACTIVITY_COLOR);
        row.activity->Activate(ACTIVITY_TIMEOUT_MS);
      }
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

  const int kMargin = 6;
  int availableWidth = m_Scroll->width() - style()->pixelMetric(QStyle::PM_ScrollBarExtent) - kMargin;
  m_Cols->setGeometry(0, 0, qMax(m_Cols->minimumSizeHint().width(), availableWidth), maxHeight + kMargin);
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

SettingsWidget::SettingsWidget(QSettings& settings, QWidget* parent /*= nullptr*/)
  : QWidget(parent)
  , m_Settings(settings)
{
  setAutoFillBackground(true);

  QGridLayout* settingsLayout = new QGridLayout(this);
  settingsLayout->setContentsMargins(0, 0, 0, 0);
  m_Scroll = new QScrollArea(this);
  settingsLayout->addWidget(m_Scroll);

  QWidget* base = new QWidget(m_Scroll->viewport());
  m_Scroll->setWidget(base);
  m_Scroll->setWidgetResizable(true);

  QGridLayout* grid = new QGridLayout(base);
  grid->setAlignment(Qt::AlignLeft | Qt::AlignTop);
  int row = 0;
  grid->addWidget(new QLabel(tr("Auto Start"), base), row, 0);
  QCheckBox* checkbox = new QCheckBox(base);
  checkbox->setChecked(m_Settings.value(SETTING_AUTO_START).toBool());
  connect(checkbox, &QCheckBox::toggled, this, &SettingsWidget::onAutoStartToggled);
  grid->addWidget(checkbox, row, 1);
  ++row;

  grid->addWidget(new QLabel(tr("sACN Interface"), base), row, 0);
  m_sACNInterface = new QComboBox(base);
  connect(m_sACNInterface, &QComboBox::currentIndexChanged, this, &SettingsWidget::onCurrentIndexChanged);
  grid->addWidget(m_sACNInterface, row, 1);
  ++row;

  grid->addWidget(new QLabel(tr("Artnet Interface"), base), row, 0);
  m_ArtNetInterface = new QComboBox(base);
  connect(m_ArtNetInterface, &QComboBox::currentIndexChanged, this, &SettingsWidget::onCurrentIndexChanged);
  grid->addWidget(m_ArtNetInterface, row, 1);
  ++row;

  QLabel* label = new QLabel(tr("sACN & Artnet: Level Changes Only"), base);
  label->setToolTip(tr("Only act upon incoming sACN & ArtNet when a level in the universe changes"));
  grid->addWidget(label, row, 0);
  m_LevelChangesOnly = new QCheckBox(base);
  m_LevelChangesOnly->setToolTip(label->toolTip());
  grid->addWidget(m_LevelChangesOnly, row, 1);
  ++row;

  grid->addWidget(new QLabel(tr("OTP Interface"), base), row, 0);
  m_OTPInterface = new QComboBox(base);
  connect(m_OTPInterface, &QComboBox::currentIndexChanged, this, &SettingsWidget::onCurrentIndexChanged);
  grid->addWidget(m_OTPInterface, row, 1);
  ++row;

  QHBoxLayout* otpModulesLayout = new QHBoxLayout();
  for (size_t moduleIndex = 0; moduleIndex < m_OTPModules.size(); ++moduleIndex)
  {
    QString moduleName;
    switch (static_cast<otp::ModuleType>(moduleIndex))
    {
      case otp::ModuleType::kPos: moduleName = tr("Position"); break;
      case otp::ModuleType::kPosVelAccel: moduleName = tr("Position Vel/Accel"); break;
      case otp::ModuleType::kRot: moduleName = tr("Rotation"); break;
      case otp::ModuleType::kRotVelAccel: moduleName = tr("Rotation Vel/Accel"); break;
      case otp::ModuleType::kScale: moduleName = tr("Scale"); break;
      case otp::ModuleType::kFrame: moduleName = tr("Reference Frame"); break;
      default: break;
    }

    m_OTPModules[moduleIndex] = new QCheckBox(moduleName, base);
    otpModulesLayout->addWidget(m_OTPModules[moduleIndex]);
  }

  grid->addWidget(new QLabel(tr("Incoming OTP Modules"), base), row, 0);
  grid->addLayout(otpModulesLayout, row, 1, Qt::AlignLeft);
  ++row;

  label = new QLabel(tr("JavaScript Globals"), base);
  label->setToolTip(tr("Declare global JavaScript variables\n\nEx:\nvar gPacketCounter = 0;"));
  grid->addWidget(label, row, 0, Qt::AlignTop);
  m_Script = new ScriptEdit(base);
  m_Script->setToolTip(label->toolTip());
  grid->addWidget(m_Script, row, 1);
  ++row;

  QWidget* cell = new QWidget(base);
  grid->addWidget(cell, row, 0, Qt::AlignTop);
  QHBoxLayout* cellLayout = new QHBoxLayout(cell);
  cellLayout->setContentsMargins(0, 0, 0, 0);
  cellLayout->setAlignment(Qt::AlignLeft);

  label = new QLabel(tr("MIDI Devices"), cell);
  cellLayout->addWidget(label);
  QPushButton* button = new QPushButton(cell);
  button->setIcon(QIcon(":/qt/etc/images/Refresh.svg"));
  button->setToolTip(tr("Refresh MIDI Devices"));
  int buttonSize = button->sizeHint().height();
  button->setFixedSize(buttonSize, buttonSize);
  connect(button, &QPushButton::clicked, this, &SettingsWidget::refreshMIDIDevices);
  cellLayout->addWidget(button);

  m_MIDI = new QTableWidget(0, static_cast<int>(MIDIProp::kCount), base);
  for (int col = 0; col < m_MIDI->columnCount(); ++col)
    m_MIDI->setHorizontalHeaderItem(col, new QTableWidgetItem(MIDIPropName(static_cast<MIDIProp>(col))));
  m_MIDI->verticalHeader()->hide();
  m_MIDI->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
  grid->addWidget(m_MIDI, row, 1);

  Clear();
}

void SettingsWidget::Clear()
{
  PopulateInterfaces(m_sACNInterface, tr("Default (All Interfaces)"));
  PopulateInterfaces(m_ArtNetInterface, tr("Default (First Interface)"));
  PopulateInterfaces(m_OTPInterface, tr("Default (All Interfaces)"));
}

void SettingsWidget::Load(const QStringList& lines)
{
  Router::Settings settings;
  for (QStringList::const_iterator i = lines.begin(); i != lines.end(); i++)
    LoadLine(*i, settings);

  // populate UI
  LoadSettings(settings);

  // save settings from UI and perform error checking
  SaveSettings(settings);

  // load saved settings (that have been error checked)
  LoadSettings(settings);
}

void SettingsWidget::LoadLine(const QString& line, Router::Settings& settings)
{
  QStringList items;
  FileUtils::GetItemsFromQuotedString(line, items);

  if (items.size() >= 3 && items[0].compare(QLatin1String("Settings"), Qt::CaseInsensitive) == 0)
  {
    settings.sACNIP = items[1];
    settings.artNetIP = items[2];
    if (items.size() > 3)
      settings.levelChangesOnly = items[3].toInt() != 0;
    if (items.size() > 4)
      settings.script = items[4];
    if (items.size() > 5)
      settings.otpIP = items[5];
    if (items.size() > 6)
    {
      settings.otpModuleTypes.clear();

      for (qsizetype moduleIndex = 0; moduleIndex < static_cast<qsizetype>(otp::ModuleType::kCount); ++moduleIndex)
      {
        qsizetype offset = moduleIndex + 6;
        if (offset >= items.size())
          break;

        if (items[offset].toInt() != 0)
          settings.otpModuleTypes.insert(static_cast<otp::ModuleType>(moduleIndex));
      }
    }
  }
}

void SettingsWidget::LoadSettings(const Router::Settings& settings)
{
  Clear();
  SetInterface(m_sACNInterface, settings.sACNIP);
  SetInterface(m_ArtNetInterface, settings.artNetIP);
  m_LevelChangesOnly->setChecked(settings.levelChangesOnly);
  SetInterface(m_OTPInterface, settings.otpIP);
  for (size_t moduleIndex = 0; moduleIndex < m_OTPModules.size(); ++moduleIndex)
  {
    bool moduleEnabled = settings.otpModuleTypes.find(static_cast<otp::ModuleType>(moduleIndex)) != settings.otpModuleTypes.end();
    m_OTPModules[moduleIndex]->setChecked(moduleEnabled);
  }
  m_Script->setText(settings.script);
  m_Script->CheckForErrors();
}

void SettingsWidget::Save(QTextStream& stream)
{
  Router::Settings settings;
  SaveSettings(settings);

  stream << QStringLiteral("Settings,%1,%2,%3,%4,%5")
                .arg(FileUtils::QuotedString(settings.sACNIP))
                .arg(FileUtils::QuotedString(settings.artNetIP))
                .arg(settings.levelChangesOnly ? 1 : 0)
                .arg(FileUtils::QuotedString(settings.script))
                .arg(FileUtils::QuotedString(settings.otpIP));

  for (size_t moduleIndex = 0; moduleIndex < m_OTPModules.size(); ++moduleIndex)
  {
    bool moduleEnabled = settings.otpModuleTypes.find(static_cast<otp::ModuleType>(moduleIndex)) != settings.otpModuleTypes.end();
    stream << "," + QString::number(moduleEnabled ? 1 : 0);
  }

  stream << QLatin1Char('\n');
}

void SettingsWidget::SaveSettings(Router::Settings& settings)
{
  settings.sACNIP = GetInterface(m_sACNInterface);
  settings.artNetIP = GetInterface(m_ArtNetInterface);
  settings.otpIP = GetInterface(m_OTPInterface);
  settings.otpModuleTypes.clear();
  for (size_t moduleIndex = 0; moduleIndex < m_OTPModules.size(); ++moduleIndex)
  {
    if (m_OTPModules[moduleIndex]->isChecked())
      settings.otpModuleTypes.insert(static_cast<otp::ModuleType>(moduleIndex));
  }
  settings.levelChangesOnly = m_LevelChangesOnly->isChecked();
  settings.script = m_Script->toPlainText();
}

void SettingsWidget::PopulateInterfaces(QComboBox* combo, const QString& defaultText)
{
  combo->clear();

  combo->addItem(defaultText, QString());

  std::unordered_set<quint32> ips;

  QList<QNetworkInterface> ifaces = QNetworkInterface::allInterfaces();
  for (QList<QNetworkInterface>::const_iterator ifaceIter = ifaces.begin(); ifaceIter != ifaces.end(); ++ifaceIter)
  {
    const QNetworkInterface& iface = *ifaceIter;
    if (!iface.flags().testFlag(QNetworkInterface::IsRunning))
      continue;

    QList<QNetworkAddressEntry> addrs = iface.addressEntries();
    for (QList<QNetworkAddressEntry>::const_iterator addrIter = addrs.begin(); addrIter != addrs.end(); ++addrIter)
    {
      QHostAddress addr = addrIter->ip();
      if (addr.protocol() != QAbstractSocket::IPv4Protocol)
        continue;

      quint32 ip = addr.toIPv4Address();
      if (ips.find(ip) != ips.end())
        continue;  // already added

      QString ipStr = addr.toString();
      combo->addItem(QStringLiteral("%1 (%2)").arg(ipStr).arg(iface.humanReadableName()), ipStr);
    }
  }
}

QString SettingsWidget::GetInterface(QComboBox* combo)
{
  return combo->currentData().toString();
}

void SettingsWidget::SetInterface(QComboBox* combo, const QString& ip)
{
  if (ip.isEmpty())
  {
    combo->setCurrentIndex(0);
    return;
  }

  int index = combo->findData(ip, Qt::UserRole, Qt::MatchFixedString);
  if (index >= 0)
  {
    combo->setCurrentIndex(index);
    return;
  }

  QHostAddress addr(ip);
  if (addr.protocol() == QAbstractSocket::IPv4Protocol && addr.toIPv4Address() != 0)
  {
    QString addrIP = addr.toString();
    combo->addItem(QStringLiteral("%1 (%2)").arg(addrIP).arg(tr("Not Found")), addrIP);
    index = combo->count() - 1;
    combo->setItemData(index, ERROR_COLOR, Qt::ForegroundRole);
    combo->setCurrentIndex(index);
  }
}

QString SettingsWidget::MIDIPropName(MIDIProp prop)
{
  switch (prop)
  {
    case MIDIProp::kType: return tr("Type");
    case MIDIProp::kName: return tr("Name");
    case MIDIProp::kPort: return tr("Port");
    default: break;
  }

  return QString();
}

void SettingsWidget::showEvent(QShowEvent* /*event*/)
{
  refreshMIDIDevices();
}

void SettingsWidget::onAutoStartToggled(bool checked)
{
  m_Settings.setValue(SETTING_AUTO_START, checked);
}

void SettingsWidget::onCurrentIndexChanged(int index)
{
  QComboBox* combo = qobject_cast<QComboBox*>(sender());
  if (!combo)
    return;

  QPalette pal = combo->palette();
  pal.setColor(QPalette::ButtonText, palette().color(QPalette::ButtonText));

  if (index >= 0)
  {
    QVariant v = combo->itemData(index, Qt::ForegroundRole);
    if (!v.isNull())
      pal.setColor(QPalette::ButtonText, v.value<QBrush>().color());
  }

  combo->setPalette(pal);
}

void SettingsWidget::refreshMIDIDevices()
{
  MIDIDeviceList devices;

  try
  {
    std::vector<RtMidi::Api> apis;
    RtMidi::getCompiledApi(apis);

    for (const RtMidi::Api& api : apis)
    {
      // midi in
      {
        std::unique_ptr<RtMidiIn> midiIn = std::make_unique<RtMidiIn>(api);

        unsigned int portCount = midiIn->getPortCount();
        for (unsigned int port = 0; port < portCount; ++port)
        {
          MIDIDevice device;
          device.props[static_cast<size_t>(MIDIProp::kName)] = QString::fromStdString(midiIn->getPortName(port));
          device.props[static_cast<size_t>(MIDIProp::kPort)] = QString::number(port);
          device.props[static_cast<size_t>(MIDIProp::kType)] = tr("Input");
          device.color = RECV_COLOR;
          devices.push_back(device);
        }
      }

      // midi out
      {
        std::unique_ptr<RtMidiOut> midiOut = std::make_unique<RtMidiOut>(api);

        unsigned int portCount = midiOut->getPortCount();
        for (unsigned int port = 0; port < portCount; ++port)
        {
          midiOut->getPortName(port);
          MIDIDevice device;
          device.props[static_cast<size_t>(MIDIProp::kName)] = QString::fromStdString(midiOut->getPortName(port));
          device.props[static_cast<size_t>(MIDIProp::kPort)] = QString::number(port);
          device.props[static_cast<size_t>(MIDIProp::kType)] = tr("Output");
          device.color = SEND_COLOR;
          devices.push_back(device);
        }
      }
    }
  }
  catch (RtMidiError&)
  {
  }

  m_MIDI->setRowCount(static_cast<int>(devices.size()));
  for (int row = 0; row < static_cast<int>(devices.size()); ++row)
  {
    const MIDIDevice& device = devices[static_cast<size_t>(row)];
    for (int col = 0; col < static_cast<int>(MIDIProp::kCount); ++col)
    {
      QTableWidgetItem* item = m_MIDI->item(row, col);
      if (!item)
      {
        item = new QTableWidgetItem();
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        item->setForeground(device.color);
        m_MIDI->setItem(row, col, item);
      }

      item->setText(device.props[static_cast<size_t>(col)]);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////

ProtocolComboBox::ProtocolComboBox(size_t row, Protocol protocol, QWidget* parent /*= nullptr*/)
  : QComboBox(parent)
  , m_Row(row)
{
  setToolTip(tr("Protocol"));

  addItem(ProtocolName(Protocol::kOSC), static_cast<int>(Protocol::kOSC));
  addItem(ProtocolName(Protocol::ksACN), static_cast<int>(Protocol::ksACN));
  addItem(ProtocolName(Protocol::kArtNet), static_cast<int>(Protocol::kArtNet));
  addItem(ProtocolName(Protocol::kMIDI), static_cast<int>(Protocol::kMIDI));
  addItem(ProtocolName(Protocol::kPSN), static_cast<int>(Protocol::kPSN));
  addItem(ProtocolName(Protocol::kOTP), static_cast<int>(Protocol::kOTP));

  int index = findData(static_cast<int>(SanitizedProtocol(static_cast<int>(protocol))));
  if (index >= 0)
    setCurrentIndex(index);

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
    case Protocol::ksACN: return tr("sACN");
    case Protocol::kArtNet: return tr("ArtNet");
    case Protocol::kMIDI: return tr("MIDI");
    case Protocol::kOTP: return tr("OTP");
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
  m_Incoming.base = new QWidget(this);
  QHBoxLayout* headerLayout = new QHBoxLayout(m_Incoming.base);
  headerLayout->setSpacing(6);
  headerLayout->setContentsMargins(QMargins());
  headerLayout->addStretch(std::numeric_limits<int>::max());
  QLabel* label = new QLabel(tr("Incoming"), m_Incoming.base);
  QFont fnt = label->font();
  fnt.setPointSize(12);
  label->setFont(fnt);
  headerLayout->addWidget(label);
  m_Incoming.mute = new RoutingCheckBox(RoutingCheckBox::Style::Mute, 0, m_Incoming.base);
  m_Incoming.mute->setToolTip(tr("Mute all running input"));
  connect(m_Incoming.mute, &RoutingCheckBox::toggledWithId, this, &RoutingWidget::onMuteToggled);
  headerLayout->addWidget(m_Incoming.mute, 0, Qt::AlignCenter);
  headerLayout->addStretch(std::numeric_limits<int>::max());

  m_Outgoing.base = new QWidget(this);
  headerLayout = new QHBoxLayout(m_Outgoing.base);
  headerLayout->setSpacing(6);
  headerLayout->setContentsMargins(QMargins());
  headerLayout->addStretch(std::numeric_limits<int>::max());
  label = new QLabel(tr("Outgoing"), m_Outgoing.base);
  label->setFont(fnt);
  headerLayout->addWidget(label);
  m_Outgoing.mute = new RoutingCheckBox(RoutingCheckBox::Style::Mute, 1, m_Outgoing.base);
  m_Outgoing.mute->setToolTip(tr("Mute all running output"));
  connect(m_Outgoing.mute, &RoutingCheckBox::toggledWithId, this, &RoutingWidget::onMuteToggled);
  headerLayout->addWidget(m_Outgoing.mute, 0, Qt::AlignCenter);
  headerLayout->addStretch(std::numeric_limits<int>::max());

  for (int i = 0; i < static_cast<int>(Col::kCount); ++i)
  {
    QWidget* header = nullptr;

    switch (static_cast<Col>(i))
    {
      case Col::kInPath:
      case Col::kOutPath:
      {
        header = new QWidget(this);
        headerLayout = new QHBoxLayout(header);
        headerLayout->setSpacing(6);
        headerLayout->setContentsMargins(QMargins());
        label = new QLabel(HeaderForCol(static_cast<Col>(i)), header);
        headerLayout->addWidget(label);
        RoutingButton* button = new RoutingButton(QLatin1String("?"), static_cast<size_t>(i), header);
        button->setToolTip(tr("Show Help..."));
        connect(button, &RoutingButton::clickedWithId, this, &RoutingWidget::onHeaderHelpClicked);
        int n = button->sizeHint().height();
        button->setFixedWidth(n);
        headerLayout->addWidget(button);
        headerLayout->addStretch(std::numeric_limits<int>::max());
      }
      break;

      default:
        header = new QLabel(HeaderForCol(static_cast<Col>(i)), this);
        static_cast<QLabel*>(header)->setAlignment(Qt::AlignLeft);
        break;
    }

    m_Headers[i] = header;
  }
  m_Headers[static_cast<int>(Col::kOutScript)]->setToolTip(tr("JavaScript"));

  m_Scroll = new QScrollArea(this);

  m_Cols = new Splitter(m_Scroll->viewport());
  m_Scroll->setWidget(m_Cols);
  m_Cols->show();
  for (int i = 0; i < static_cast<int>(Col::kCount); ++i)
  {
    m_RoutingCols[i] = new RoutingCol(m_Cols);
    m_Cols->addWidget(m_RoutingCols[i]);
    m_Cols->setCollapsible(i, false);
    m_Cols->setStretchFactor(i, 1);
    m_RoutingCols[i]->show();

    switch (static_cast<Col>(i))
    {
      case Col::kLabel:
      case Col::kInIP:
      case Col::kOutIP: m_Cols->setStretchFactor(i, 3); break;

      case Col::kInPath:
      case Col::kOutPath: m_Cols->setStretchFactor(i, 8); break;
    }
  }

  connect(m_Cols, &QSplitter::splitterMoved, this, &RoutingWidget::updateHeaders);
  connect(m_Scroll->horizontalScrollBar(), &QScrollBar::valueChanged, this, &RoutingWidget::updateHeaders);

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
    case Col::kEnable: return tr("On");

    case Col::kMute: return tr("Mute");

    case Col::kLabel: return tr("Name");

    case Col::kInProtocol:
    case Col::kOutProtocol: return tr("Protocol");

    case Col::kInIP:
    case Col::kOutIP: return tr("IP");

    case Col::kInPort:
    case Col::kOutPort: return tr("Port");

    case Col::kInTCP:
    case Col::kOutTCP: return tr("Proto");

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

void RoutingWidget::LoadRoutes(const Router::ROUTES& routes, const ItemStateTable& itemStateTable)
{
  Clear();

  bool b = m_Incoming.mute->blockSignals(true);
  m_Incoming.mute->setChecked(itemStateTable.GetMuteAllIncoming());
  m_Incoming.mute->blockSignals(b);

  b = m_Outgoing.mute->blockSignals(true);
  m_Outgoing.mute->setChecked(itemStateTable.GetMuteAllOutgoing());
  m_Outgoing.mute->blockSignals(b);

  size_t id = 0;
  m_Rows.reserve(routes.size());
  for (Router::ROUTES::const_iterator i = routes.begin(); i != routes.end(); i++)
    AddRow(id++, /*remove*/ true, i->label, *i);

  AddRow(id++, /*remove*/ false, QString(), Router::sRoute());

  for (int i = 0; i < static_cast<int>(Col::kCount); ++i)
    m_RoutingCols[i]->ResetCachedSizeHints();

  m_Cols->autoSize(nullptr);

  UpdateLayout();
  UpdateEnableState();
  UpdateMuteState();
  UpdateTcpIndicators();
}

void RoutingWidget::SetTcpConnections(const Router::CONNECTIONS& connections)
{
  m_TcpConnections = connections;
  UpdateTcpIndicators();
}

void RoutingWidget::AddRow(size_t id, bool remove, const QString& label, const Router::sRoute& route)
{
  int col = 0;

  Row row;
  row.id = id;

  row.enable = new RoutingCheckBox(id, m_Cols->widget(col));
  row.enable->setToolTip(tr("Enable this route - enforced when you click [Start]"));
  row.enable->setChecked(route.enable);
  connect(row.enable, &RoutingCheckBox::toggledWithId, this, &RoutingWidget::onEnableToggled);
  AddCol(col++, row.enable, /*fixed*/ true);

  row.mute = new RoutingCheckBox(RoutingCheckBox::Style::Mute, id, m_Cols->widget(col));
  row.mute->setToolTip(tr("Mute running route"));
  row.mute->setChecked(route.mute);
  connect(row.mute, &RoutingCheckBox::toggledWithId, this, &RoutingWidget::onMuteRouteToggled);
  AddCol(col++, row.mute, /*fixed*/ true);

  row.label = new LineEdit(label, m_Cols->widget(col));
  row.label->setToolTip(tr("Text label for this route"));
  AddCol(col++, row.label);

  row.inState = new Indicator(m_Cols->widget(col));
  row.inState->setToolTip(tr("Status"));
  row.inState->SetColor(MUTED_COLOR);
  row.inState->Deactivate();
  AddCol(col++, row.inState, /*fixed*/ true);

  row.inActivity = new Indicator(m_Cols->widget(col));
  row.inActivity->setToolTip(tr("Activity"));
  row.inActivity->SetColor(MUTED_COLOR);
  row.inActivity->Deactivate();
  AddCol(col++, row.inActivity, /*fixed*/ true);

  row.inProtocol = new ProtocolComboBox(id, route.src.protocol, m_Cols->widget(col));
  connect(row.inProtocol, &ProtocolComboBox::protocolChanged, this, &RoutingWidget::onInProtocolChanged);
  AddCol(col++, row.inProtocol, /*fixed*/ true);

  row.inIP = new LineEdit(m_Cols->widget(col));
  if (route.src.multicastInterfaceIP.isEmpty())
    row.inIP->setText(route.src.addr.ip);
  else
    row.inIP->setText(route.src.addr.ip + QLatin1Char(',') + route.src.multicastInterfaceIP);
  AddCol(col++, row.inIP);

  row.inPort = new LineEdit(m_Cols->widget(col));
  row.inPort->setText(ValidPort(route.src.protocol, route.src.addr.port) ? QString::number(route.src.addr.port) : QString());
  AddCol(col++, row.inPort);

  row.inTCP = new QLabel(tr("UDP"), m_Cols->widget(col));
  row.inTCP->setAlignment(Qt::AlignCenter);
  row.inTCP->setToolTip(tr("Incoming transport for this endpoint."));
  AddCol(col++, row.inTCP, /*fixed*/ true);

  row.inPath = new LineEdit(m_Cols->widget(col));
  row.inPath->setText(route.src.path);
  int fh = row.inPath->sizeHint().height();
  row.enable->setFixedHeight(fh);
  row.mute->setFixedHeight(fh);
  row.inState->setFixedHeight(fh);
  row.inActivity->setFixedHeight(fh);
  row.inTCP->setFixedHeight(fh);
  AddCol(col++, row.inPath);

  row.inMin = new LineEdit(m_Cols->widget(col));
  row.inMin->setToolTip(tr("Clip first outgoing OSC argument\n\nScale first outgoing OSC argument when all min/max fields populated"));
  QString transformStr;
  TransformToString(route.dst.inMin, transformStr);
  row.inMin->setText(transformStr);
  AddCol(col++, row.inMin);

  row.inMax = new LineEdit(m_Cols->widget(col));
  row.inMax->setToolTip(tr("Clip first outgoing OSC argument\n\nScale first outgoing OSC argument when all min/max fields populated"));
  TransformToString(route.dst.inMax, transformStr);
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
  row.divider->setFixedSize(48, fh);
  AddCol(col++, row.divider, /*fixed*/ true);

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

  row.outProtocol = new ProtocolComboBox(id, route.dst.protocol, m_Cols->widget(col));
  connect(row.outProtocol, &ProtocolComboBox::protocolChanged, this, &RoutingWidget::onOutProtocolChanged);
  AddCol(col++, row.outProtocol, /*fixed*/ true);

  row.outIP = new LineEdit(m_Cols->widget(col));
  if (route.dst.multicastInterfaceIP.isEmpty())
    row.outIP->setText(route.dst.addr.ip);
  else
    row.outIP->setText(route.dst.addr.ip + QLatin1Char(',') + route.dst.multicastInterfaceIP);
  AddCol(col++, row.outIP);

  row.outPort = new LineEdit(m_Cols->widget(col));
  row.outPort->setText(ValidPort(route.dst.protocol, route.dst.addr.port) ? QString::number(route.dst.addr.port) : QString());
  AddCol(col++, row.outPort);

  row.outTCP = new QLabel(tr("UDP"), m_Cols->widget(col));
  row.outTCP->setAlignment(Qt::AlignCenter);
  row.outTCP->setToolTip(tr("Outgoing transport for this endpoint."));
  AddCol(col++, row.outTCP, /*fixed*/ true);

  row.outPath = new LineEdit(m_Cols->widget(col));
  row.outPath->setText(route.dst.path);
  fh = row.outPath->sizeHint().height();
  row.outState->setFixedHeight(fh);
  row.outActivity->setFixedHeight(fh);
  row.outTCP->setFixedHeight(fh);

  row.outScriptText = new ScriptEdit(m_Cols->widget(col));
  row.outScriptText->hide();
  row.outScriptText->setText(route.dst.scriptText);
  row.outScriptText->SetGlobals(m_Globals);
  row.outScriptText->CheckForErrors();
  AddCol(col++, {row.outPath, row.outScriptText});
  row.outPath->setVisible(!route.dst.script);
  row.outScriptText->setVisible(route.dst.script);

  row.outScript = new RoutingCheckBox(id, m_Cols->widget(col));
  row.outScript->setToolTip(tr("JavaScript"));
  row.outScript->setFixedHeight(row.outPath->sizeHint().height());
  row.outScript->setChecked(route.dst.script);
  connect(row.outScript, &RoutingCheckBox::toggledWithId, this, &RoutingWidget::onOutScriptToggled);
  AddCol(col++, row.outScript, /*fixed*/ true);

  row.outMin = new LineEdit(m_Cols->widget(col));
  row.outMin->setToolTip(tr("Clip first outgoing OSC argument\n\nScale first outgoing OSC argument when all min/max fields populated"));
  TransformToString(route.dst.outMin, transformStr);
  row.outMin->setText(transformStr);
  AddCol(col++, row.outMin);

  row.outMax = new LineEdit(m_Cols->widget(col));
  row.outMax->setToolTip(tr("Clip first outgoing OSC argument\n\nScale first outgoing OSC argument when all min/max fields populated"));
  TransformToString(route.dst.outMax, transformStr);
  row.outMax->setText(transformStr);
  AddCol(col++, row.outMax);

  row.addRemove = new RoutingButton(remove ? QLatin1String("-") : QLatin1String("+"), id, m_Cols->widget(col));
  row.addRemove->setToolTip(remove ? tr("Remove this route") : tr("Add this route"));
  connect(row.addRemove, &RoutingButton::clickedWithId, this, &RoutingWidget::onAddRemoveClicked);
  AddCol(col++, row.addRemove, /*fixed*/ true, /*fixedHeight*/ true);

  m_Rows.push_back(row);

  onInProtocolChanged(m_Rows.size() - 1, Protocol::kInvalid);
  onOutProtocolChanged(m_Rows.size() - 1, Protocol::kInvalid);
}

void RoutingWidget::AddCol(int index, QWidget* w, bool fixed /*= false*/, bool fixedHeight /*= false*/)
{
  RoutingCol::Widgets widgets = {w};
  AddCol(index, widgets, fixed, fixedHeight);
}

void RoutingWidget::AddCol(int index, const RoutingCol::Widgets& w, bool fixed /*= false*/, bool fixedHeight /*= false*/)
{
  RoutingCol* col = qobject_cast<RoutingCol*>(m_Cols->widget(index));
  if (!col)
    return;

  if (!w.empty())
  {
    if (fixed)
    {
      if (col->empty())
      {
        int fw = fixedHeight ? w.front()->sizeHint().height() : w.front()->sizeHint().width();
        if (index >= 0 && index < static_cast<int>(Col::kCount))
          fw = qMax(fw, m_Headers[index]->sizeHint().width());

        col->setMinimumWidth(fw);
        col->setMaximumWidth(fw);
      }
    }
    else if (index >= 0 && index < static_cast<int>(Col::kCount))
      col->setMinimumWidth(qMax(w.front()->minimumSizeHint().width(), m_Headers[index]->sizeHint().width()));
  }

  col->AddWidgets(w);
}

void RoutingWidget::Load(const QStringList& lines)
{
  Router::ROUTES routes;
  ItemStateTable itemStateTable;
  for (QStringList::const_iterator i = lines.begin(); i != lines.end(); i++)
    LoadLine(*i, routes, itemStateTable);

  // populate UI
  LoadRoutes(routes, itemStateTable);

  // save routes from UI and perform error checking
  SaveRoutes(routes, itemStateTable);

  // load saved routes (that have been error checked)
  LoadRoutes(routes, itemStateTable);
}

void RoutingWidget::LoadLine(const QString& line, Router::ROUTES& routes, ItemStateTable& itemStateTable)
{
  QStringList items;
  FileUtils::GetItemsFromQuotedString(line, items);
  if (items.isEmpty())
    return;

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
      route.src.multicastInterfaceIP = items[12];

    if (items.size() > 13)
      route.src.protocol = ProtocolComboBox::SanitizedProtocol(items[13].toInt());

    if (items.size() > 14)
      route.dst.protocol = ProtocolComboBox::SanitizedProtocol(items[14].toInt());

    if (items.size() > 15)
      route.enable = (items[15].toInt() != 0);

    if (items.size() > 16)
      route.mute = (items[16].toInt() == 0);

    if (items.size() > 17)
      route.dst.multicastInterfaceIP = items[17];

    routes.push_back(route);
  }
  else if (items.size() == 3 && items[0].compare(QLatin1String("Mute"), Qt::CaseInsensitive) == 0)
  {
    itemStateTable.SetMuteAllIncoming(items[0].toInt() != 0);
    itemStateTable.SetMuteAllOutgoing(items[1].toInt() != 0);
  }
}

void RoutingWidget::Save(QTextStream& stream)
{
  Router::ROUTES routes;
  ItemStateTable itemStateTable;
  SaveRoutes(routes, itemStateTable);

  stream << QStringLiteral("Mute,%1,%2\n").arg(itemStateTable.GetMuteAllIncoming() ? 1 : 0).arg(itemStateTable.GetMuteAllOutgoing() ? 1 : 0);

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
    stream << QStringLiteral(",%1").arg(FileUtils::QuotedString(route.src.multicastInterfaceIP));
    stream << QStringLiteral(",%1").arg(static_cast<int>(route.src.protocol));
    stream << QStringLiteral(",%1").arg(static_cast<int>(route.dst.protocol));
    stream << QStringLiteral(",%1").arg(route.enable ? 1 : 0);
    stream << QStringLiteral(",%1").arg(route.mute ? 0 : 1);
    stream << QStringLiteral(",%1").arg(FileUtils::QuotedString(route.dst.multicastInterfaceIP));
    stream << QLatin1Char('\n');
  }
}

void RoutingWidget::SaveRoutes(Router::ROUTES& routes, ItemStateTable& itemStateTable)
{
  routes.clear();

  itemStateTable.Clear();

  itemStateTable.SetMuteAllIncoming(m_Incoming.mute->isChecked());
  itemStateTable.SetMuteAllOutgoing(m_Outgoing.mute->isChecked());

  // show state/activity per EosAddr
  AddrStates srcAddrStates;
  AddrStates dstAddrStates;

  for (size_t i = 0; i < m_Rows.size(); i++)
  {
    Row& row = m_Rows[i];

    Router::sRoute route;
    route.src.protocol = row.inProtocol->GetProtocol();

    route.src.addr.port = row.inPort->text().toUShort();
    if (!ValidPort(route.src.protocol, route.src.addr.port))
      continue;  // port required

    route.enable = row.enable->isChecked();
    route.mute = row.mute->isChecked();
    route.label = row.label->text();

    QStringList ips = row.inIP->text().split(QLatin1Char(','));
    if (ips.size() > 1)
    {
      route.src.addr.ip = ips[0].trimmed();
      route.src.multicastInterfaceIP = ips[1].trimmed();
    }
    else
      route.src.addr.ip = row.inIP->text();

    route.src.path = row.inPath->text();

    ips = row.outIP->text().split(QLatin1Char(','));
    if (ips.size() > 1)
    {
      route.dst.addr.ip = ips[0].trimmed();
      route.dst.multicastInterfaceIP = ips[1].trimmed();
    }
    else
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

    AddrStates::const_iterator j = srcAddrStates.find(route.src.addr);
    if (j == srcAddrStates.end())
      srcAddrStates[route.src.addr] = route.srcItemStateTableId = itemStateTable.Register(/*mute*/ false);
    else
      route.srcItemStateTableId = j->second;
    row.inItemStateTableId = route.srcItemStateTableId;

    j = dstAddrStates.find(route.dst.addr);
    if (j == dstAddrStates.end())
      dstAddrStates[route.dst.addr] = route.dstItemStateTableId = itemStateTable.Register(row.mute->isChecked());
    else
      route.dstItemStateTableId = j->second;
    row.outItemStateTableId = route.dstItemStateTableId;

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

bool RoutingWidget::RouteUsesTcp(const Router::CONNECTIONS& connections, const EosAddr& addr, bool output)
{
  if (addr.port == 0)
    return false;

  for (Router::CONNECTIONS::const_iterator i = connections.begin(); i != connections.end(); ++i)
  {
    const Router::sConnection& connection = *i;
    if (connection.addr == addr && (!output || !connection.server))
      return true;
  }

  return false;
}

void RoutingWidget::UpdateTcpIndicators()
{
  const QString tcpStyle = QStringLiteral("QLabel { background-color: rgb(29, 92, 58); color: white; border-radius: 7px; padding: 0px 4px; }");
  const QString udpStyle = QStringLiteral("QLabel { background-color: rgb(56, 76, 102); color: white; border-radius: 7px; padding: 0px 4px; }");

  for (size_t i = 0; i < m_Rows.size(); ++i)
  {
    Row& row = m_Rows[i];

    EosAddr inAddr;
    inAddr.port = row.inPort->text().toUShort();
    inAddr.ip = row.inIP->text().split(QLatin1Char(',')).value(0).trimmed();
    bool inTcp = (row.inProtocol->GetProtocol() == Protocol::kOSC && RouteUsesTcp(m_TcpConnections, inAddr, /*output*/ false));
    row.inTCP->setText(inTcp ? tr("TCP") : tr("UDP"));
    row.inTCP->setStyleSheet(inTcp ? tcpStyle : udpStyle);
    row.inTCP->setToolTip(inTcp ? tr("This incoming endpoint matches a configured TCP connection.") : tr("This incoming endpoint is routed over UDP."));

    EosAddr outAddr;
    outAddr.port = row.outPort->text().toUShort();
    outAddr.ip = row.outIP->text().split(QLatin1Char(',')).value(0).trimmed();
    bool outTcp = (row.outProtocol->GetProtocol() == Protocol::kOSC && RouteUsesTcp(m_TcpConnections, outAddr, /*output*/ true));
    row.outTCP->setText(outTcp ? tr("TCP") : tr("UDP"));
    row.outTCP->setStyleSheet(outTcp ? tcpStyle : udpStyle);
    row.outTCP->setToolTip(outTcp ? tr("This outgoing endpoint matches a configured TCP connection.") : tr("This outgoing endpoint is routed over UDP."));
  }

  for (int i = 0; i < static_cast<int>(Col::kCount); ++i)
    m_RoutingCols[i]->ResetCachedSizeHints();

  UpdateLayout();
}

void RoutingWidget::UpdateItemState(const ItemState* itemState, Indicator& stateIndicator, Indicator& activityIndicator)
{
  if (!(itemState && itemState->dirty))
    return;

  QColor color;
  ItemState::GetStateColor(itemState->state, color);
  stateIndicator.SetColor(color);

  if (itemState->state == ItemState::STATE_UNINITIALIZED)
  {
    stateIndicator.setToolTip(tr("Status"));
    stateIndicator.Deactivate();
    activityIndicator.Deactivate();
    activityIndicator.SetColor(MUTED_COLOR);
  }
  else
  {
    QString name;
    ItemState::GetStateName(itemState->state, name);
    stateIndicator.setToolTip(name);
    stateIndicator.Activate(0);

    if (itemState->activity)
    {
      activityIndicator.SetColor(ACTIVITY_COLOR);
      activityIndicator.Activate(ACTIVITY_TIMEOUT_MS);
    }
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

  int half = static_cast<int>(RoutingCol::Constants::kSpacing) / 2;
  int y = m_Incoming.base->sizeHint().height() + half;
  int h = height() - y;
  int x1 = RectForCol(Col::kInState).left() - half;
  int x2 = RectForCol(Col::kInMax).right() + static_cast<int>(RoutingCol::Constants::kSpacing);
  painter.fillRect(QRect(x1, y, x2 - x1, h), QColor(45, 45, 45));

  x1 = RectForCol(Col::kOutState).left() - static_cast<int>(RoutingCol::Constants::kSpacing);
  x2 = RectForCol(Col::kOutMax).right() + static_cast<int>(RoutingCol::Constants::kSpacing);
  painter.fillRect(QRect(x1, y, x2 - x1, h), QColor(45, 45, 45));
}

void RoutingWidget::UpdateLayout()
{
  bool b = m_Cols->blockSignals(true);
  int y = m_Incoming.base->sizeHint().height() + static_cast<int>(RoutingCol::Constants::kSpacing);
  y += m_Headers[static_cast<int>(Col::kInPath)]->sizeHint().height() + static_cast<int>(RoutingCol::Constants::kSpacing);

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

  const int kMargin = 6;
  int availableWidth = m_Scroll->width() - style()->pixelMetric(QStyle::PM_ScrollBarExtent) - kMargin;
  m_Cols->setGeometry(0, 0, qMax(m_Cols->minimumSizeHint().width(), availableWidth), maxHeight + kMargin);
  m_Cols->blockSignals(b);

  updateHeaders();
}

void RoutingWidget::updateHeaders()
{
  int x1 = RectForCol(Col::kInState).left();
  int x2 = RectForCol(Col::kInMax).right();
  m_Incoming.base->setGeometry(x1, 0, x2 - x1, m_Incoming.base->sizeHint().height());

  x1 = RectForCol(Col::kOutState).left();
  x2 = RectForCol(Col::kOutMax).right();
  m_Outgoing.base->setGeometry(x1, 0, x2 - x1, m_Outgoing.base->sizeHint().height());

  int y = m_Incoming.base->height() + static_cast<int>(RoutingCol::Constants::kSpacing);
  for (int i = 0; i < static_cast<int>(Col::kCount); ++i)
  {
    QRect r = RectForCol(static_cast<Col>(i));
    m_Headers[i]->setGeometry(r.x(), y, r.width(), m_Headers[static_cast<int>(Col::kInPath)]->sizeHint().height());
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

void SetMuted(QWidget* w, bool b)
{
  QPalette pal = w->palette();
  pal.setColor(QPalette::Active, QPalette::WindowText, b ? OFF_COLOR : TEXT_COLOR);
  pal.setColor(QPalette::Active, QPalette::ButtonText, b ? OFF_COLOR : TEXT_COLOR);
  pal.setColor(QPalette::Active, QPalette::Text, b ? OFF_COLOR : TEXT_COLOR);
  w->setPalette(pal);
}

void RoutingWidget::UpdateEnableState()
{
  size_t lastRow = m_Rows.size() - 1;
  for (size_t i = 0; i < m_Rows.size(); ++i)
  {
    Row& row = m_Rows[i];

    bool e = row.enable->isChecked();
    Protocol protocol = row.inProtocol->GetProtocol();

    row.mute->setEnabled(e && i != lastRow);
    row.label->setEnabled(e);
    row.inIP->setEnabled(e && protocol != Protocol::kMIDI && protocol != Protocol::kOTP);
    row.inPort->setEnabled(e);
    row.inTCP->setEnabled(e);
    row.inProtocol->setEnabled(e);
    row.inPath->setEnabled(e && protocol != Protocol::ksACN && protocol != Protocol::kArtNet);
    row.inMin->setEnabled(e);
    row.inMax->setEnabled(e);

    row.divider->setEnabled(e);

    protocol = row.outProtocol->GetProtocol();

    row.outIP->setEnabled(e && protocol != Protocol::kMIDI && protocol != Protocol::kOTP);
    row.outPort->setEnabled(e);
    row.outTCP->setEnabled(e);
    row.outProtocol->setEnabled(e);
    row.outPath->setEnabled(e);
    row.outScriptText->setEnabled(e);
    row.outScript->setEnabled(e);
    row.outMin->setEnabled(e);
    row.outMax->setEnabled(e);
  }
}

void RoutingWidget::UpdateMuteState()
{
  bool muteAllIncoming = m_Incoming.mute->isChecked();
  bool muteAllOutgoing = m_Outgoing.mute->isChecked();

  if (m_Rows.empty())
    return;

  size_t lastRow = m_Rows.size() - 1;
  for (size_t i = 0; i < lastRow; ++i)
  {
    Row& row = m_Rows[i];

    bool muteRoute = row.mute->isChecked();
    bool mute = muteAllIncoming || muteRoute;

    SetMuted(row.inIP, mute);
    SetMuted(row.inPort, mute);
    SetMuted(row.inTCP, mute);
    SetMuted(row.inProtocol, mute);
    SetMuted(row.inPath, mute);
    SetMuted(row.inMin, mute);
    SetMuted(row.inMax, mute);

    mute = muteAllOutgoing || muteRoute;

    SetMuted(row.outIP, mute);
    SetMuted(row.outPort, mute);
    SetMuted(row.outTCP, mute);
    SetMuted(row.outProtocol, mute);
    SetMuted(row.outPath, mute);
    SetMuted(row.outMin, mute);
    SetMuted(row.outMax, mute);
  }
}

void RoutingWidget::onEnableToggled(size_t /*id*/, bool /*checked*/)
{
  UpdateEnableState();
}

void RoutingWidget::onOutScriptToggled(size_t id, bool checked)
{
  if (id >= m_Rows.size())
    return;

  m_Rows[id].outPath->setVisible(!checked);
  m_Rows[id].outScriptText->setVisible(checked);
  UpdateLayout();
}

void RoutingWidget::onMuteToggled(size_t id, bool checked)
{
  UpdateMuteState();
  bool incoming = (id == 0);
  emit muteToggled(incoming, checked);
}

void RoutingWidget::onMuteRouteToggled(size_t row, bool checked)
{
  if (row >= m_Rows.size())
    return;

  UpdateMuteState();
  emit muteRouteToggled(m_Rows[row].outItemStateTableId, checked);
}

void RoutingWidget::onAddRemoveClicked(size_t id)
{
  if (id >= m_Rows.size())
    return;

  if (id == (m_Rows.size() - 1))
    AddRow(m_Rows.size() - 1, /*remove*/ false, QString(), Router::sRoute());  // add new route
  else
    m_Rows.erase(m_Rows.begin() + id);

  Router::ROUTES routes;
  ItemStateTable itemStateTable;
  SaveRoutes(routes, itemStateTable);
  LoadRoutes(routes, itemStateTable);
}

void RoutingWidget::onInProtocolChanged(size_t row, Protocol protocol)
{
  if (row >= m_Rows.size())
    return;

  const Row& r = m_Rows[row];
  Protocol inProtocol = r.inProtocol->GetProtocol();
  Protocol outProtocol = r.outProtocol->GetProtocol();

  r.inIP->setEnabled(r.enable->isChecked() && inProtocol != Protocol::kMIDI && inProtocol != Protocol::kOTP);
  r.inIP->setToolTip(GetHelpText(Col::kInIP, inProtocol, outProtocol, /*script*/ false));
  r.inPort->setToolTip(GetHelpText(Col::kInPort, inProtocol, outProtocol, /*script*/ false));
  r.inPath->setToolTip(GetHelpText(Col::kInPath, inProtocol, outProtocol, /*script*/ false));
  r.inPath->setEnabled(r.enable->isChecked() && inProtocol != Protocol::ksACN && inProtocol != Protocol::kArtNet);

  if (inProtocol == Protocol::kPSN)
  {
    bool postLoad = (protocol == Protocol::kInvalid);
    if (!postLoad || r.inIP->text().isEmpty())
      r.inIP->setText(Router::GetDefaultPSNIP());
    if (!postLoad || r.inPort->text().isEmpty())
      r.inPort->setText(QString::number(Router::GetDefaultPSNPort()));
  }

  UpdateTcpIndicators();
}

void RoutingWidget::onOutProtocolChanged(size_t row, Protocol protocol)
{
  if (row >= m_Rows.size())
    return;

  const Row& r = m_Rows[row];
  Protocol inProtocol = r.inProtocol->GetProtocol();
  Protocol outProtocol = r.outProtocol->GetProtocol();

  r.outIP->setEnabled(r.enable->isChecked() && outProtocol != Protocol::kMIDI && outProtocol != Protocol::kOTP);
  r.outIP->setToolTip(GetHelpText(Col::kOutIP, inProtocol, outProtocol, /*script*/ false));
  r.outPort->setToolTip(GetHelpText(Col::kOutPort, inProtocol, outProtocol, /*script*/ false));
  r.outPath->setToolTip(GetHelpText(Col::kOutPath, inProtocol, outProtocol, /*script*/ false));
  r.outScriptText->setToolTip(GetHelpText(Col::kOutPath, inProtocol, outProtocol, /*script*/ true));

  if (outProtocol == Protocol::kPSN)
  {
    bool postLoad = (protocol == Protocol::kInvalid);
    if (!postLoad || r.outIP->text().isEmpty())
      r.outIP->setText(Router::GetDefaultPSNIP());
    if (!postLoad || r.outPort->text().isEmpty())
      r.outPort->setText(QString::number(Router::GetDefaultPSNPort()));
  }

  UpdateTcpIndicators();
}

void RoutingWidget::onHeaderHelpClicked(size_t id)
{
  if (!m_Help.dialog)
  {
    m_Help.dialog = new QWidget(this, Qt::Tool);
    QGridLayout* layout = new QGridLayout(m_Help.dialog);
    layout->setContentsMargins(QMargins());
    m_Help.edit = new QTextEdit(m_Help.dialog);
    m_Help.edit->setFont(UI::FixedFont());
    m_Help.edit->setWordWrapMode(QTextOption::NoWrap);
    QPalette pal = m_Help.edit->palette();
    pal.setColor(QPalette::Base, pal.color(QPalette::Window));
    m_Help.edit->setPalette(pal);
    m_Help.edit->setReadOnly(true);
    layout->addWidget(m_Help.edit);
  }

  m_Help.edit->setPlainText(GetHelpText(static_cast<Col>(id), Protocol::kInvalid, Protocol::kInvalid, /*script*/ true));
  m_Help.edit->document()->adjustSize();

  // adjust to document size
  QSize sz = m_Help.edit->document()->size().toSize() + QSize(20, 20);

  // center on main window
  QRect r(window()->geometry().center() - QPoint(sz.width() / 2, sz.height() / 2), sz);

  // keep within screen bounds
  QScreen* sc = screen();
  if (sc)
  {
    QRect sr = sc->availableGeometry();
    sr.adjust(100, 100, -100, -100);
    r.setSize(r.size().boundedTo(sr.size()));
    int overflow = sr.right() - r.right();
    if (overflow < 0)
      r.translate(overflow, 0);
    if (r.x() < sr.x())
      r.moveTo(sr.x(), r.y());
    overflow = sr.bottom() - r.bottom();
    if (overflow < 0)
      r.translate(0, overflow);
    if (r.y() < sr.y())
      r.moveTo(r.x(), sr.y());
  }

  m_Help.dialog->setGeometry(r);
  m_Help.dialog->show();
  m_Help.dialog->activateWindow();
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

QString RoutingWidget::GetHelpText(Col col, Protocol inProtocol, Protocol outProtocol, bool script)
{
  QString text;
  bool all = (inProtocol == Protocol::kInvalid && outProtocol == Protocol::kInvalid);

  switch (col)
  {
    case Col::kInIP:
    {
      if (inProtocol == Protocol::kMIDI)
      {
        text = tr("Incoming MIDI: IP is not used");
      }
      else if (inProtocol == Protocol::kOTP)
      {
        text = tr("Incoming OTP: IP is not used");
      }
      else
      {
        text =
            tr("Only route packets received from this specific IP address\n"
               "\n"
               "Leave blank to route packets received from any IP address\n"
               "\n"
               "Multicast Format: x.x.x.x,y.y.y.y\n"
               "(where x.x.x.x is the multicast group and y.y.y.y is the interface)");
      }
    }
    break;

    case Col::kInPort:
    {
      switch (inProtocol)
      {
        case Protocol::ksACN: text = tr("Route sACN levels received on this sACN universe (REQUIRED)"); break;
        case Protocol::kArtNet: text = tr("Route ArtNet levels received on this ArtNet universe (REQUIRED)"); break;
        case Protocol::kMIDI: text = tr("See available MIDI ports in Settings tab"); break;
        case Protocol::kOTP: text = tr("OTP System Number [%1-%2] (REQUIRED)").arg(otp::kMinSystemNumber).arg(otp::kMaxSystemNumber); break;
        default: text = tr("Route packets received on this port (REQUIRED)"); break;
      }
    }
    break;

    case Col::kInPath:
    {
      if (inProtocol != Protocol::ksACN && inProtocol != Protocol::kArtNet)
      {
        text =
            tr("Only route received OSC commands with this specific OSC command path\n"
               "(use * for wildcard matching, ex: /eos/out/event/*)\n\n"
               "Leave blank to route received packets with any OSC command path (or non-OSC packets)");
      }

      if (all || inProtocol == Protocol::ksACN)
      {
        if (!text.isEmpty())
          text += "\n\n";

        text +=
            tr("Incoming sACN:\n"
               "  Port is the sACN universe\n"
               "  Path is not used"
               "  Use %1 - %512 to reference the universe levels in Outgoing path");
      }

      if (all || inProtocol == Protocol::kArtNet)
      {
        if (!text.isEmpty())
          text += "\n\n";

        text +=
            tr("Incoming ArtNet:\n"
               "  Port is the ArtNet universe\n"
               "  Path is not used"
               "  Use %1 - %512 to reference the universe levels in Outgoing path");
      }

      if (all || inProtocol == Protocol::kPSN)
      {
        if (!text.isEmpty())
          text += "\n\n";

        text +=
            tr("Incoming PSN:\n"
               "  Individual:\n"
               "    /psn/<id>/pos=x,y,z\n"
               "    /psn/<id>/speed=x,y,z\n"
               "    /psn/<id>/orientation=x,y,z\n"
               "    /psn/<id>/acceleration=x,y,z\n"
               "    /psn/<id>/target=x,y,z\n"
               "    /psn/<id>/status=status\n"
               "    /psn/<id>/timestamp=timestamp\n"
               "  Unified:\n"
               "    /psn/<id>/pos/speed/orientation/acceleration/...");
      }

      if (all || inProtocol == Protocol::kMIDI)
      {
        if (!text.isEmpty())
          text += "\n\n";

        text +=
            tr("Incoming MIDI:\n"
               "  Raw:\n"
               "    /midi=a,b,c...\n"
               "  MIDI Show Control:\n");

        for (int i = 0; i < static_cast<int>(MSCCmd::kCount); ++i)
          text += QStringLiteral("    /msc/<device ID>/<command format>/%1\n").arg(MSCCmdName(static_cast<MSCCmd>(i)));
      }

      if (all || inProtocol == Protocol::kOTP)
      {
        if (!text.isEmpty())
          text += "\n\n";

        text += tr("Incoming OTP:\n"
                   "  Port is the OTP System Number [%1-%2]\n\n"
                   "  /otp/<group>/<point>/<priority>/pos=x,y,z\n"
                   "  /otp/<group>/<point>/<priority>/posVelAccel=velX,velY,velZ,accelX,accelY,accelZ\n"
                   "  /otp/<group>/<point>/<priority>/rot=x,y,z\n"
                   "  /otp/<group>/<point>/<priority>/rotVelAccel=velX,velY,velZ,accelX,accelY,accelZ\n"
                   "  /otp/<group>/<point>/<priority>/scale=x,y,z\n"
                   "  /otp/<group>/<point>/<priority>/frame=x,y,z\n")
                    .arg(otp::kMinSystemNumber)
                    .arg(otp::kMaxSystemNumber);
      }

      if (!all)
      {
        text +=
            tr("\n\n"
               "Click the [?] button for advanced examples & options");
      }
    }
    break;

    case Col::kOutIP:
    {
      if (outProtocol == Protocol::kMIDI)
      {
        text = tr("Outgoing MIDI: IP is not used");
      }
      else if (outProtocol == Protocol::kOTP)
      {
        text = tr("Outgoing OTP: IP is not used");
      }
      else
      {
        text =
            tr("Route received packets to this IP address\n"
               "\n"
               "Leave blank to route packets to the same IP address they were sent from\n"
               "\n"
               "Multicast Format: x.x.x.x,y.y.y.y\n"
               "(where x.x.x.x is the multicast group and y.y.y.y is the interface)");
      }
    }
    break;

    case Col::kOutPort:
    {
      switch (outProtocol)
      {
        case Protocol::ksACN:
        {
          text =
              tr("Route recevied packets to this outgoing sACN universe\n"
                 "\n"
                 "Leave blank to route packets to the same universe/port they were received on");
        }
        break;

        case Protocol::kArtNet:
        {
          text =
              tr("Route recevied packets to this outgoing ArtNet universe\n"
                 "\n"
                 "Leave blank to route packets to the same universe/port they were received on");
        }
        break;

        case Protocol::kMIDI:
        {
          text = tr("See available MIDI ports in Settings tab");
        }
        break;

        case Protocol::kOTP:
        {
          text = tr("OTP System Number [%1-%2]").arg(otp::kMinSystemNumber).arg(otp::kMaxSystemNumber);
        }
        break;

        default:
        {
          text =
              tr("Route received packets to this port\n"
                 "\n"
                 "Leave blank to route packets to the same port they were received on");
        }
        break;
      }
    }
    break;

    case Col::kOutPath:
    {
      switch (outProtocol)
      {
        case Protocol::ksACN: text = tr("Route received packets to this outgoing sACN universe"); break;
        case Protocol::kArtNet: text = tr("Route received packets to this outgoing ArtNet universe"); break;
        default: text = tr("Route received packets to this OSC command"); break;
      }

      text +=
          tr("\n\n"
             "Use %1, %2, %3, etc... to insert specific sections from the received OSC command"
             "\n\n"
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
             "Path:   /eos/%4/start\n"
             "Output: /cue/25/start");

      if (all || inProtocol == Protocol::ksACN)
      {
        text +=
            tr("\n\n"
               "Incoming sACN:\n"
               "  Use %1 - %512 to reference the universe levels");
      }

      if (all || inProtocol == Protocol::kArtNet)
      {
        text +=
            tr("\n\n"
               "Incoming ArtNet:\n"
               "  Use %1 - %512 to reference the universe levels");
      }

      if (all || outProtocol == Protocol::ksACN)
      {
        text +=
            tr("\n\n"
               "Outgoing sACN:\n"
               "  /sacn=1,2,3,...\n"
               "  /sacn/offset/<number>=1,2,3,...\n"
               "  /sacn/priority/<number>=1,2,3,...\n"
               "  /sacn/perChannelPriority/<number>=1,2,3,...\n"
               "\n"
               "Ex: sACN to OSC\n"
               "Input:  <sACN universe levels: %1 - %512>\n"
               "Path:   /eos/chan/1/param/red/green/blue=%10,%11,%12\n"
               "Output: /eos/chan/1/param/red/green/blue, 255(f), 0(f), 127(f)\n"
               "\n"
               "Ex: OSC to sACN\n"
               "Input:  /rgb/255/0/127\n"
               "Path:   /sacn/offset/10=%2,%3,%4\n"
               "Output: sACN output universe: 10=255, 11=0, 12=127");
      }

      if (all || outProtocol == Protocol::kArtNet)
      {
        text +=
            tr("\n\n"
               "Outgoing ArtNet:\n"
               "  /artnet=1,2,3,...\n"
               "  /artnet/offset/<number>=1,2,3,...\n"
               "\n"
               "Ex: ArtNet to OSC\n"
               "Input:  <ArtNet universe levels: %1 - %512>\n"
               "Path:   /eos/chan/1/param/red/green/blue=%10,%11,%12\n"
               "Output: /eos/chan/1/param/red/green/blue, 255(f), 0(f), 127(f)\n"
               "\n"
               "Ex: OSC to ArtNet\n"
               "Input:  /rgb/255/0/127\n"
               "Path:   /artnet/offset/10=%2,%3,%4\n"
               "Output: ArtNet output universe: 10=255, 11=0, 12=127");
      }

      if (all || inProtocol == Protocol::kPSN || outProtocol == Protocol::kPSN)
      {
        text +=
            tr("\n\n"
               "Incoming/Outgoing PSN:\n"
               "  Individual:\n"
               "    /psn/<id>/pos=x,y,z\n"
               "    /psn/<id>/speed=x,y,z\n"
               "    /psn/<id>/orientation=x,y,z\n"
               "    /psn/<id>/acceleration=x,y,z\n"
               "    /psn/<id>/target=x,y,z\n"
               "    /psn/<id>/status=status\n"
               "    /psn/<id>/timestamp=timestamp\n"
               "  Unified:\n"
               "    /psn/<id>/pos/speed/orientation/acceleration/...\n"
               "\n"
               "Ex: PSN to OSC\n"
               "Input:  /psn/1/pos, 10(f), 20(f), 30(f)\n"
               "Path:   /eos/chan/%2/param/x_focus/y_focus/z_focus=%4,%5,%6\n"
               "Output: /eos/chan/1/param/x_focus/y_focus/z_focus, 10(f), 20(f), 30(f)\n"
               "\n"
               "Ex: OSC to PSN\n"
               "Input:  /hoist/xyz, 10(f), 20(f), 30(f)\n"
               "Path:   /psn/1/pos=%3,%4,%5\n"
               "Output: PSN packet: tracker id 1, pos(10, 20, 30)");
      }

      if (all || inProtocol == Protocol::kMIDI || outProtocol == Protocol::kMIDI)
      {
        QString mscCommands;
        for (int i = 0; i < static_cast<int>(MSCCmd::kCount); ++i)
        {
          if (i > 0)
            mscCommands += QLatin1String(", ");
          mscCommands += MSCCmdName(static_cast<MSCCmd>(i));
        }

        text +=
            tr("\n\n"
               "Incoming/Outgoing MIDI:\n"
               "  Raw:\n"
               "    /midi=a,b,c...\n"
               "  MIDI Show Control:\n");

        for (int i = 0; i < static_cast<int>(MSCCmd::kCount); ++i)
          text += QStringLiteral("    /msc/<device ID>/<command format>/%1\n").arg(MSCCmdName(static_cast<MSCCmd>(i)));

        text +=
            tr("\n"
               "Ex: MIDI to OSC\n"
               "Input:  /midi, 255(i), 0(i), 127(i)\n"
               "Path:   /eos/chan/1/param/red/green/blue=%2,%3,%4\n"
               "Output: /eos/chan/1/param/red/green/blue, 255(f), 0(f), 127(f)\n"
               "\n"
               "Ex: OSC to MIDI\n"
               "Input:  /rgb, 255(f), 0(f), 127(f)\n"
               "Path:   /midi=%2,%3,%4\n"
               "Output: MIDI packet: FF 00 7F\n"
               "\n"
               "Ex: MSC to OSC\n"
               "Input:  /msc/2/1/go, 3(i), 4(i)\n"
               "Path:   /eos/cue/%6/%5/fire=\n"
               "Output: /eos/cue/4/3/fire\n"
               "\n"
               "Ex: OSC to MSC\n"
               "Input:  /eos/out/event/cue/1/3/fire\n"
               "Path:   /msc/2/1/go=%6,%5\n"
               "Output: MIDI packet: F0 7F 04 02 05 01 33 00 31 F7\n");
      }

      if (all || inProtocol == Protocol::kOTP || outProtocol == Protocol::kOTP)
      {
        text += tr("\n\n"
                   "Incoming/Outgoing OTP:\n"
                   "  Port is the OTP System Number [%1-%2]\n\n"
                   "  /otp/<group>/<point>/<priority>/pos=x,y,z\n"
                   "  /otp/<group>/<point>/<priority>/posVelAccel=velX,velY,velZ,accelX,accelY,accelZ\n"
                   "  /otp/<group>/<point>/<priority>/rot=x,y,z\n"
                   "  /otp/<group>/<point>/<priority>/rotVelAccel=velX,velY,velZ,accelX,accelY,accelZ\n"
                   "  /otp/<group>/<point>/<priority>/scale=x,y,z\n"
                   "  /otp/<group>/<point>/<priority>/frame=x,y,z\n")
                    .arg(otp::kMinSystemNumber)
                    .arg(otp::kMaxSystemNumber);

        text +=
            tr("\n"
               "Ex: OTP to OSC\n"
               "Input:  /otp/5/6/100/pos, 7(i), 8(i), 9(i)\n"
               "Path:   /eos/chan/%3/param/red/green/blue=%6,%7,%8\n"
               "Output: /eos/chan/6/param/red/green/blue, 7(i), 8(i), 9(i)\n"
               "\n"
               "Ex: OSC to OTP\n"
               "Input:  /hoist/xyz, 10(f), 20(f), 30(f)\n"
               "Path:   /otp/5/6/100/pos=%3,%4,%5\n"
               "Output: OTP packet: 5/6, priority(100), pos(10, 20, 30)");
      }

      if (script)
      {
        text +=
            tr("\n\n"
               "JavaScript Variables:\n"
               "--------------------\n"
               "OSC = outgoing osc path (string)\n"
               "ARGS = array of osc arguments\n"
               "NAME = name of route\n"
               "LOGS = array of log messages you may output\n"
               "\n"
               "Write your own JavaScript to modify the OSC and ARGS variables\n"
               "Set OSC to an empty string to skip sending\n"
               "\n"
               "Ex:\n"
               "// modify outgoing osc fader from percent to 8-bit value:\n"
               "OSC = OSC + \"/level\";\n"
               "ARGS[0] = Math.round(ARGS[0] * 255);\n"
               "\n"
               "Incoming sACN & Artnet:\n"
               "ARGS is an array of the incoming universe levels\n"
               "\n"
               "Ex:\n"
               "// convert sACN or ArtNet universe level 100 to OSC:\n"
               "OSC = \"/level\"\n"
               "if (ARGS.length > 100) {\n"
               "  ARGS[0] = ARGS[100];\n"
               "  ARGS.length = 1;\n"
               "} else {\n"
               "  OSC = \"\";\n"
               "}");
      }

      if (!all)
      {
        text +=
            tr("\n\n"
               "Click the [?] button for advanced examples & options");
      }
    }
  }

  return text;
}

////////////////////////////////////////////////////////////////////////////////

MainWindow::MainWindow(EosPlatform* platform, QWidget* parent /*=0*/, Qt::WindowFlags f /*=Qt::WindowFlags()*/)
  : QWidget(parent, f)
  , m_Settings("ETC", QLatin1String(VER_PRODUCTNAME_STR))
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
                    "QSplitter::handle {image: none;}"
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

  QMenu* help = menu->addMenu(tr("&Help"));
  help->addAction(tr("&View Help"), this, &MainWindow::onViewHelp);
  help->addAction(tr("&About"), this, &MainWindow::onAboutHelp);

  QSplitter* splitter = new QSplitter(Qt::Vertical, this);
  layout->addWidget(splitter, 1, 0);

  QWidget* routingBase = new QWidget(splitter);
  splitter->addWidget(routingBase);
  QVBoxLayout* routingLayout = new QVBoxLayout(routingBase);
  routingLayout->setContentsMargins(4, 0, 4, 0);

  QTabWidget* tabs = new QTabWidget(routingBase);
  routingLayout->addWidget(tabs);

  m_RoutingWidget = new RoutingWidget(tabs);
  connect(m_RoutingWidget, &RoutingWidget::muteToggled, this, &MainWindow::onMuteToggled);
  connect(m_RoutingWidget, &RoutingWidget::muteRouteToggled, this, &MainWindow::onMuteRouteToggled);
  tabs->addTab(m_RoutingWidget, tr("Routes"));

  m_TcpWidget = new TcpWidget(tabs);
  tabs->addTab(m_TcpWidget, tr("TCP"));

  m_SettingsWidget = new SettingsWidget(m_Settings, tabs);
  m_RoutingWidget->SetGlobals(m_SettingsWidget->GetScript());
  tabs->addTab(m_SettingsWidget, tr("Settings"));

  QHBoxLayout* buttonLayout = new QHBoxLayout();
  buttonLayout->setAlignment(Qt::AlignRight);
  routingLayout->addLayout(buttonLayout);

  m_StartButton = new QPushButton(tr("Apply && Start"), routingBase);
  QFont fnt = m_StartButton->font();
  fnt.setPointSize(fnt.pointSize() + 2);
  m_StartButton->setFont(fnt);
  QPalette pal = m_StartButton->palette();
  QColor buttonColor = QColor(8, 91, 44);
  pal.setColor(QPalette::Active, QPalette::Button, buttonColor);
  pal.setColor(QPalette::Inactive, QPalette::Button, buttonColor);
  pal.setColor(QPalette::Active, QPalette::ButtonText, Qt::white);
  m_StartButton->setPalette(pal);
  connect(m_StartButton, &QPushButton::clicked, this, &MainWindow::onStartClicked);
  buttonLayout->addWidget(m_StartButton);

  m_StopButton = new QPushButton(tr("Stop"), routingBase);
  m_StopButton->setFont(fnt);
  buttonColor = QColor(89, 18, 30);
  pal.setColor(QPalette::Active, QPalette::Button, buttonColor);
  pal.setColor(QPalette::Inactive, QPalette::Button, buttonColor);
  m_StopButton->setPalette(pal);
  m_StopButton->setEnabled(false);
  connect(m_StopButton, &QPushButton::clicked, this, &MainWindow::onStopClicked);
  buttonLayout->addWidget(m_StopButton);

  QWidget* logBase = new QWidget(splitter);
  QGridLayout* logLayout = new QGridLayout(logBase);
  logLayout->setContentsMargins(4, 0, 4, 0);
  splitter->addWidget(logBase);

  m_LogWidget = new LogWidget(logDepth, logBase);
  pal = m_LogWidget->palette();
  pal.setColor(QPalette::Window, BG_COLOR.darker(145));
  m_LogWidget->setPalette(pal);
  logLayout->addWidget(m_LogWidget, 0, 0);

  log->addAction(tr("&Clear"), m_LogWidget, &LogWidget::clear);
  log->addAction(tr("&Open"), this, &MainWindow::onOpenLog);

  QString version = QLatin1String(VER_PRODUCTNAME_STR) + QLatin1Char(' ') + QLatin1String(VER_PRODUCTVERSION_STR);
  m_Log.AddInfo(version.toUtf8().constData());

  m_RoutingWidget->LoadRoutes(Router::ROUTES(), ItemStateTable());
  m_TcpWidget->LoadConnections(Router::CONNECTIONS());
  m_RoutingWidget->SetTcpConnections(Router::CONNECTIONS());
  m_SettingsWidget->LoadSettings(Router::Settings());

  RestoreLastFile();
  UpdateWindowTitle();

  pal = palette();
  pal.setColor(QPalette::Window, BG_COLOR.darker(150));
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
  m_StartButton->setEnabled(true);
  m_StopButton->setEnabled(false);

  if (m_RouterThread)
  {
    m_RouterThread->Stop();
    SyncRouterThread(/*logsOnly*/ true);
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

  Router::ROUTES routes;
  m_RoutingWidget->SaveRoutes(routes, m_ItemStateTable);

  Router::CONNECTIONS connections;
  m_TcpWidget->SaveConnections(connections, &m_ItemStateTable);

  Router::Settings settings;
  m_SettingsWidget->SaveSettings(settings);

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

    m_RouterThread = new RouterThread(routes, connections, settings, m_ItemStateTable, m_ReconnectDelay);
    m_RouterThread->start();
    m_StartButton->setEnabled(false);
    m_StopButton->setEnabled(true);
    return true;
  }

  return false;
}

void MainWindow::GetPersistentSavePath(QString& path) const
{
  path = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).absoluteFilePath("save.osc.txt");
}

void MainWindow::UpdateWindowTitle()
{
  QString title = QStringLiteral("%1 %2.%3.%4").arg(VER_PRODUCTNAME_STR).arg(OSCROUTER_VERSION_MAJOR).arg(OSCROUTER_VERSION_MINOR).arg(OSCROUTER_VERSION_PATCH);
  if (!m_FilePath.isEmpty())
  {
    title.append(" - ");
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
  return Load(path);
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

  Shutdown();

  m_SettingsWidget->Load(lines);
  m_RoutingWidget->Load(lines);
  m_TcpWidget->Load(lines);
  Router::CONNECTIONS connections;
  m_TcpWidget->SaveConnections(connections, /*itemStateTable*/ nullptr);
  m_RoutingWidget->SetTcpConnections(connections);

  SaveToBuffer(m_FileContents);
  SetUnsaved(false);

  return true;
}

bool MainWindow::SaveFile(const QString& path)
{
  m_FilePath = path;
  m_Settings.setValue(SETTING_LAST_FILE, m_FilePath);

  if (SaveToFile(path))
  {
    SaveToBuffer(m_FileContents);
    SetUnsaved(false);
    UpdateWindowTitle();
    return true;
  }
  else
    QMessageBox::critical(this, QLatin1String(VER_PRODUCTNAME_STR), tr("Unable to save file \"%1\"").arg(path));

  return false;
}

bool MainWindow::SaveToDevice(QIODevice& device)
{
  QTextStream stream(&device);
  stream.setEncoding(QStringConverter::Utf8);
  if (!device.open(QFile::WriteOnly | QFile::Truncate))
    return false;

  m_SettingsWidget->Save(stream);
  m_RoutingWidget->Save(stream);
  m_TcpWidget->Save(stream);
  return true;
}

bool MainWindow::SaveToBuffer(QByteArray& buffer)
{
  QBuffer device(&buffer);
  return SaveToDevice(device);
}

bool MainWindow::SaveToFile(const QString& path)
{
  QDir().mkpath(QFileInfo(path).absolutePath());

  QFile file(path);
  return SaveToDevice(file);
}

void MainWindow::RestoreLastFile()
{
  bool loaded = false;

  QString path = m_Settings.value(SETTING_LAST_FILE).toString();
  if (!path.isEmpty())
  {
    if (LoadFile(path))
      loaded = true;
  }

  if (!loaded)
  {
    // fall-back to loading persistent file
    GetPersistentSavePath(path);
    if (Load(path))
    {
      m_FilePath = m_Settings.value(SETTING_LAST_FILE).toString();
      loaded = true;
    }
  }

  if (!loaded)
    SaveToBuffer(m_FileContents);
  else if (m_StartButton->isEnabled() && m_Settings.value(SETTING_AUTO_START).toBool())
    onStartClicked(false);
}

void MainWindow::SyncRouterThread(bool logsOnly)
{
  if (m_RouterThread)
  {
    m_RouterThread->Sync(m_TempLogQ, m_ItemStateTable);
    m_Log.AddQ(m_TempLogQ);
  }

  m_Log.Flush(m_TempLogQ);
  FlushLogQ(m_TempLogQ);
  m_TempLogQ.clear();

  if (!logsOnly)
  {
    Router::CONNECTIONS connections;
    m_TcpWidget->SaveConnections(connections, /*itemStateTable*/ nullptr);
    m_RoutingWidget->SetTcpConnections(connections);

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
  SyncRouterThread(/*logsOnly*/ false);
}

void MainWindow::buildRoutes()
{
  BuildRoutes();
}

void MainWindow::onNewFile()
{
  if (!ResolveUnsaved())
    return;

  Shutdown();

  m_RoutingWidget->LoadRoutes(Router::ROUTES(), ItemStateTable());
  m_TcpWidget->LoadConnections(Router::CONNECTIONS());
  m_RoutingWidget->SetTcpConnections(Router::CONNECTIONS());
  m_SettingsWidget->LoadSettings(Router::Settings());
  m_FilePath.clear();
  m_Settings.setValue(SETTING_LAST_FILE, m_FilePath);
  QString path;
  GetPersistentSavePath(path);
  QFile::setPermissions(path, QFile::WriteOwner);
  QFile::remove(path);
  SaveToBuffer(m_FileContents);
  SetUnsaved(false);
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
  QString path = QFileDialog::getOpenFileName(this, tr("Open"), dir, QLatin1String(VER_PRODUCTNAME_STR) + QLatin1Char(' ') + tr("File (*.txt *.osc.txt)"));
  if (!path.isEmpty())
  {
    if (!LoadFile(path))
      QMessageBox::critical(this, QLatin1String(VER_PRODUCTNAME_STR), tr("Unable to open file \"%1\"").arg(path));
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
  QString path = QFileDialog::getSaveFileName(this, tr("Save"), dir, QLatin1String(VER_PRODUCTNAME_STR) + QLatin1Char(' ') + tr("File (*.osc.txt)"));
  if (!path.isEmpty())
    SaveFile(path);
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

void MainWindow::onViewHelp()
{
  if (!m_Help)
  {
    const QSize kPadding(20, 20);

    m_Help = new QWidget(this, Qt::Tool);
    m_Help->setContentsMargins(QMargins());

    QGridLayout* windowLayout = new QGridLayout(m_Help);
    windowLayout->setSpacing(0);
    windowLayout->setContentsMargins(QMargins());
    QScrollArea* scroll = new QScrollArea(m_Help);
    windowLayout->addWidget(scroll);

    QWidget* base = new QWidget(scroll);
    scroll->setWidget(base);

    QLabel* incomingLabel = new QLabel(tr("Incoming"), base);
    QFont fnt = incomingLabel->font();
    fnt.setPointSize(12);
    incomingLabel->setFont(fnt);
    incomingLabel->setAlignment(Qt::AlignCenter);

    QTextEdit* incomingEdit = new QTextEdit(base);
    incomingEdit->setFont(UI::FixedFont());
    incomingEdit->setWordWrapMode(QTextOption::NoWrap);
    QPalette pal = incomingEdit->palette();
    pal.setColor(QPalette::Base, DARK_BG_COLOR);
    incomingEdit->setPalette(pal);
    incomingEdit->setReadOnly(true);
    incomingEdit->setText(RoutingWidget::GetHelpText(/*incoming*/ true));
    incomingEdit->document()->adjustSize();
    incomingEdit->resize(incomingEdit->document()->size().toSize() + kPadding);

    QLabel* outgoingLabel = new QLabel(tr("Outgoing"), base);
    outgoingLabel->setFont(incomingLabel->font());
    outgoingLabel->setAlignment(Qt::AlignCenter);

    QTextEdit* outgoingEdit = new QTextEdit(base);
    outgoingEdit->setFont(incomingEdit->font());
    outgoingEdit->setWordWrapMode(QTextOption::NoWrap);
    outgoingEdit->setPalette(pal);
    outgoingEdit->setReadOnly(true);
    outgoingEdit->setText(RoutingWidget::GetHelpText(/*incoming*/ false));
    outgoingEdit->document()->adjustSize();
    outgoingEdit->resize(outgoingEdit->document()->size().toSize() + kPadding);

    const int kSpacing = 6;
    incomingLabel->setGeometry(kSpacing, kSpacing, incomingEdit->width(), incomingLabel->sizeHint().height());
    incomingEdit->move(kSpacing, incomingLabel->geometry().bottom() + kSpacing + 1);
    outgoingLabel->setGeometry(incomingLabel->geometry().right() + kSpacing + 1, kSpacing, outgoingEdit->width(), incomingLabel->height());
    outgoingEdit->move(outgoingLabel->x(), incomingEdit->y());

    base->resize(outgoingLabel->geometry().right() + kSpacing + 1, std::max(incomingEdit->geometry().bottom(), outgoingEdit->geometry().bottom()) + kSpacing + 1);

    m_Help->resize(base->geometry().right() + kPadding.width(), m_Help->sizeHint().height());
    m_Help->setMaximumSize(base->geometry().right() + kPadding.width(), base->geometry().bottom() + kPadding.height());
  }

  m_Help->show();
}

void MainWindow::onAboutHelp()
{
  if (!m_About)
  {
    m_About = new QWidget(this, Qt::Tool);

    QGridLayout* grid = new QGridLayout(m_About);
    int row = 0;

    QLabel* icon = new QLabel(this);
    grid->addWidget(icon, row, 0, Qt::AlignCenter);
    icon->setPixmap(QPixmap(QStringLiteral(":/qt/etc/images/Icon.png")).scaledToWidth(200, Qt::SmoothTransformation));
    ++row;

    grid->addWidget(new QLabel(QLatin1String(VER_PRODUCTNAME_STR) + QLatin1Char(' ') + QLatin1String(VER_PRODUCTVERSION_STR), this), row, 0, Qt::AlignCenter);
    ++row;

    QTextBrowser* browser = new QTextBrowser(this);
    grid->addWidget(browser, row, 0);
    browser->setOpenExternalLinks(true);
    browser->setWordWrapMode(QTextOption::NoWrap);
    QPalette pal = browser->palette();
    pal.setColor(QPalette::Base, palette().color(QPalette::Window));
    browser->setPalette(pal);
    browser->setText(QStringLiteral("<h2>Third Party Software</h2>"
                                    "<b>psn-cpp</b>"
                                    "<br>"
                                    "Website: <a href=\"%1\">%1</a>"
                                    "<br>"
                                    "License: The MIT License (MIT)"
                                    "<br>"
                                    "Copyright (c) 2014 VYV Corporation"
                                    "<br>"
                                    "<br>"
                                    "<b>libartnet</b>"
                                    "<br>"
                                    "Website: <a href=\"%2\">%2</a>"
                                    "<br>"
                                    "License: LGPL-2.1 license"
                                    "<br>"
                                    "Copyright (C) 2004-2007 Simon Newton")
                         .arg(QLatin1String("https://github.com/vyv/psn-cpp"))
                         .arg(QLatin1String("https://github.com/OpenLightingProject/libartnet")));
  }

  m_About->show();
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
  SaveToFile(path);
  QApplication::exit(0);
}

bool MainWindow::ResolveUnsaved()
{
  if (m_Unsaved)
  {
    QMessageBox mb(QMessageBox::Question, QLatin1String(VER_PRODUCTNAME_STR), tr("Do you want to save changes?"), QMessageBox::NoButton, this);
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

void MainWindow::SetUnsaved(bool unsaved)
{
  if (m_Unsaved == unsaved)
    return;

  m_Unsaved = unsaved;
  UpdateWindowTitle();
}

void MainWindow::UpdateUnsaved()
{
  QByteArray newFileContents;
  SetUnsaved(SaveToBuffer(newFileContents) && newFileContents != m_FileContents);
}

void MainWindow::onStartClicked(bool /*checked*/)
{
  Router::ROUTES routes;
  ItemStateTable itemStateTable;
  m_RoutingWidget->SaveRoutes(routes, itemStateTable);
  m_RoutingWidget->LoadRoutes(routes, itemStateTable);

  Router::CONNECTIONS connections;
  m_TcpWidget->SaveConnections(connections, /*itemStateTable*/ 0);
  m_TcpWidget->LoadConnections(connections);
  m_RoutingWidget->SetTcpConnections(connections);

  Router::Settings settings;
  m_SettingsWidget->SaveSettings(settings);
  m_SettingsWidget->LoadSettings(settings);

  UpdateUnsaved();

  QTimer::singleShot(1, this, &MainWindow::buildRoutes);
}

void MainWindow::onStopClicked(bool /*checked*/)
{
  Shutdown();
}

void MainWindow::onMuteToggled(bool incoming, bool checked)
{
  if (incoming)
    m_ItemStateTable.SetMuteAllIncoming(checked);
  else
    m_ItemStateTable.SetMuteAllOutgoing(checked);

  UpdateUnsaved();
}

void MainWindow::onMuteRouteToggled(size_t id, bool checked)
{
  m_ItemStateTable.Mute(id, checked);
  UpdateUnsaved();
}

////////////////////////////////////////////////////////////////////////////////
