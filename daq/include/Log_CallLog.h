// Log_CallLog.h: interface for the CLog_CallLog class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_LOG_CALLLOG_H__C6FD590F_C71A_4F2A_BAA3_393743B26131__INCLUDED_)
#define AFX_LOG_CALLLOG_H__C6FD590F_C71A_4F2A_BAA3_393743B26131__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "../public/TraceLog.h"

class CLog_CallLog  
{
public:
	CLog_CallLog(Log_TraceLog *log, const std::string &func, const std::string &parm = "", long level = 5);
	virtual ~CLog_CallLog();
private:
	Log_TraceLog *m_log;
	std::string m_func;
	std::string m_parm;
	long m_level;
};

#endif // !defined(AFX_LOG_CALLLOG_H__C6FD590F_C71A_4F2A_BAA3_393743B26131__INCLUDED_)
