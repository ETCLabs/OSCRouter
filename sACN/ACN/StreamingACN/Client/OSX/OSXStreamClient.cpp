/*OSXStreamClient.cpp  Implementation of the OSX version of the wrapper for the streaming ACN client
*/


#include <map>
#include <vector>

#include <pthread.h>

#include "deftypes.h"
#include "CID.h"
#include "ipaddr.h"
#include "AsyncSocketInterface.h"
#include "tock.h"

#include "../StreamACNCliInterface.h"
#include "OSX_StreamACNCliInterface.h"
#include "../Src/StreamClient.h"
#include "OSXStreamClient.h"

//The overall creator.  Call Startup on the returned pointer.
//To clean up, call Shutdown on the pointer and call DestroyInstance;
IOSXStreamACNCli* IOSXStreamACNCli::CreateInstance()
{
	return new COSXStreamClient;
}

//The overall destructor.  Call Shutdown before this function
void IOSXStreamACNCli::DestroyInstance(IOSXStreamACNCli* p)
{
	delete static_cast<COSXStreamClient*>(p);
}

COSXStreamClient::COSXStreamClient()
{
	m_lockinitted = false;
}

COSXStreamClient::~COSXStreamClient()
{
	if(m_lockinitted)
		Shutdown();
}


//Call this to init the library after creation. Can be used right away (if it returns true)
//Each socket will use a thread to perform a blocking recv from, so we can also manipulate their
//priorities, etc, here.
bool COSXStreamClient::Startup(IAsyncSocketServ* psocket, IStreamACNCliNotify* pnotify, listenTo version)
{
	m_psocklib = psocket;

	if(InternalStartup(psocket, pnotify, version))
	{
		m_lockinitted = (0 == pthread_mutex_init(&m_lock, NULL));
		return m_lockinitted;
	}
	return false;
}

//Call this to de-init the library before destruction
void COSXStreamClient::Shutdown()
{
	//Calling internal shutdown should close the sockets
	InternalShutdown();

	// wait for all threads to finish
	for (pthread_t th : m_threads)
		pthread_join(th, nullptr);

	if(m_lockinitted)
	{
		pthread_mutex_destroy(&m_lock);
		m_lockinitted = false;
	}
}


/****************************************
* IStreamACNCli overrides -- Currently, nothing to override, so let them fall through
****************************************/
void COSXStreamClient::FindExpiredSources(){CStreamClient::FindExpiredSources();}
bool COSXStreamClient::ListenUniverse(uint2 universe, netintid* netiflist, int netiflist_size){return CStreamClient::ListenUniverse(universe, netiflist, netiflist_size);}
void COSXStreamClient::EndUniverse(uint2 universe){CStreamClient::EndUniverse(universe);}
void COSXStreamClient::toggleListening(listenTo version, bool on_or_off){CStreamClient::toggleListening(version, on_or_off);}
int COSXStreamClient::listeningTo(){return CStreamClient::listeningTo();}
int COSXStreamClient::setUniversalHoldLastLook(int hold_time){return CStreamClient::setUniversalHoldLastLook(hold_time);}
int COSXStreamClient::getUniversalHoldLastLook(){return CStreamClient::getUniversalHoldLastLook();}
/****************************************
* CStreamClient overrides
****************************************/
//Grabs the simple lock for the source tree
bool COSXStreamClient::GetLock()
{
	return (m_lockinitted && (0 == pthread_mutex_lock(&m_lock)));
}

//Frees the simple lock
void COSXStreamClient::ReleaseLock()
{
	if(m_lockinitted)
		pthread_mutex_unlock(&m_lock);
}

//The Receive thread that recvs into the socket and dies when the socket closes
struct readinfo
{
	IAsyncSocketServ* psocklib;
	COSXStreamClient* pclient;
	sockid id;
};

void* StreamClient_ReadThread(void* p)
{
	struct readinfo* pinfo = reinterpret_cast<struct readinfo*>(p);

	uint1 buffer [1500];
	uint buflen = 1500;
	CIPAddr from;

	for(int readlen = pinfo->psocklib->ReceiveInto(pinfo->id, from, buffer, buflen);
		 readlen >= 0;
		 readlen = pinfo->psocklib->ReceiveInto(pinfo->id, from, buffer, buflen))
	{
		if(readlen > 0) //==0 not an error, but nothing to parse
			pinfo->pclient->ParsePacket(from, buffer, readlen);
	}

	delete pinfo;	
	return 0;
}

//This class uses manual receive sockets, so the derivative needs to call AsyncSocket::ReceiveInto
//and call ParsePacket with the result.  ReceiveInto is blocking, and requires a read thread to
//drive it, so this function initiates the thread in the OS class.  The OS class Shutdown() function
//will clean up.
bool COSXStreamClient::SpawnSocketThread(sockid id)
{
	pthread_t th;
	struct readinfo* p = new struct readinfo;
	if(!p)
		return false;
	p->id = id;
	p->pclient = this;
	p->psocklib = m_psocklib;
	
	if (pthread_create(&th, NULL, StreamClient_ReadThread, p) != 0)
		return false;

	m_threads.insert(th);
	return true;
}

