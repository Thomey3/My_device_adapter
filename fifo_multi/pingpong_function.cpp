#include "pingpong_example.h"

extern STXDMA_CARDINFO pstCardInfo;

void SetColor(UINT uFore, UINT uBack) {
	HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleTextAttribute(handle, uFore + uBack * 0x10);
}

void QT_BoardGetCardInfo()
{
	uint64_t uintValue = 0;
	QTXdmaApiInterface::Func_QTXdmaReadRegister(&pstCardInfo, BASE_BOARD_INFO, OFFSET_BDINFO_BDINFO, &uintValue);
	string hexstr = pub::DecIntToHexStr(uintValue);
	//printf("Card Type is %s\n", hexstr.data());
	QTXdmaApiInterface::Func_QTXdmaReadRegister(&pstCardInfo, BASE_BOARD_INFO, OFFSET_BDINFO_SOFT_VER, &uintValue);
	string vexstr = pub::DecIntToHexStr(uintValue);
	//printf("Card Logical Version is %s\n", vexstr.data());
	QTXdmaApiInterface::Func_QTXdmaReadRegister(&pstCardInfo, BASE_BOARD_INFO, OFFSET_BDINFO_ADC, &uintValue);
	string hexAdcStr = pub::DecIntToHexStr(uintValue);//表示转换成16进制存入字符串
	int strLen = hexAdcStr.length();
	uintValue = std::stoi(hexAdcStr);//将10进制的字符串1200转化为10进制数值
	uint32_t c1 = uintValue / 10000;//采样率
	uint32_t c2 = uintValue / 100 - (uintValue / 10000) * 100;//通道数
	uint32_t c3 = uintValue - (uintValue / 100) * 100;//分辨率
	//printf("Card Sample Rate is %d\n", c1);
	//printf("Card Channel Number is %d\n", c2);
	//printf("Card Resolution is %d\n", c3);
	uint64_t CurrentStates = 0;
	QTXdmaApiInterface::Func_QTXdmaReadRegister(&pstCardInfo, BASE_BOARD_INFO, 0x20, &CurrentStates);
	//printf("Card Status is %#x\n\n", CurrentStates);//状态值
}

int QT_BoardSetPerTrigger(uint32_t pre_trig_length)
{
	int ret = 0;
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_TRIG_CTRL, 0x28, pre_trig_length);
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_TRIG_CTRL, 0x2C, 1);
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_TRIG_CTRL, 0x30, 0);
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_TRIG_CTRL, 0x30, 1);
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_TRIG_CTRL, 0x30, 0);
	return ret;
}

int QT_BoardSetFrameheader(uint32_t frameheaderenable)
{
	int ret = 0;
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_TRIG_CTRL, 0x38, frameheaderenable);
	return ret;
}

int  QT_BoardSetTransmitMode(int transmitpoints, int transmittimes)
{
	int ret = 0;
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_DMA_ADC, 0x00, transmitpoints);
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_DMA_ADC, 0x20, transmittimes);
	return ret;
}

int QT_BoardSetClockMode(uint32_t clockmode)
{
	int ret = 0;
	uint64_t readclockbackvalue = 0;
	QTXdmaApiInterface::Func_QTXdmaReadRegister(&pstCardInfo, 0x2000, 0x04, &readclockbackvalue);
	if (clockmode != readclockbackvalue)
	{
		QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, 0x2000, 0x04, clockmode);
		QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, 0x2000, 0x08, 0);
		QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, 0x2000, 0x08, 1);
		QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, 0x2000, 0x08, 0);
		Sleep(50);
	}
	return ret;
}

int QT_BoardSetInterruptSwitch()
{
	int ret = 0;
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_PCIE_INTR, 0x00, 1);
	return ret;

}

int QT_BoardSetADCStart()
{
	int ret = 0;
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_PCIE_INTR, 0x08, 0);
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_PCIE_INTR, 0x08, 1);
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_PCIE_INTR, 0x08, 0);
	return ret;
}

int QT_BoardSetInterruptClear()
{
	int ret = 0;
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_PCIE_INTR, 0x04, 0);
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_PCIE_INTR, 0x04, 1);
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_PCIE_INTR, 0x04, 0);
	return ret;
}

int QT_BoardSetADCStop()
{
	int ret = 0;
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_PCIE_INTR, 0x10, 0);
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_PCIE_INTR, 0x10, 1);
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_PCIE_INTR, 0x10, 0);
	return ret;
}

