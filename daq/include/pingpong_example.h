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
	//��������: ��ȡ�忨��Ϣ���忨�ͺš�����汾�������ʡ�ͨ�������ֱ��ʣ�
	//����������
	//��������: 
	void QT_BoardGetCardInfo();

	//��������: ����ж�
	//����������
	//��������: �ɹ�����0,ʧ�ܷ���-1������ϸ������Ϣд����־�ļ�
	int QT_BoardSetInterruptClear();

	//��������: ��λ��ֹͣ�ɼ�ADC����
	//����������
	//��������: �ɹ�����0,ʧ�ܷ���-1������ϸ������Ϣд����־�ļ�
	int QT_BoardSetADCStop();

	//��������: ��λ�������λ
	//����������
	//��������: �ɹ�����0,ʧ�ܷ���-1������ϸ������Ϣд����־�ļ�
	int QT_BoardSetSoftReset();

	//��������: ����Ԥ����
	//����������pre_trig_length��Ԥ��������
	//��������: �ɹ�����0,ʧ�ܷ���-1������ϸ������Ϣд����־�ļ�
	int QT_BoardSetPerTrigger(uint32_t pre_trig_length);

	//��������: ����֡ͷ�Ƿ�ʹ��
	//����������frameheaderenable��0:����  1:ʹ��
	//��������: �ɹ�����0,ʧ�ܷ���-1������ϸ������Ϣд����־�ļ�
	int QT_BoardSetFrameheader(uint32_t frameheaderenable);

	//��������: ����ʱ��ģʽ
	//����������clockmode��ʱ��ģʽ 0:�ڲο�ʱ�� 1:�����ʱ�� 2:��ο�ʱ��
	//��������: �ɹ�����0,ʧ�ܷ���-1������ϸ������Ϣд����־�ļ�
	int QT_BoardSetClockMode(uint32_t clockmode);

	//��������: ����StdSingle DMA�������
	//����������once_trig_bytes�����δ���������   DMATotolbytes���ж�������
	//��������: �ɹ�����0,ʧ�ܷ���-1������ϸ������Ϣд����־�ļ�
	int QT_BoardSetStdSingleDMAParameter(uint32_t once_trig_bytes, uint32_t DMATotolbytes);

	//��������: ����StdMulti DMA�������
	//����������once_trig_bytes�����δ���������   DMATotolbytes���ж�������
	//��������: �ɹ�����0,ʧ�ܷ���-1������ϸ������Ϣд����־�ļ�
	int QT_BoardSetStdMultiDMAParameter(uint32_t once_trig_bytes, uint32_t DMATotolbytes);

	//��������: ����FifoSingle DMA�������
	//����������once_trig_bytes�����δ���������   DMATotolbytes���ж�������
	//��������: �ɹ�����0,ʧ�ܷ���-1������ϸ������Ϣд����־�ļ�
	int QT_BoardSetFifoSingleDMAParameter(uint32_t once_trig_bytes, uint32_t DMATotolbytes);

	//��������: ����FifoMulti DMA�������
	//����������once_trig_bytes�����δ���������   DMATotolbytes���ж�������
	//��������: �ɹ�����0,ʧ�ܷ���-1������ϸ������Ϣд����־�ļ�
	int QT_BoardSetFifoMultiDMAParameter(uint32_t once_trig_bytes, uint32_t DMATotolbytes);

	//��������: ���ô���ģʽΪ���޴δ���ģʽ
	//����������transmitpoints��0Ϊ���޵� 1Ϊ���޵�   transmittimes��DMA0������� 0���޴� 1���޴�
	//��������: �ɹ�����0,ʧ�ܷ���-1������ϸ������Ϣд����־�ļ�
	int QT_BoardSetTransmitMode(int transmitpoints, int transmittimes);

	//��������: �������
	//����������
	//��������: �ɹ�����0,ʧ�ܷ���-1������ϸ������Ϣд����־�ļ�
	int QT_BoardSoftTrigger();

	//��������: �ڲ����崥��
	//����������counts����������  pulse_period���ڲ����崥������  pulse_width���ڲ����崥�����
	//��������: �ɹ�����0,ʧ�ܷ���-1������ϸ������Ϣд����־�ļ�
	int QT_BoardInternalPulseTrigger(int counts, int pulse_period, int pulse_width);

	//��������: �ⲿ���崥��
	//����������mode������ģʽ  counts����������
	//��������: �ɹ�����0,ʧ�ܷ���-1������ϸ������Ϣд����־�ļ�
	int QT_BoardExternalTrigger(int mode, int counts);

	//��������: ͨ������
	//����������mode ����ͨ����������ģʽ  counts����������  channelID ����ͨ����������ͨ��ID��  rasing_codevalue ����ͨ�������ش�����ƽ  falling_codevalue ����ͨ���½��ش�����ƽ
	//��������: �ɹ�����0,ʧ�ܷ���-1������ϸ������Ϣд����־�ļ�
	int QT_BoardChannelTrigger(int mode, int counts, int channelID, int rasing_codevalue, int falling_codevalue);

	//��������: PCIe�ж�ʹ��
	//����������
	//��������: �ɹ�����0,ʧ�ܷ���-1������ϸ������Ϣд����־�ļ�
	int QT_BoardSetInterruptSwitch();

	//��������: ��λ����ʼ�ɼ�ADC����
	//����������
	//��������: �ɹ�����0,ʧ�ܷ���-1������ϸ������Ϣд����־�ļ�
	int QT_BoardSetADCStart();

	//��������: �ж�ģʽ
	//����������
	//��������: �ɹ�����0,ʧ�ܷ���-1������ϸ������Ϣд����־�ļ�
	void QT_BoardInterruptGatherType();

	//��������: ��ѯģʽ
	//����������
	//��������: �ɹ�����0,ʧ�ܷ���-1������ϸ������Ϣд����־�ļ�
	void QT_BoardPollingGatherType();

	//��������: ����Ƶ�ʿ�����
	//����������bitWidth��bitλ��  ddsInputClockMHz��DDSʱ��Ƶ��  ddsOutputFreqMHz��DDS���Ƶ��  ctrlWord��Ƶ�ʿ�����
	//��������: �ɹ�����0,ʧ�ܷ���-1������ϸ������Ϣд����־�ļ�
	int phase_inc(double bitWidth, double ddsInputClockMHz, double ddsOutputFreqMHz, uint32_t *ctrlWord);

	//��������: ������λ������
	//����������bitWidth��bitλ��  ddsInputClockMHz��DDSʱ��Ƶ��  ddsOutputFreqMHz��DDS���Ƶ��  ddsCount��DDS����  ctrlWord��Ƶ�ʿ�����
	//��������: �ɹ�����0,ʧ�ܷ���-1������ϸ������Ϣд����־�ļ�
	int phase_offset(double bitWidth, double ddsInputClockMHz, double ddsOutputFreqMHz, int ddsCount, uint32_t *ctrlWord);

	//��������: �·�������
	//����������freq��Ƶ�ʿ�����   phase_inc����λ������
	//��������: �ɹ�����0,ʧ�ܷ���-1������ϸ������Ϣд����־�ļ�
	int QT_BoardSetDacPhaseAndFreq(uint64_t freq, uint64_t phase_inc);

	//��������: ����DDS���Ƶ��
	//����������
	//��������: �ɹ�����0,ʧ�ܷ���-1������ϸ������Ϣд����־�ļ�
	int QT_BoardUpdateTheOutputFrequency();

	//��������: DAC����Դ
	//����������sourceId��0:DDS   1:�ļ�
	//��������: �ɹ�����0,ʧ�ܷ���-1������ϸ������Ϣд����־�ļ�
	int QT_BoardSetDacPlaySourceSelect(uint32_t sourceId);

	//��������: ��λ����ʼ/ֹͣ����DAC����
	//����������enable����ʼ/ֹͣ
	//��������: �ɹ�����0,ʧ�ܷ���-1������ϸ������Ϣд����־�ļ�
	int QT_BoardSetDaPlayStart(bool enable);

	//��������: DAC����ͨ��ѡ��
	//����������channelId��ͨ��ID
	//��������: �ɹ�����0,ʧ�ܷ���-1������ϸ������Ϣд����־�ļ�
	int QT_BoardSetDacPlayChannelSelect(uint32_t channelId);

	//��������: DACѭ����ȡ  ������010    ���Σ�000
	//����������enable ѭ����ȡʹ�� ֻ�����޴δ���ģʽ��������,1=enable, 0=disable  count ѭ����ȡ����(0=���޴�ѭ����ֱ��ѭ��ʹ����ֹ)
	//��������: �ɹ�����0,ʧ�ܷ���-1������ϸ������Ϣд����־�ļ�
	int QT_BoardSetDACLoopRead(int enable, uint32_t count);

	//��������: DAC����ģʽ
	//����������enable�� count
	//��������: �ɹ�����0,ʧ�ܷ���-1������ϸ������Ϣд����־�ļ�
	int QT_BoardSetDacTriggerMode(uint32_t triggermode);

	//��������: DAC DMA��������
	//����������uWriteStartAddr��DDR��ַ  uPlaySize���·�������
	//��������: �ɹ�����0,ʧ�ܷ���-1������ϸ������Ϣд����־�ļ�
	int QT_BoardSetDacRegister(uint32_t uWriteStartAddr, uint32_t uPlaySize);

	//��������: ƫ�õ���
	//����������channel_id��ͨ��ID    offset_value��ƫ��ֵ
	//��������: �ɹ�����0,ʧ�ܷ���-1������ϸ������Ϣд����־�ļ�
	int QT_BoardSetOffset(int channel_id, int offset_value);

#ifdef __cplusplus
}
#endif
