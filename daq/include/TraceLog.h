
#ifndef __TRACELOG__H__
#define __TRACELOG__H__


/***********************************************************
Include
***********************************************************/
#include <windows.h>
//#include <windef.h>
//#include <basetsd.h>
//#include <winbase.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <io.h>
#include <string>

#include "Log_Lock.h"

/***********************************************************
                        define
***********************************************************/
#define TIMESTRING_NORMAL		1				//YYYY-MM-DD HH24:MI:SS
#define TIMESTRING_NORMAL2		2				//YYYYMMDDHH24MISS
#define TIMESTRING_UNNORMAL		3				//MMDDHH24MISS.000


/***********************************************************
                        Log_Lock
// CriticalSection Lock:
// The object was locked until Unlock() was called.
***********************************************************/

/*
class Log_Lock
{
public:
	Log_Lock();
	~Log_Lock();

	void			Lock();
	void			Unlock();

private:
	CRITICAL_SECTION m_CriticalSection;
};

inline Log_Lock::Log_Lock()
{
	::InitializeCriticalSection( &m_CriticalSection );
}

inline Log_Lock::~Log_Lock()
{
	::DeleteCriticalSection( &m_CriticalSection );
}

inline void Log_Lock::Lock()
{
	::EnterCriticalSection( &m_CriticalSection );
}

inline void Log_Lock::Unlock()
{
	::LeaveCriticalSection( &m_CriticalSection );
}
*

/***********************************************************
                        Log_SingleLock
***********************************************************/

/*
class Log_SingleLock
{
public:
	Log_SingleLock(Log_Lock *lock);
	~Log_SingleLock();

	void Lock();

private:
	Log_Lock*		mlock;
};


inline Log_SingleLock::Log_SingleLock(Log_Lock *lock)
	: mlock(lock)
{
}

inline Log_SingleLock::~Log_SingleLock()
{
	if(mlock) mlock->Unlock();
}

inline void Log_SingleLock::Lock()
{
	if(mlock) mlock->Lock();
}

  */

/***********************************************************
                        Log_TraceLog
***********************************************************/
class Log_TraceLog
{
public:
	Log_TraceLog(std::string fname,long level = 5,unsigned long maxsize = 5*1024*1024);
	Log_TraceLog(std::string fname,bool bAddDateInFileName);
	virtual ~Log_TraceLog();

	void TraceNoTime(long level, const char *format ,...);	//最高效：不带时间戳
	void Trace(long level, const char *format ,...);		//高效：带时间戳
	void TraceInfo(long level, const char *format ,...);
	void LogTime(long level);
	void TraceLevel(long level);
	bool Passed(long level);
	std::string GetDateTimeString(long timestringtype = TIMESTRING_NORMAL);
	static std::string GetDateString();
	std::string GetTimeString();
	std::string GetTraceLevelTypeString();						//获取日志级别名称
	std::string GetLogName();
	int		GetLogLevel();
	unsigned long GetMaxSize();
	bool	SetMaxSize(unsigned long nSize);
	void Working();											//循环，删除超时的文件	

private:
	long				m_Level;
	std::string			m_Logname;
	FILE*				m_Handle;
	unsigned long		m_MaxSize;
	CLog_Lock			m_Lock;
	std::string			m_strCurDate;
	std::string			m_strPrefix;
private:
	//自动日志删除的线程控制
	HANDLE				m_Thread;
	DWORD				m_ThreadID;
	
};

/***********************************************************
                        Log_CallLog
***********************************************************/

/*
class Log_CallLog
{
public:
	Log_CallLog(Log_TraceLog *log, const std::string &func, const std::string &parm = "", long level = 5);
	virtual ~Log_CallLog();
private:
	Log_TraceLog *m_log;
	std::string m_func;
	std::string m_parm;
	long m_level;
};
*/

/**************************\
** inline implementations **
\**************************/

inline void Log_TraceLog::TraceLevel(long level)
{ 
	m_Level = level; 
	Trace(1,"LogLevel Change: %s.", GetTraceLevelTypeString() );
}

inline bool Log_TraceLog::Passed(long level)
{
	return level<=m_Level ? true : false;
}


#endif 