void QT_BoardInterruptGatherType()
{
	uint32_t inpoint = 0;
	do {
		inpoint = QTXdmaGetOneEvent(&pstCardInfo);
		if (inpoint == 1)
		{
			QT_BoardSetInterruptClear();
		}
	} while (inpoint != 1);
}

void QT_BoardPollingGatherType()
{
	uint64_t uTriggerpoint = 0;
	do
	{
		QTXdmaApiInterface::Func_QTXdmaReadRegister(&pstCardInfo, BASE_PCIE_INTR, 0x1C, &uTriggerpoint, false);
		if (uTriggerpoint == 1) {
			QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_PCIE_INTR, OFFSET_PCIE_INTR_INTR_CLEAR, 0);
			QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_PCIE_INTR, OFFSET_PCIE_INTR_INTR_CLEAR, 1);
			QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_PCIE_INTR, OFFSET_PCIE_INTR_INTR_CLEAR, 0);
		}
	} while (uTriggerpoint != 1);
}

int QT_BoardSetSoftReset()
{
	int ret = 0;
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_PCIE_INTR, 0x18, 0);
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_PCIE_INTR, 0x18, 1);
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_PCIE_INTR, 0x18, 0);
	return ret;
}

int QT_BoardSetStdSingleDMAParameter(uint32_t once_trig_bytes, uint32_t DMATotolbytes)
{
	int ret = 0;
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_DMA_ADC, 0x14, 4 * 1024 * 1024);//DMA单次搬运的长度
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_TRIG_CTRL, 0x34, once_trig_bytes / 64 * 64);//设置单次触发长度
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_DMA_ADC, 0x18, DMATotolbytes / 64 * 64);//每一次触发的长度 如果是触发式采集应该注意触发频率和单次采集长度的大小关系 单次采集长度<触发频率理论采集长度
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_DMA_ADC, 0x1c, DMATotolbytes / 64 * 64);//xdma传输段长(byte) 一般为触发次数* 单次触发长度
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_DMA_ADC, 0x04, 0);//乒乓操作第一块地址
	return ret;
}

int QT_BoardSetStdMultiDMAParameter(uint32_t once_trig_bytes, uint32_t DMATotolbytes)
{
	int ret = 0;
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_DMA_ADC, 0x14, 4 * 1024 * 1024);//DMA单次搬运的长度
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_TRIG_CTRL, 0x34, once_trig_bytes / 64 * 64);//设置单次触发长度
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_DMA_ADC, 0x18, DMATotolbytes / 64 * 64);//每一次触发的长度 如果是触发式采集应该注意触发频率和单次采集长度的大小关系 单次采集长度<触发频率理论采集长度
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_DMA_ADC, 0x1c, DMATotolbytes / 64 * 64);//xdma传输段长(byte) 一般为触发次数* 单次触发长度
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_DMA_ADC, 0x04, 0);//乒乓操作第一块地址
	return ret;
}

int QT_BoardSetFifoSingleDMAParameter(uint32_t once_trig_bytes, uint32_t DMATotolbytes)
{
	int ret = 0;
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_DMA_ADC, 0x14, 4 * 1024 * 1024);//DMA单次搬运的长度
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_TRIG_CTRL, 0x34, once_trig_bytes / 64 * 64);//设置单次触发长度
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_DMA_ADC, 0x18, DMATotolbytes / 64 * 64);//每一次触发的长度 如果是触发式采集应该注意触发频率和单次采集长度的大小关系 单次采集长度<触发频率理论采集长度
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_DMA_ADC, 0x1c, DMATotolbytes / 64 * 64);//xdma传输段长(byte) 一般为触发次数* 单次触发长度
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_DMA_ADC, OFFSET_DMA_ADC_BASEADDR0_LOW, 0);
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_DMA_ADC, OFFSET_DMA_ADC_BASEADDR0_HIGH, 0);
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_DMA_ADC, OFFSET_DMA_ADC_BASEADDR1_LOW, 0x80000000);
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_DMA_ADC, OFFSET_DMA_ADC_BASEADDR1_HIGH, 0);
	return ret;
}

