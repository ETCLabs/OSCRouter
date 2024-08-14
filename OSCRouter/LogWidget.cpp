
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

#include "LogWidget.h"

// must be last include
#include "LeakWatcher.h"

////////////////////////////////////////////////////////////////////////////////

LogWidget::LogWidget(size_t maxLineCount, QWidget *parent)
  : QWidget(parent)
  , m_LineHeight(0)
  , m_LineWidth(0)
  , m_ForwardingWheelEvent(false)
  , m_AutoScroll(true)
{
  m_Lines.resize(maxLineCount + 1);

  QPalette pal(palette());
  pal.setColor(QPalette::Base, BG_COLOR);
  setPalette(pal);

  setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

  m_VScrollBar = new QScrollBar(Qt::Vertical, this);
  connect(m_VScrollBar, SIGNAL(valueChanged(int)), this, SLOT(onVScrollChanged(int)));

  m_HScrollBar = new QScrollBar(Qt::Horizontal, this);
  connect(m_HScrollBar, SIGNAL(valueChanged(int)), this, SLOT(onHScrollChanged(int)));

  UpdateFont();
}

////////////////////////////////////////////////////////////////////////////////

void LogWidget::Clear()
{
  size_t prevNumLines = GetNumLines();
  int prevLineWidth = m_LineWidth;

  m_Index = sRingBufferIndex();
  m_LineWidth = 0;

  if (GetNumLines() != prevNumLines)
    UpdateVScrollBar();

  if (m_LineWidth != prevLineWidth)
    UpdateHScrollBar();

  update();
}

////////////////////////////////////////////////////////////////////////////////

void LogWidget::Log(EosLog::LOG_Q &logQ)
{
  if (logQ.empty() || m_Lines.empty())
    return;

  size_t prevNumLines = GetNumLines();

  for (EosLog::LOG_Q::const_iterator i = logQ.begin(); i != logQ.end(); i++)
  {
    const EosLog::sLogMsg &msg = *i;

    sLine &line = m_Lines[m_Index.tail];

    if (msg.text.c_str())
    {
      line.text = QString::fromUtf8(msg.text.c_str());

      switch (msg.type)
      {
        case EosLog::LOG_MSG_TYPE_DEBUG: line.color = MUTED_COLOR; break;
        case EosLog::LOG_MSG_TYPE_WARNING: line.color = WARNING_COLOR; break;
        case EosLog::LOG_MSG_TYPE_ERROR: line.color = ERROR_COLOR; break;
        case EosLog::LOG_MSG_TYPE_RECV: line.color = RECV_COLOR; break;
        case EosLog::LOG_MSG_TYPE_SEND: line.color = SEND_COLOR; break;
        default: line.color = palette().color(QPalette::Text); break;
      }
    }
    else
      line.text.clear();

    if (++m_Index.tail >= m_Lines.size())
      m_Index.tail = 0;

    if (m_Index.tail == m_Index.head)
    {
      m_Index.head = (m_Index.tail + 1);
      if (m_Index.head >= m_Lines.size())
        m_Index.head = 0;
    }
  }

  if (GetNumLines() != prevNumLines)
    UpdateVScrollBar();

  update();
}

////////////////////////////////////////////////////////////////////////////////

size_t LogWidget::GetNumLines() const
{
  if (m_Index.tail > m_Index.head)
    return (m_Index.tail - m_Index.head);
  else if (m_Index.tail < m_Index.head)
    return (m_Lines.size() - 1);
  return 0;
}

////////////////////////////////////////////////////////////////////////////////

void LogWidget::GetContentsRect(QRect &r) const
{
  r = rect().adjusted(0, 0, -m_VScrollBar->width(), -m_HScrollBar->height());
}

////////////////////////////////////////////////////////////////////////////////

void LogWidget::UpdateFont()
{
  m_LineHeight = QFontMetrics(font()).height();
}

////////////////////////////////////////////////////////////////////////////////

