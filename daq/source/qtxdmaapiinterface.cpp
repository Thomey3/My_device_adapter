#include "qtxdmaapiinterface.h"

#include "TraceLog.h"

Log_TraceLog g_LogReg(std::string("./logs/KunchiUpperMonitorReg.log"));
Log_TraceLog * pLogReg = &g_LogReg;

extern void printfLog(int nLevel, const char * fmt, ...);

void printfLogReg(int nLevel, const char * fmt, ...)
{
	if (pLogReg == NULL)
		return;

	char buf[1024];
	va_list list;
	va_start(list, fmt);
	vsprintf_s(buf, fmt, list);
	va_end(list);

	pLogReg->Trace(nLevel, buf);
}

QTXdmaApiInterface::QTXdmaApiInterface()
{

}

int QTXdmaApiInterface::Func_QTXdmaReadRegister(STXDMA_CARDINFO *g_stCardInfo, uint64_t base, uint32_t offset, uint64_t *value, bool bWrilteLog)
{
    if(!g_stCardInfo){
        printfLogReg(5, "QTXdmaApiInterface::Func_QTXdmaReadRegister, g_stCardInfo is NULL");
        return -1;
    }

    if(bWrilteLog)
        printfLogReg(5, "[QTXdmaApiInterface::Func_QTXdmaReadRegister], base 0x%x, offset 0x%x", base, offset);

    return QTXdmaReadRegister(g_stCardInfo, base, offset, value);
}

int QTXdmaApiInterface::Func_QTXdmaWriteRegister(STXDMA_CARDINFO *g_stCardInfo, uint64_t base, uint32_t offset, uint64_t value)
{
    if(!g_stCardInfo){
        printfLogReg(5, "QTXdmaApiInterface::Func_QTXdmaWriteRegister, g_stCardInfo is NULL");
        return -1;
    }

    printfLogReg(5, "[QTXdmaApiInterface::Func_QTXdmaWriteRegister], base 0x%x, offset 0x%x, value 0x%x", base, offset, value);
    return QTXdmaWriteRegister(g_stCardInfo, base, offset, value);
}
