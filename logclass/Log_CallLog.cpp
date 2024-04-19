// Log_CallLog.cpp: implementation of the CLog_CallLog class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "WonderTac_EncryptorSrv.h"
#include "Log_CallLog.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

inline CLog_CallLog::CLog_CallLog(Log_TraceLog *log, const std::string &func, const std::string &parm, long level)
: m_log(log), m_func(func), m_parm(parm), m_level(level)
{
	char buf[20];
	
	sprintf(buf, "[%08X] ", GetCurrentThreadId());
	
	std::string str(buf);
	str += "Entering " + m_func + "(" + m_parm + ")";
	m_log->Trace(m_level, (char *)str.c_str());
}

inline CLog_CallLog::~CLog_CallLog()
{
	char buf[20];
	
	sprintf(buf, "[%08X] ", GetCurrentThreadId());
	
	std::string str(buf);
	str += "Leaving " + m_func + "()";
	m_log->Trace(m_level, (char *)str.c_str());
}
