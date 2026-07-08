/*OSXStreamClient.h  OSX implementation IStreamACNCli class.
  See StreamACNCliInterface.h and OSX_StreamACNCliInterface.h for details.
*/

#ifndef _OSXSTREAMCLIENT_H_
#define _OSXSTREAMCLIENT_H_

#ifndef _OSX_STREAMACNCLIINTERFACE_H_
#error "#include error: OSXStreamClient.h requires OSX_StreamACNCliInterface.h"
#endif
#ifndef _STREAMCLIENT_H_
#error "#include error: OSXStreamClient.h requires StreamClient.h"
#endif

#include <unordered_set>

class COSXStreamClient : public IOSXStreamACNCli, public CStreamClient
{
public:
   /***************************************
	 * IOSXStreamACNCli overrides
	 ***************************************/
	//Call this to init the library after creation. Can be used right away (if it returns true)
   //Each socket will use a thread to perform a blocking recv from, so we can also manipulate their
   //priorities, etc, here.
  virtual bool Startup(IAsyncSocketServ* psocket, IStreamACNCliNotify* pnotify, listenTo version);

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
	COSXStreamClient();  
	virtual ~COSXStreamClient();

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
	bool m_lockinitted;
	pthread_mutex_t m_lock;  //The simple binary lock
	IAsyncSocketServ* m_psocklib;
	std::unordered_set<pthread_t> m_threads;
};

#endif

