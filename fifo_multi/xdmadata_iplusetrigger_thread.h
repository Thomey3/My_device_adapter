#ifndef XDMADATA_IPLUSETRIGGER_THREAD_H
#define XDMADATA_IPLUSETRIGGER_THREAD_H

#include <string>
using namespace std;

#include "../../3rd/include/pthread.h"

class xdmadata_iplusetrigger_thread
{
    public:
        explicit xdmadata_iplusetrigger_thread();
        void start();
        void stop();
        void setFileName(const string& filename);
        int getResult();
        void resetResult();
        void set_dmaTotalBytes(const int& iBytes);
        void set_filePath(const string& filepath);
        void set_runningstate(bool bstate);
        void set_triggertype(const int& iTriggerType);
        void set_hostdatablock(const int& iHostDataBlock);
        void set_GatherType(int iType);
        void set_BoardIndex(int iBoardIndex);
    protected:
	    static void * run(void* Param);
    private:
        volatile bool stopped;
        static string m_filename;
        static string m_filepath;
        static int m_iResult;
        static int m_iDMATotalBytes;
        static bool m_bRunningState;
        static int m_iTriggerType;
        static uint8_t * m_transferbuffer;
        static int m_iHostDataBlockSize;

        static int m_iGatherType;//采集模式 1-interrupt 中断 2-polling 轮询
        static int m_iBoardIndex;//板卡编号

};

#endif