int QT_BoardSetFifoMultiDMAParameter(uint32_t once_trig_bytes, uint32_t DMATotolbytes)
{
	int ret = 0;
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_DMA_ADC, 0x14, 4 * 1024 * 1024);//DMA单次搬运的长度
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_TRIG_CTRL, 0x34, once_trig_bytes / 64 * 64);//设置单次触发长度
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_DMA_ADC, 0x18, DMATotolbytes / 64 * 64);//每一次触发的长度 如果是触发式采集应该注意触发频率和单次采集长度的大小关系 单次采集长度<触发频率理论采集长度
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_DMA_ADC, 0x1c, DMATotolbytes / 64 * 64);//xdma传输段长(byte) 一般为触发次数* 单次触发长度
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_DMA_ADC, OFFSET_DMA_ADC_BASEADDR0_LOW, 0);
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_DMA_ADC, OFFSET_DMA_ADC_BASEADDR0_HIGH, 0);
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_DMA_ADC, OFFSET_DMA_ADC_BASEADDR1_LOW, 0x0);
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_DMA_ADC, OFFSET_DMA_ADC_BASEADDR1_HIGH, 0x1);
	return ret;
}

int QT_BoardSoftTrigger()
{
	int ret = 0;
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_TRIG_CTRL, 0x00, 0);
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_TRIG_CTRL, 0x04, 1);
	return ret;
}

int QT_BoardInternalPulseTrigger(int counts, int pulse_period, int pulse_width)
{
	int ret = 0;
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_TRIG_CTRL, 0x00, 1);
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_TRIG_CTRL, 0x04, counts);
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_TRIG_CTRL, 0x08, pulse_period / 10);
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_TRIG_CTRL, 0x0C, pulse_width / 10);
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_TRIG_CTRL, 0x10, 1);
	return ret;
}

int QT_BoardExternalTrigger(int mode, int counts)
{
	int ret = 0;
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_TRIG_CTRL, 0x00, mode);
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_TRIG_CTRL, 0x04, counts);
	return ret;

}

int QT_BoardChannelTrigger(int mode, int counts, int channelID, int rasing_codevalue, int falling_codevalue)
{
	int ret = 0;
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_TRIG_CTRL, 0x00, mode);
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_TRIG_CTRL, 0x04, counts);
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_TRIG_CTRL, 0x24, channelID);
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_TRIG_CTRL, 0x14, rasing_codevalue);
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_TRIG_CTRL, 0x18, falling_codevalue);
	return ret;
}

int phase_inc(double bitWidth, double ddsInputClockMHz, double ddsOutputFreqMHz, uint32_t *ctrlWord)
{
	uint32_t *output = ctrlWord;
	//uint32_t *output2 = ctrlfreq;
	uint32_t PhaseInc = 0;
	uint32_t bitMask = 0;
	uint32_t Freq = 0;
	//返回小数对整数部分的四舍五入值,比如 round(3.623); 返回4
	PhaseInc = (uint32_t)round(pow(2.0, bitWidth) / ddsInputClockMHz * ddsOutputFreqMHz);

	for (size_t i = 0; i < bitWidth; i++)
	{
		bitMask |= (1 << i);
	}
	//取低bitWidth位的数据
	PhaseInc = PhaseInc & bitMask;
	//*output2 = Freq;
	*output = PhaseInc;
	return 0;
}

int phase_offset(double bitWidth, double ddsInputClockMHz, double ddsOutputFreqMHz, int ddsCount, uint32_t *ctrlWord)
{
	uint32_t PhaseOffset = 0;
	uint32_t bitMask = 0;
	uint32_t *output = ctrlWord;
	//返回小数对整数部分的四舍五入值,比如 round(3.623); 返回4
	PhaseOffset = (uint32_t)round(pow(2.0, bitWidth) / ddsInputClockMHz * ddsOutputFreqMHz / ddsCount);
	for (size_t i = 0; i < bitWidth; i++)
	{
		bitMask |= (1 << i);
	}
	//取低bitWidth位的数据
	PhaseOffset = PhaseOffset & bitMask;
	*output = PhaseOffset;
	return 0;
}

int QT_BoardSetDacPhaseAndFreq(uint64_t freq, uint64_t phase_inc)
{
	int ret = 0;
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_DAC_CTRL, 0x14, freq);
	ret = QTXdmaApiInterface::Func_QTXdmaReadRegister(&pstCardInfo, BASE_DAC_CTRL, 0x14, &freq);
	printf("%lld\n", freq);

	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_DAC_CTRL, 0x18, phase_inc);
	ret = QTXdmaApiInterface::Func_QTXdmaReadRegister(&pstCardInfo, BASE_DAC_CTRL, 0x18, &phase_inc);
	printf("%lld\n", phase_inc);
	return ret;
}


