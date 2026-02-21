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

#include "otp/qotp.h"

namespace otp
{
QConsumer::QConsumer(QObject* parent /*= nullptr*/)
  : QObject(parent)
{
  ticker_.setSingleShot(true);
  connect(&ticker_, &QTimer::timeout, this, &QConsumer::tick);
}

Consumer& QConsumer::start(const QString& name, const ModuleTypeList& module_types, const SystemSet& systems, const otp::NetInterfaces& nets /*= otp::NetInterfaces()*/,
                           const QUuid& cid /*= QUuid::createUuid()*/)
{
  stop();

  consumer_ = new Consumer(name, module_types, cid);
  consumer_->SetCallbacks(this);
  consumer_->Init(nets);
  consumer_->SetSystems(systems);
  tick();
  return *consumer_;
}

void QConsumer::stop()
{
  ticker_.stop();
  delete consumer_;
  consumer_ = nullptr;
}

void QConsumer::tick()
{
  if (!consumer_)
    return;

  consumer_->Tick();
  ticker_.start(10);
}

void QConsumer::ConsumerCallback_ProducerChanged(EndpointChange change, const QUuid& cid, const Endpoint& producer)
{
  switch (change)
  {
    case EndpointChange::kAppeared: emit producerAppeared(cid, producer); break;
    case EndpointChange::kExpired: emit producerExpired(cid, producer); break;
    case EndpointChange::kUpdated: emit producerChanged(cid, producer); break;
    default: break;
  }
}

void QConsumer::ConsumerCallback_PointChanged(PointChange change, const Frame& frame, const Point& point)
{
  switch (change)
  {
    case PointChange::kNone: emit pointUnchanged(frame, point); break;
    case PointChange::kSystemAppeared: emit systemAppeared(frame, point); break;
    case PointChange::kGroupAppeared: emit groupAppeared(frame, point); break;
    case PointChange::kPointAppeared: emit pointAppeared(frame, point); break;
    case PointChange::kPointUpdated: emit pointChanged(frame, point); break;
    default: break;
  }
}

void QConsumer::ConsumerCallback_Log(const LogMsg& msg)
{
  emit log(msg.type, msg.text);
}

QProducer::QProducer(QObject* parent /*= nullptr*/)
  : QObject(parent)
{
  ticker_.setSingleShot(true);
  connect(&ticker_, &QTimer::timeout, this, &QProducer::tick);
}

Producer& QProducer::start(const QString& name, const otp::NetInterfaces& nets /*= otp::NetInterfaces()*/, const QUuid& cid /*= QUuid::createUuid()*/)
{
  stop();

  producer_ = new Producer(name, cid);
  producer_->SetCallbacks(this);
  producer_->Init(nets);
  tick();
  return *producer_;
}

void QProducer::stop()
{
  ticker_.stop();
  delete producer_;
  producer_ = nullptr;
}

void QProducer::tick()
{
  if (!producer_)
    return;

  producer_->Tick();
  ticker_.start(10);
}

void QProducer::ProducerCallback_ConsumerChanged(EndpointChange change, const QUuid& cid, const Endpoint& producer)
{
  switch (change)
  {
    case EndpointChange::kAppeared: emit consumerAppeared(cid, producer); break;
    case EndpointChange::kExpired: emit consumerExpired(cid, producer); break;
    case EndpointChange::kUpdated: emit consumerChanged(cid, producer); break;
    default: break;
  }
}

void QProducer::ProducerCallback_Log(const LogMsg& msg)
{
  emit log(msg.type, msg.text);
}
}  // namespace otp
