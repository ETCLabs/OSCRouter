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

#include "otp/example.h"

OTPExample::OTPExample(QObject* parent /*= nullptr*/)
  : QObject(parent)
{
  QString iface("192.168.50.243");

  otp::QConsumer* consumer = new otp::QConsumer(this);
  connect(consumer, &otp::QConsumer::producerAppeared, this, &OTPExample::onProducerAppeared);
  connect(consumer, &otp::QConsumer::pointChanged, this, &OTPExample::onPointChanged);
  connect(consumer, &otp::QConsumer::log, this, &OTPExample::onLog);
  consumer->start("OTPExample-Consumer", {otp::ModuleType::kPos}, /*systems*/ {1, 4}, iface);

  producer_ = new otp::QProducer(this);
  connect(producer_, &otp::QProducer::consumerAppeared, this, &OTPExample::onConsumerAppeared);
  connect(producer_, &otp::QProducer::log, this, &OTPExample::onLog);
  producer_->start("OTPExample-Producer", iface);

  QTimer* timer = new QTimer(this);
  connect(timer, &QTimer::timeout, this, &OTPExample::movePoints);
  timer->start(50);
}

void OTPExample::movePoints()
{
  t_ += 0.01f;
  float s = sinf(t_);
  float c = cosf(t_);

  otp::Point point;
  point.name = "A";
  point.modules.pos = otp::Vector3(qRound(s * 100), qRound(s * 1000), qRound(s * 10000));
  producer_->obj()->SetPoint(otp::Frame(1, 2, 3), point);

  point.name = "B";
  point.modules.pos = otp::Vector3(qRound(c * 100), qRound(c * 1000), qRound(c * 10000));
  producer_->obj()->SetPoint(otp::Frame(4, 5, 6), point);
}

void OTPExample::onConsumerAppeared(const QUuid& cid, const otp::Endpoint& consumer)
{
  qInfo() << "consumer appeared" << cid << consumer.toString();
}

void OTPExample::onPointChanged(const otp::Frame& frame, const otp::Point& point)
{
  qInfo() << frame.toString() << point.toString();
}

void OTPExample::onProducerAppeared(const QUuid& cid, const otp::Endpoint& producer)
{
  qInfo() << "producer appeared" << cid << producer.toString();
}

void OTPExample::onLog(QtMsgType type, QString text)
{
  QDebug(type) << text;
}
