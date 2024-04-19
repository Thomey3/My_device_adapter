#pragma once
#ifndef PINGPONG_EXAMPLE_H
#define PINGPONG_EXAMPLE_H

#include "QTXdmaApi.h"
#include "qtpciexdma.h"
#include "configdata.h"
#include "pub.h"
#include "qtxdmaapiinterface.h"
#include <stdio.h>
#include <time.h>
#include <windows.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif
	//函数功能: 获取板卡信息（板卡型号、软件版本、采样率、通道数、分辨率）
	//函数参数：
	//函数返回: 
	void QT_BoardGetCardInfo();

	//函数功能: 清除中断
	//函数参数：
	//函数返回: 成功返回0,失败返回-1并把详细错误信息写入日志文件
	int QT_BoardSetInterruptClear();

	//函数功能: 上位机停止采集ADC数据
	//函数参数：
	//函数返回: 成功返回0,失败返回-1并把详细错误信息写入日志文件
	int QT_BoardSetADCStop();

	//函数功能: 上位机软件复位
	//函数参数：
	//函数返回: 成功返回0,失败返回-1并把详细错误信息写入日志文件
	int QT_BoardSetSoftReset();

	//函数功能: 设置预触发
	//函数参数：pre_trig_length：预触发点数
	//函数返回: 成功返回0,失败返回-1并把详细错误信息写入日志文件
	int QT_BoardSetPerTrigger(uint32_t pre_trig_length);

	//函数功能: 设置帧头是否使能
	//函数参数：frameheaderenable：0:禁用  1:使能
	//函数返回: 成功返回0,失败返回-1并把详细错误信息写入日志文件
	int QT_BoardSetFrameheader(uint32_t frameheaderenable);

	//函数功能: 设置时钟模式
	//函数参数：clockmode：时钟模式 0:内参考时钟 1:外采样时钟 2:外参考时钟
	//函数返回: 成功返回0,失败返回-1并把详细错误信息写入日志文件
	int QT_BoardSetClockMode(uint32_t clockmode);

	//函数功能: 设置StdSingle DMA传输参数
	//函数参数：once_trig_bytes：单次触发数据量   DMATotolbytes：中断数据量
	//函数返回: 成功返回0,失败返回-1并把详细错误信息写入日志文件
	int QT_BoardSetStdSingleDMAParameter(uint32_t once_trig_bytes, uint32_t DMATotolbytes);

	//函数功能: 设置StdMulti DMA传输参数
	//函数参数：once_trig_bytes：单次触发数据量   DMATotolbytes：中断数据量
	//函数返回: 成功返回0,失败返回-1并把详细错误信息写入日志文件
	int QT_BoardSetStdMultiDMAParameter(uint32_t once_trig_bytes, uint32_t DMATotolbytes);

	//函数功能: 设置FifoSingle DMA传输参数
	//函数参数：once_trig_bytes：单次触发数据量   DMATotolbytes：中断数据量
	//函数返回: 成功返回0,失败返回-1并把详细错误信息写入日志文件
	int QT_BoardSetFifoSingleDMAParameter(uint32_t once_trig_bytes, uint32_t DMATotolbytes);

	//函数功能: 设置FifoMulti DMA传输参数
	//函数参数：once_trig_bytes：单次触发数据量   DMATotolbytes：中断数据量
	//函数返回: 成功返回0,失败返回-1并把详细错误信息写入日志文件
	int QT_BoardSetFifoMultiDMAParameter(uint32_t once_trig_bytes, uint32_t DMATotolbytes);

	//函数功能: 设置传输模式为有限次传输模式
	//函数参数：transmitpoints：0为有限点 1为无限点   transmittimes：DMA0传输次数 0有限次 1无限次
	//函数返回: 成功返回0,失败返回-1并把详细错误信息写入日志文件
	int QT_BoardSetTransmitMode(int transmitpoints, int transmittimes);

	//函数功能: 软件触发
	//函数参数：
	//函数返回: 成功返回0,失败返回-1并把详细错误信息写入日志文件
	int QT_BoardSoftTrigger();

	//函数功能: 内部脉冲触发
	//函数参数：counts：触发次数  pulse_period：内部脉冲触发周期  pulse_width：内部脉冲触发宽度
	//函数返回: 成功返回0,失败返回-1并把详细错误信息写入日志文件
	int QT_BoardInternalPulseTrigger(int counts, int pulse_period, int pulse_width);

	//函数功能: 外部脉冲触发
	//函数参数：mode：触发模式  counts：触发次数
	//函数返回: 成功返回0,失败返回-1并把详细错误信息写入日志文件
	int QT_BoardExternalTrigger(int mode, int counts);

	//函数功能: 通道触发
	//函数参数：mode 设置通道触发触发模式  counts：触发次数  channelID 设置通道触发触发通道ID号  rasing_codevalue 设置通道上升沿触发电平  falling_codevalue 设置通道下降沿触发电平
	//函数返回: 成功返回0,失败返回-1并把详细错误信息写入日志文件
	int QT_BoardChannelTrigger(int mode, int counts, int channelID, int rasing_codevalue, int falling_codevalue);

	//函数功能: PCIe中断使能
	//函数参数：
	//函数返回: 成功返回0,失败返回-1并把详细错误信息写入日志文件
	int QT_BoardSetInterruptSwitch();

	//函数功能: 上位机开始采集ADC数据
	//函数参数：
	//函数返回: 成功返回0,失败返回-1并把详细错误信息写入日志文件
	int QT_BoardSetADCStart();

	//函数功能: 中断模式
	//函数参数：
	//函数返回: 成功返回0,失败返回-1并把详细错误信息写入日志文件
	void QT_BoardInterruptGatherType();

	//函数功能: 轮询模式
	//函数参数：
	//函数返回: 成功返回0,失败返回-1并把详细错误信息写入日志文件
	void QT_BoardPollingGatherType();

	//函数功能: 计算频率控制字
	//函数参数：bitWidth：bit位宽  ddsInputClockMHz：DDS时钟频率  ddsOutputFreqMHz：DDS输出频率  ctrlWord：频率控制字
	//函数返回: 成功返回0,失败返回-1并把详细错误信息写入日志文件
	int phase_inc(double bitWidth, double ddsInputClockMHz, double ddsOutputFreqMHz, uint32_t *ctrlWord);

	//函数功能: 计算相位控制字
	//函数参数：bitWidth：bit位宽  ddsInputClockMHz：DDS时钟频率  ddsOutputFreqMHz：DDS输出频率  ddsCount：DDS个数  ctrlWord：频率控制字
	//函数返回: 成功返回0,失败返回-1并把详细错误信息写入日志文件
	int phase_offset(double bitWidth, double ddsInputClockMHz, double ddsOutputFreqMHz, int ddsCount, uint32_t *ctrlWord);

	//函数功能: 下发控制字
	//函数参数：freq：频率控制字   phase_inc：相位控制字
	//函数返回: 成功返回0,失败返回-1并把详细错误信息写入日志文件
	int QT_BoardSetDacPhaseAndFreq(uint64_t freq, uint64_t phase_inc);

	//函数功能: 更新DDS输出频率
	//函数参数：
	//函数返回: 成功返回0,失败返回-1并把详细错误信息写入日志文件
	int QT_BoardUpdateTheOutputFrequency();

	//函数功能: DAC播放源
	//函数参数：sourceId：0:DDS   1:文件
	//函数返回: 成功返回0,失败返回-1并把详细错误信息写入日志文件
	int QT_BoardSetDacPlaySourceSelect(uint32_t sourceId);

	//函数功能: 上位机开始/停止播放DAC数据
	//函数参数：enable：开始/停止
	//函数返回: 成功返回0,失败返回-1并把详细错误信息写入日志文件
	int QT_BoardSetDaPlayStart(bool enable);

	//函数功能: DAC播放通道选择
	//函数参数：channelId：通道ID
	//函数返回: 成功返回0,失败返回-1并把详细错误信息写入日志文件
	int QT_BoardSetDacPlayChannelSelect(uint32_t channelId);

	//函数功能: DAC循环读取  连续：010    单次：000
	//函数参数：enable 循环读取使能 只在有限次传输模式下起作用,1=enable, 0=disable  count 循环读取次数(0=无限次循环，直至循环使能中止)
	//函数返回: 成功返回0,失败返回-1并把详细错误信息写入日志文件
	int QT_BoardSetDACLoopRead(int enable, uint32_t count);

	//函数功能: DAC触发模式
	//函数参数：enable： count
	//函数返回: 成功返回0,失败返回-1并把详细错误信息写入日志文件
	int QT_BoardSetDacTriggerMode(uint32_t triggermode);

	//函数功能: DAC DMA参数设置
	//函数参数：uWriteStartAddr：DDR地址  uPlaySize：下发数据量
	//函数返回: 成功返回0,失败返回-1并把详细错误信息写入日志文件
	int QT_BoardSetDacRegister(uint32_t uWriteStartAddr, uint32_t uPlaySize);

	//函数功能: 偏置调节
	//函数参数：channel_id：通道ID    offset_value：偏置值
	//函数返回: 成功返回0,失败返回-1并把详细错误信息写入日志文件
	int QT_BoardSetOffset(int channel_id, int offset_value);

#ifdef __cplusplus
}
#endif
