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

#import "EosPlatform_Mac_Native.h"
#import "EosPlatform_Mac_Bridge.h"
#import <QtWidgets/QApplication>
#import <QtCore/QDir>
#import <QtCore/QUrl>


////////////////////////////////////////////////////////////////////////////////

@implementation EosPlatform_Mac_Native

////////////////////////////////////////////////////////////////////////////////

void* Bridge_CreatePlatform(std::string &error)
{
	return [EosPlatform_Mac_Native CreatePlatform:error];
}

////////////////////////////////////////////////////////////////////////////////

+ (NSAutoreleasePool*)CreatePlatform:(std::string&)error
{
	return [[NSAutoreleasePool alloc] init];
}

////////////////////////////////////////////////////////////////////////////////

void Bridge_DestroyPlatform(void *platform)
{
	[EosPlatform_Mac_Native DestroyPlatform:(id)platform];
}

+ (void)DestroyPlatform:(id)platform
{
	NSAutoreleasePool *pool = (NSAutoreleasePool*)platform;
	if(pool != nil)
		[pool release];
}

////////////////////////////////////////////////////////////////////////////////

void* Bridge_BeginActivity(const std::string &reason, std::string &error)
{
	return [EosPlatform_Mac_Native BeginActivity:reason error:error];
}

+ (id)BeginActivity:(const std::string&)reason error:(std::string&)error
{
	id activity = nil;
	
	NSProcessInfo *processInfo = [NSProcessInfo processInfo];
	if(processInfo != nil)
	{
		if([processInfo respondsToSelector:@selector(beginActivityWithOptions:reason:)] == YES)
		{
			const char *reasonStr = ((reason.empty() || reason.c_str()==0)
				? "routing started"
				: reason.c_str());
			activity = [processInfo beginActivityWithOptions:NSActivityUserInitiated|NSActivityLatencyCritical reason:[NSString stringWithUTF8String:reasonStr]];
			if(activity == nil)
				error = "beginActivityWithOptions failed";
			else
				[activity retain];
		}
	}
	else
		error = "could not get current process";
	
	return activity;
}

////////////////////////////////////////////////////////////////////////////////

void Bridge_EndActivity(void *activity)
{
	[EosPlatform_Mac_Native EndActivity:(id)activity];
}

+ (void)EndActivity:(id)activity
{
	if(activity != nil)
	{
		NSProcessInfo *processInfo = [NSProcessInfo processInfo];
		if(processInfo != nil)
		{
			if([processInfo respondsToSelector:@selector(endActivity:)] == YES)
				[processInfo endActivity:activity];
		}
		
		[activity release];
	}
}

////////////////////////////////////////////////////////////////////////////////

@end

////////////////////////////////////////////////////////////////////////////////

void Bridge_InitQtPlugins()
{
    // only load plugins from our bundle
    CFURLRef url = (CFURLRef)CFAutorelease((CFURLRef)CFBundleCopyBundleURL(CFBundleGetMainBundle()));
    if(url != nil)
    {
        QDir dir( QUrl::fromCFURL(url).path() );
        if( dir.cd("Contents/Plugins") )
            QApplication::setLibraryPaths(QStringList() << dir.canonicalPath());
    }
}

////////////////////////////////////////////////////////////////////////////////
