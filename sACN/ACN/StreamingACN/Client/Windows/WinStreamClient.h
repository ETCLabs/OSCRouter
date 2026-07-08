/*WinStreamClient.h  Windows implementation IStreamACNCli class.
  See StreamACNCliInterface.h and Win_StreamACNCliInterface.h for details.
*/

#ifndef _WINSTREAMCLIENT_H_
#define _WINSTREAMCLIENT_H_

#ifndef _WIN_STREAMACNCLIINTERFACE_H_
#error "#include error: WinStreamClient.h requires Win_StreamACNCliInterface.h"
#endif
#ifndef _STREAMCLIENT_H_
#error "#include error: WinStreamClient.h requires StreamClient.h"
#endif

#include <unordered_set>

class CWinStreamClient : public IWinStreamACNCli, public CStreamClient
{
public:
   /***************************************
	 * IWin_StreamACNCli overrides
	 ***************************************/
	//Call this to init the library after creation. Can be used right away (if it returns true)
  //	virtual bool Startup(IAsyncSocketServ* psocket, IStreamACNCliNotify* pnotify, int threadpriority);

        virtual bool CWinStreamClient::Startup(IAsyncSocketServ* psocket, IStreamACNCliNotify* pnotify, int threadpriority, listenTo version);

	//Call this to de-init the library before destruction
	virtual void Shutdown();

	/****************************************
	 * IStreamACNCli overrides  
	 ****************************************/
	virtual void FindExpiredSources();
	virtual bool ListenUniverse(uint2 universe, netintid* netiflist, int netiflist_size);
	virtual void EndUniverse(uint2 universe);
        virtual void toggleListening(listenTo version, bool on_or_off);
        virtual int listeningTo();
        virtual int setUniversalHoldLastLook(int hold_time);
        virtual int getUniversalHoldLastLook();
	CWinStreamClient();  
	virtual ~CWinStreamClient();

	/****************************************
	 * CStreamClient overrides
	 ****************************************/
	//Grabs the simple lock for the source tree
	virtual bool GetLock();

	//Frees the simple lock
	virtual void ReleaseLock();

	//This class uses manual receive sockets, so the derivative needs to call AsyncSocket::ReceiveInto
	//and call ParsePacket with the result.  ReceiveInto is blocking, and requires a read thread to
	//drive it, so this function initiates the thread in the OS class.  The OS class Shutdown() function
	//will clean up.
	virtual bool SpawnSocketThread(sockid id);
						
protected:
	CRITICAL_SECTION m_lock;  
	IAsyncSocketServ* m_psocklib;
	int m_threadpriority;  //The priority to use with the read threads.
	std::unordered_set<uintptr_t> m_threads;
};

#endif

