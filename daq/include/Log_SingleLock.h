// Log_SingleLock.h: interface for the CLog_SingleLock class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_LOG_SINGLELOCK_H__20A94895_072A_45F7_BC8B_FC52FDFB1705__INCLUDED_)
#define AFX_LOG_SINGLELOCK_H__20A94895_072A_45F7_BC8B_FC52FDFB1705__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Log_Lock.h"

class CLog_SingleLock  
{
public:
	CLog_SingleLock(CLog_Lock *lock);
	~CLog_SingleLock();
	
	void Lock();
	
private:
	CLog_Lock * mlock;
};

inline CLog_SingleLock::CLog_SingleLock(CLog_Lock *lock)
: mlock(lock)
{
}

inline CLog_SingleLock::~CLog_SingleLock()
{
	if(mlock) mlock->Unlock();
}

inline void CLog_SingleLock::Lock()
{
	if(mlock) mlock->Lock();
}

#endif // !defined(AFX_LOG_SINGLELOCK_H__20A94895_072A_45F7_BC8B_FC52FDFB1705__INCLUDED_)
