
#include <windows.h>
#include "Mutex.h"

namespace mt {

//互斥锁数据
struct Mutex::MutexData
{
	//临界区
	CRITICAL_SECTION critSection;
};

Mutex::Mutex()
{
	m_mutexData = new MutexData();

	InitializeCriticalSection(&m_mutexData->critSection);
}

Mutex::~Mutex()
{
	DeleteCriticalSection(&m_mutexData->critSection);

	delete m_mutexData;

	m_mutexData = NULL;
}

/**
 * Locks an existing mutex
 */
void Mutex::Lock()
{
	if(m_mutexData)
		EnterCriticalSection(&m_mutexData->critSection);
}

/**
 * Unlocks a mutex owned by the thread.
 */
void Mutex::Unlock()
{
	if(m_mutexData)
		LeaveCriticalSection(&m_mutexData->critSection);
}

}
