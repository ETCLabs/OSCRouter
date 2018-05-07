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

#include "EosPlatform_Mac.h"
#include "EosPlatform_Mac_Bridge.h"

////////////////////////////////////////////////////////////////////////////////

EosPlatform_Mac::EosPlatform_Mac()
	: m_Platform(0)
	, m_Activity(0)
{
}

////////////////////////////////////////////////////////////////////////////////

EosPlatform_Mac::~EosPlatform_Mac()
{
	Shutdown();
}

////////////////////////////////////////////////////////////////////////////////

bool EosPlatform_Mac::Initialize(std::string &error)
{
	if(m_Platform == 0)
    {
		m_Platform = Bridge_CreatePlatform(error);
        Bridge_InitQtPlugins();
    }
	
	return (m_Platform != 0);
}

////////////////////////////////////////////////////////////////////////////////

void EosPlatform_Mac::Shutdown()
{
	if( m_Platform )
	{
		std::string dummy;
		SetSystemIdleAllowed(true, "shutdown", dummy);
		Bridge_DestroyPlatform(m_Platform);
		m_Platform = 0;
	}
}

////////////////////////////////////////////////////////////////////////////////

bool EosPlatform_Mac::SetSystemIdleAllowed(bool b, const std::string &reason, std::string &error)
{
	if( m_Platform )
	{
		if( b )
		{
			if( m_Activity )
			{
				Bridge_EndActivity(m_Activity);
				m_Activity = 0;
			}
		}
		else if( !m_Activity )
		{
			m_Activity = Bridge_BeginActivity(reason, error);
			if( !m_Activity )
				return false;
		}
	}
	else
	{
		error = "invalid platform";
		return false;
	}
	
	return true;
}

////////////////////////////////////////////////////////////////////////////////