int QT_BoardUpdateTheOutputFrequency()
{
	int ret = 0;
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_DAC_CTRL, 0x1C, 0);
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_DAC_CTRL, 0x1C, 1);
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_DAC_CTRL, 0x1C, 0);
	return ret;
}
int QT_BoardSetDaPlayStart(bool enable)
{
	int ret = 0;
	if (enable)
	{
		QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_PCIE_INTR, 0x0C, 0);
		QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_PCIE_INTR, 0x0C, 1);
		QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_PCIE_INTR, 0x0C, 0);
	}
	else
	{
		//DDS播放置为0
		QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_DAC_CTRL, 0x14, 0);
		QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_DAC_CTRL, 0x18, 1);

		QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_DAC_CTRL, 0x1c, 0);
		QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_DAC_CTRL, 0x1c, 1);
		QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_DAC_CTRL, 0x1c, 0);

		//上位机下发停止播放指令
		QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_PCIE_INTR, 0x14, 0x0);
		QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_PCIE_INTR, 0x14, 0x1);
		QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_PCIE_INTR, 0x14, 0x0);

	}
	return ret;
}

int QT_BoardSetDacPlaySourceSelect(uint32_t sourceId)
{
	int ret = 0;
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_DAC_CTRL, 0x20, sourceId);
	return ret;
}

int QT_BoardSetDacPlayChannelSelect(uint32_t channelId)
{
	int ret = 0;
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_DAC_CTRL, 0x10, channelId);
	return ret;
}

int QT_BoardSetDACLoopRead(int enable, uint32_t count)
{
	int ret = 0;
	int eenable = enable ? 1 : 0;
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_DMA_DAC, 0x00, 0);
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_DMA_DAC, 0x1C, eenable);
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_DMA_DAC, 0x20, count);
	return ret;
}

int QT_BoardSetDacTriggerMode(uint32_t triggermode)
{
	int ret = 0;
	QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_DAC_CTRL2, 0x00, triggermode);
	return ret;
}

int QT_BoardSetDacRegister(uint32_t uWriteStartAddr, uint32_t uPlaySize)
{
	int ret = 0;
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_DMA_DAC, 0x00, 0);
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_DMA_DAC, 0x04, uWriteStartAddr);
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_DMA_DAC, 0x14, 4 * 1024 * 1024);
	ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_DMA_DAC, 0x18, uPlaySize);
	return 0;
}

int QT_BoardSetOffset(int channel_id, int offset_value)
{
	int ret = 0;
	//将电压值转换成偏置寄存器下发数值
	uint32_t single_offset_reg = 0;
	//根据界面设置去获取偏置并下发
	if (channel_id == 1)
	{
		single_offset_reg = (uint32_t)((double)((double)2.5 - offset_value) * 8192 / (double)2.5 - 169);
		ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_PCIE_INTR, 0x20, single_offset_reg);
		ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_PCIE_INTR, 0x30, 0);
		ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_PCIE_INTR, 0x30, 1);
		ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_PCIE_INTR, 0x30, 0);
	}
	else if (channel_id == 2)
	{
		single_offset_reg = (uint32_t)((double)((double)2.5 - offset_value) * 8192 / (double)2.5 - 175);
		ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_PCIE_INTR, 0x24, single_offset_reg);
		ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_PCIE_INTR, 0x34, 0);
		ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_PCIE_INTR, 0x34, 1);
		ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_PCIE_INTR, 0x34, 0);
	}
	else if (channel_id == 3)
	{
		single_offset_reg = (uint32_t)((double)((double)2.5 - offset_value) * 8192 / (double)2.5 - 157);
		ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_PCIE_INTR, 0x28, single_offset_reg);
		ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_PCIE_INTR, 0x38, 0);
		ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_PCIE_INTR, 0x38, 1);
		ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_PCIE_INTR, 0x38, 0);
	}
	else if (channel_id == 4)
	{
		single_offset_reg = (uint32_t)((double)((double)2.5 - offset_value) * 8192 / (double)2.5 - 195);
		ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_PCIE_INTR, 0x2c, single_offset_reg);
		ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_PCIE_INTR, 0x3c, 0);
		ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_PCIE_INTR, 0x3c, 1);
		ret = QTXdmaApiInterface::Func_QTXdmaWriteRegister(&pstCardInfo, BASE_PCIE_INTR, 0x3c, 0);
	}
	else
	{
		puts("Wrong channel id!");
	}
	return ret;
}