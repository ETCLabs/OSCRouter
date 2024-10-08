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

#include "EosPlatform.h"

#ifndef WIN32
#include "EosPlatform_Mac.h"
#endif

////////////////////////////////////////////////////////////////////////////////

bool EosPlatform::Initialize(std::string& /*error*/)
{
  return true;
}

////////////////////////////////////////////////////////////////////////////////

bool EosPlatform::SetSystemIdleAllowed(bool /*b*/, const std::string& /*reason*/, std::string& error)
{
  error = "not required for this platform";
  return false;
}

////////////////////////////////////////////////////////////////////////////////

EosPlatform* EosPlatform::Create()
{
#ifdef WIN32
  return (new EosPlatform());
#else
  return (new EosPlatform_Mac());
#endif
}

////////////////////////////////////////////////////////////////////////////////
