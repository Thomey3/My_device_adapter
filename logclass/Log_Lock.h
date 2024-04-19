// Log_Lock.h: interface for the CLog_Lock class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_LOG_LOCK_H__CE9B9748_0488_4926_A5C9_99E3D881CD27__INCLUDED_)
#define AFX_LOG_LOCK_H__CE9B9748_0488_4926_A5C9_99E3D881CD27__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CLog_Lock  
{
public:
	CLog_Lock();
	~CLog_Lock();
	
	void Lock();
	void Unlock();
	
private:
	CRITICAL_SECTION m_CriticalSection;
};

inline CLog_Lock::CLog_Lock()
{
	::InitializeCriticalSection( &m_CriticalSection );
}

inline CLog_Lock::~CLog_Lock()
{
	::DeleteCriticalSection( &m_CriticalSection );
}

inline void CLog_Lock::Lock()
{
	::EnterCriticalSection( &m_CriticalSection );
}

inline void CLog_Lock::Unlock()
{
	::LeaveCriticalSection( &m_CriticalSection );
}

#endif // !defined(AFX_LOG_LOCK_H__CE9B9748_0488_4926_A5C9_99E3D881CD27__INCLUDED_)