void LogWidget::UpdateVScrollBar()
{
  bool wasAtBottom = (!m_VScrollBar->isEnabled() || m_VScrollBar->value() == m_VScrollBar->maximum());

  if (m_Lines.empty() || m_LineHeight < 1)
  {
    m_VScrollBar->setEnabled(false);
  }
  else
  {
    QRect r;
    GetContentsRect(r);
    size_t lineCount = GetNumLines();
    size_t linesPerPage = r.height() / m_LineHeight;
    if (linesPerPage >= lineCount)
    {
      m_VScrollBar->setEnabled(false);
    }
    else
    {
      size_t range = (lineCount - linesPerPage);
      m_VScrollBar->setMinimum(0);
      m_VScrollBar->setMaximum(static_cast<int>(range));
      m_VScrollBar->setEnabled(true);
    }
  }

  if (m_AutoScroll && wasAtBottom && m_VScrollBar->isEnabled())
    m_VScrollBar->setValue(m_VScrollBar->maximum());
}

////////////////////////////////////////////////////////////////////////////////

void LogWidget::UpdateHScrollBar()
{
  QRect r;
  GetContentsRect(r);
  if (m_LineWidth > r.width())
  {
    m_HScrollBar->setMinimum(0);
    m_HScrollBar->setMaximum(m_LineWidth - r.width());
    m_HScrollBar->setEnabled(true);
  }
  else
    m_HScrollBar->setEnabled(false);
}

////////////////////////////////////////////////////////////////////////////////

bool LogWidget::event(QEvent *event)
{
  switch (event->type())
  {
    case QEvent::FontChange: UpdateFont(); break;
  }

  return QWidget::event(event);
}

////////////////////////////////////////////////////////////////////////////////

void LogWidget::resizeEvent(QResizeEvent * /*event*/)
{
  QSize vsh(m_VScrollBar->sizeHint());
  QSize hsh(m_HScrollBar->sizeHint());
  m_VScrollBar->setGeometry(width() - vsh.width(), 0, vsh.width(), height() - hsh.height());
  m_HScrollBar->setGeometry(0, height() - hsh.height(), width() - vsh.width(), hsh.height());
  UpdateVScrollBar();
  UpdateHScrollBar();
}

////////////////////////////////////////////////////////////////////////////////

void LogWidget::paintEvent(QPaintEvent * /*event*/)
{
  QPainter painter(this);
  painter.fillRect(QRect(0, 0, width(), height()), palette().color(QPalette::Window));

  size_t lineCount = GetNumLines();

  if (lineCount == 0 || m_Lines.empty())
    return;

  int x = 0;
  int y = 0;
  int bottom = m_HScrollBar->y();
  int maxLineWidth = 0;
  QRect bounds;

  size_t index = m_Index.head;

  if (m_VScrollBar->isEnabled())
  {
    int scrollOffset = m_VScrollBar->value();
    if (scrollOffset >= 0)
    {
      size_t offset = static_cast<size_t>(scrollOffset);
      if (offset < lineCount)
        index = ((index + offset) % m_Lines.size());
    }
  }

  if (m_HScrollBar->isEnabled())
    x -= m_HScrollBar->value();

  QRect r;
  GetContentsRect(r);
  painter.setClipRect(r);

  while (index != m_Index.tail)
  {
    const sLine &line = m_Lines[index];
    QRect textRect(x, y, width() - x, m_LineHeight);
    if (y > bottom)
      break;

    painter.setPen(line.color);
    painter.drawText(textRect, Qt::AlignLeft, line.text, &bounds);
    y += m_LineHeight;

    if (bounds.width() > maxLineWidth)
      maxLineWidth = bounds.width();

    if (++index >= m_Lines.size())
      index = 0;
  }

  if (m_LineWidth < maxLineWidth)
  {
    m_LineWidth = maxLineWidth;
    UpdateHScrollBar();
  }
}

////////////////////////////////////////////////////////////////////////////////

void LogWidget::wheelEvent(QWheelEvent *event)
{
  if (!m_ForwardingWheelEvent && m_VScrollBar->isEnabled())
  {
    m_ForwardingWheelEvent = true;
    QApplication::sendEvent(m_VScrollBar, event);
    m_ForwardingWheelEvent = false;
  }

  event->accept();
}

////////////////////////////////////////////////////////////////////////////////

void LogWidget::onVScrollChanged(int /*value*/)
{
  update();
}

////////////////////////////////////////////////////////////////////////////////

void LogWidget::onHScrollChanged(int /*value*/)
{
  update();
}

////////////////////////////////////////////////////////////////////////////////
