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
#define QT_INCLUDE_H

////////////////////////////////////////////////////////////////////////////////

#define TEXT_COLOR QColor(119, 167, 255)
#define MUTED_COLOR QColor(100, 100, 100)
#define SUCCESS_COLOR QColor(16, 183, 87)
#define ERROR_COLOR QColor(164, 66, 66)
#define WARNING_COLOR QColor(172, 122, 57)
#define RECV_COLOR QColor(255, 187, 255)
#define SEND_COLOR QColor(0, 181, 149)
#define CONNECT_COLOR QColor(105, 92, 152)
#define ACTIVITY_COLOR QColor(200, 200, 200)
#define BG_COLOR QColor(40, 40, 40)

#ifdef WIN32
#include <Winsock2.h>
#endif

#include <QtCore/QtCore>
#include <QtGui/QtGui>
#include <QtWidgets/QtWidgets>
#include <QtNetwork/QtNetwork>
#include <QtQml/QJSEngine>

////////////////////////////////////////////////////////////////////////////////

#endif
