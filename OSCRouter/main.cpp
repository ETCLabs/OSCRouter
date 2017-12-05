// Copyright (c) 2015 Electronic Theatre Controls, Inc., http://www.etcconnect.com
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

#include <stdio.h>
#include <stdlib.h>
#include "EosTimer.h"
#include "QtInclude.h"
#include "MainWindow.h"

// must be last include
#include "LeakWatcher.h"

////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[])
{
#ifdef WIN32
	#ifdef _DEBUG
		int flags = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
		flags = (flags & 0x0000ffff) | _CRTDBG_LEAK_CHECK_DF;
		_CrtSetDbgFlag(flags);
	#endif
#endif

	EosTimer::Init();
	
#ifndef WIN32
	if(argc > 0)
	{
		QDir dir( argv[0] );
		dir.cdUp();
		dir.cdUp();
		dir.cd("Plugins");
		QCoreApplication::setLibraryPaths( QStringList(dir.canonicalPath()) );
	}
#endif
	
	QApplication app(argc, argv);

	app.setDesktopSettingsAware(false);
	app.setStyle( QStyleFactory::create("Fusion") );

	QPalette pal;
	pal.setColor(QPalette::Window, QColor(40,40,40));
	pal.setColor(QPalette::WindowText, TEXT_COLOR);
	pal.setColor(QPalette::Disabled, QPalette::WindowText, MUTED_COLOR);
	pal.setColor(QPalette::Base, QColor(60,60,60));
	pal.setColor(QPalette::Button, QColor(60,60,60));
	pal.setColor(QPalette::Light, pal.color(QPalette::Button).lighter(20));
	pal.setColor(QPalette::Midlight, pal.color(QPalette::Button).lighter(10));
	pal.setColor(QPalette::Dark, pal.color(QPalette::Button).darker(20));
	pal.setColor(QPalette::Mid, pal.color(QPalette::Button).darker(10));
	pal.setColor(QPalette::Text, TEXT_COLOR);
	pal.setColor(QPalette::Disabled, QPalette::Text, MUTED_COLOR);
	pal.setColor(QPalette::Highlight, QColor(80,80,80));
	pal.setColor(QPalette::HighlightedText, QColor(255,142,51));
	pal.setColor(QPalette::ButtonText, TEXT_COLOR);
	pal.setColor(QPalette::Disabled, QPalette::ButtonText, MUTED_COLOR);
	app.setPalette(pal);

	MainWindow *mainWindow = new MainWindow();
	mainWindow->show();
	int result = app.exec();
	delete mainWindow;

	return result;
}

////////////////////////////////////////////////////////////////////////////////
