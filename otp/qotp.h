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

#include "otp/otp.h"

namespace otp
{
class QConsumer : public QObject, private Consumer::Callbacks
{
  Q_OBJECT

public:
  QConsumer(QObject* parent = nullptr);

  Consumer& start(const QString& name, const ModuleTypeList& module_types, const SystemSet& systems, const otp::NetInterfaces& nets = otp::NetInterfaces(), const QUuid& cid = QUuid::createUuid());
  void stop();
  Consumer* obj() const { return consumer_; }

signals:
  void producerAppeared(const QUuid& cid, const Endpoint& producer);
  void producerExpired(const QUuid& cid, const Endpoint& producer);
  void producerChanged(const QUuid& cid, const Endpoint& producer);
  void systemAppeared(const Frame& frame, const Point& point);
  void groupAppeared(const Frame& frame, const Point& point);
  void pointAppeared(const Frame& frame, const Point& point);
  void pointChanged(const Frame& frame, const Point& point);
  void pointUnchanged(const Frame& frame, const Point& point);
  void log(QtMsgType type, QString text);

private slots:
  void tick();

private:
  Consumer* consumer_ = nullptr;
  QTimer ticker_;

  void ConsumerCallback_ProducerChanged(EndpointChange change, const QUuid& cid, const Endpoint& producer) override;
  void ConsumerCallback_PointChanged(PointChange change, const Frame& frame, const Point& point) override;
  void ConsumerCallback_Log(const LogMsg& msg) override;
};

class QProducer : public QObject, private Producer::Callbacks
{
  Q_OBJECT

public:
  QProducer(QObject* parent = nullptr);

  Producer& start(const QString& name, const otp::NetInterfaces& nets = otp::NetInterfaces(), const QUuid& cid = QUuid::createUuid());
  void stop();
  Producer* obj() const { return producer_; }

signals:
  void consumerAppeared(const QUuid& cid, const Endpoint& consumer);
  void consumerExpired(const QUuid& cid, const Endpoint& consumer);
  void consumerChanged(const QUuid& cid, const Endpoint& consumer);
  void log(QtMsgType type, QString text);

private slots:
  void tick();

private:
  Producer* producer_ = nullptr;
  QTimer ticker_;

  void ProducerCallback_ConsumerChanged(EndpointChange change, const QUuid& cid, const Endpoint& consumer) override;
  void ProducerCallback_Log(const LogMsg& msg) override;
};
}  // namespace otp
