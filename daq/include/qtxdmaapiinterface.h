#ifndef QTXDMAAPIINTERFACE_H
#define QTXDMAAPIINTERFACE_H

#include "QTXdmaApi.h"


/*
函数名：Func_QTXdmaReadRegister()
函数作用：通过调用该函数可以实现对板卡上的寄存器进行读操作。
参数说明：
STXDMA_CARDINFO *g_stCardInfo：定义指向板卡信息结构体的指针。
uint32_t base：定义要读取的基地址（无符号32位整型）
uint32_t offset：定义要读取的偏移地址（无符号32位整型）
uint32_t *value：
bool bWrilteLog = true：
*/

class QTXdmaApiInterface
{
public:
    QTXdmaApiInterface();
public:
/*
函数名：Func_QTXdmaReadRegister()
函数作用：通过调用该函数可以实现对板卡上的寄存器进行读操作。
参数说明：
STXDMA_CARDINFO *g_stCardInfo：定义指向板卡信息结构体的指针。
uint32_t base：定义要读取的基地址（无符号32位整型）
uint32_t offset：定义要读取的偏移地址（无符号32位整型）
uint32_t *value：
bool bWrilteLog = true：
*/
    static int Func_QTXdmaReadRegister(STXDMA_CARDINFO *g_stCardInfo, uint64_t base, uint32_t offset, uint64_t *value, bool bWrilteLog = true);
    static int Func_QTXdmaWriteRegister(STXDMA_CARDINFO *g_stCardInfo, uint64_t base, uint32_t offset, uint64_t value);
};

#endif // QTXDMAAPIINTERFACE_H
