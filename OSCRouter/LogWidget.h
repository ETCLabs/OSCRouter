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
#ifndef LOG_WIDGET_H
#define LOG_WIDGET_H

#ifndef QT_INCLUDE_H
#include "QtInclude.h"
#endif

#ifndef EOS_LOG_H
#include "EosLog.h"
#endif

////////////////////////////////////////////////////////////////////////////////

class LogWidget : public QWidget
{
  Q_OBJECT

public:
  LogWidget(size_t maxLineCount, QWidget *parent);

  virtual void Clear();
  virtual void Log(EosLog::LOG_Q &logQ);
  virtual QSize sizeHint() const { return QSize(400, 150); }

private slots:
  void onVScrollChanged(int value);
  void onHScrollChanged(int value);

protected:
  struct sLine
  {
    QString text;
    QColor color;
  };

  typedef std::vector<sLine> RING_BUFFER;

  struct sRingBufferIndex
  {
    sRingBufferIndex()
      : head(0)
      , tail(0)
    {
    }
    bool valid() const { return (head != tail); }
    bool invalid() const { return (head == tail); }
    size_t head;
    size_t tail;
  };

  RING_BUFFER m_Lines;
  sRingBufferIndex m_Index;
  int m_LineHeight;
  int m_LineWidth;
  QScrollBar *m_VScrollBar;
  QScrollBar *m_HScrollBar;
  bool m_ForwardingWheelEvent;
  bool m_AutoScroll;

  virtual size_t GetNumLines() const;
  virtual void GetContentsRect(QRect &r) const;
  virtual void UpdateFont();
  virtual void UpdateVScrollBar();
  virtual void UpdateHScrollBar();
  virtual bool event(QEvent *event);
  virtual void resizeEvent(QResizeEvent *event);
  virtual void paintEvent(QPaintEvent *event);
  virtual void wheelEvent(QWheelEvent *event);
};

////////////////////////////////////////////////////////////////////////////////

#endif
