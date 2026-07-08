/*WinStreamClient.cpp  Implementation of the Windows version of the wrapper for the streaming ACN client
*/
#include <windows.h>
#include <process.h>
#include <map>
#include <vector>

#include "deftypes.h"
#include "cid.h"
#include "ipaddr.h"
#include "asyncsocketinterface.h"
#include "tock.h"

#include "..\StreamACNCliInterface.h"
#include "Win_StreamACNCliInterface.h"
#include "..\Src\StreamClient.h"
#include "WinStreamClient.h"

//The overall creator.  Call Startup on the returned pointer.
//To clean up, call Shutdown on the pointer and call DestroyInstance;
IWinStreamACNCli* IWinStreamACNCli::CreateInstance()
{
	return new CWinStreamClient;
}

//The overall destructor.  Call Shutdown before this function
void IWinStreamACNCli::DestroyInstance(IWinStreamACNCli* p)
{
	delete static_cast<CWinStreamClient*>(p);
}

CWinStreamClient::CWinStreamClient()
{
	m_threadpriority = THREAD_PRIORITY_NORMAL;
}

CWinStreamClient::~CWinStreamClient()
{
}


//Call this to init the library after creation. Can be used right away (if it returns true)
bool CWinStreamClient::Startup(IAsyncSocketServ* psocket, IStreamACNCliNotify* pnotify, int threadpriority, listenTo version)
{
	m_threadpriority = threadpriority;
	m_psocklib = psocket;
	if(InternalStartup(psocket, pnotify, version))
	{
		InitializeCriticalSection(&m_lock);
		return true;
	}
	return false;
}

//Call this to de-init the library before destruction
void CWinStreamClient::Shutdown()
{
	//Calling internal shutdown should close the sockets
	InternalShutdown();

	// wait for all threads to finish
	for (uintptr_t th : m_threads)
	{
		WaitForSingleObject((HANDLE)th, INFINITE);
		CloseHandle((HANDLE)th);
	}

	DeleteCriticalSection(&m_lock);
}


/****************************************
* IStreamACNCli overrides -- Currently, nothing to override, so let them fall through
****************************************/
void CWinStreamClient::FindExpiredSources(){CStreamClient::FindExpiredSources();}
bool CWinStreamClient::ListenUniverse(uint2 universe, netintid* netiflist, int netiflist_size){return CStreamClient::ListenUniverse(universe, netiflist, netiflist_size);}
void CWinStreamClient::EndUniverse(uint2 universe){CStreamClient::EndUniverse(universe);}
void CWinStreamClient::toggleListening(listenTo version, bool on_or_off){CStreamClient::toggleListening(version, on_or_off);}
int CWinStreamClient::listeningTo(){return CStreamClient::listeningTo();}
int CWinStreamClient::setUniversalHoldLastLook(int hold_time){return CStreamClient::setUniversalHoldLastLook(hold_time);}
int CWinStreamClient::getUniversalHoldLastLook(){return CStreamClient::getUniversalHoldLastLook();}
/****************************************
* CStreamClient overrides
****************************************/
//Grabs the simple lock for the source tree
bool CWinStreamClient::GetLock()
{
	EnterCriticalSection(&m_lock);
	return true;
}

//Frees the simple lock
void CWinStreamClient::ReleaseLock()
{
	LeaveCriticalSection(&m_lock);
}

//The Receive thread that recvs into the socket and dies when the socket closes
struct readinfo
{
	IAsyncSocketServ* psocklib;
	CWinStreamClient* pclient;
	sockid id;
};

unsigned __stdcall ReadThread(void* rdinfo)
{
	readinfo* pread = static_cast<readinfo*>(rdinfo);
   uint1 buffer [1500];
	uint buflen = 1500;
	CIPAddr from;

	for(int readlen = pread->psocklib->ReceiveInto(pread->id, from, buffer, buflen);
		 readlen >= 0;
		 readlen = pread->psocklib->ReceiveInto(pread->id, from, buffer, buflen))
	{
		if(readlen > 0) //==0 not an error, but nothing to parse
			pread->pclient->ParsePacket(from, buffer, readlen);
	}

	delete pread;
	return 0;
}

//This class uses manual receive sockets, so the derivative needs to call AsyncSocket::ReceiveInto
//and call ParsePacket with the result.  ReceiveInto is blocking, and requires a read thread to
//drive it, so this function initiates the thread in the OS class.  The OS class Shutdown() function
//will clean up.
bool CWinStreamClient::SpawnSocketThread(sockid id)
{
	readinfo* pread = new readinfo;
	if(!pread)
		return false;
	pread->id = id;
	pread->pclient = this;
	pread->psocklib = m_psocklib;

	unsigned tmpid;
	uintptr_t th = _beginthreadex(NULL, 0, &ReadThread, pread, CREATE_SUSPENDED, &tmpid);
	if(!th)
		return false;
	SetThreadPriority((HANDLE)th, m_threadpriority);
	ResumeThread((HANDLE)th);
	m_threads.insert(th);
	return true;
}

