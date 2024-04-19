#ifndef CONFIGDATA_H
#define CONFIGDATA_H

#include <string>
using namespace std;

class configData
{
public:
    configData();
public:
    static int signal_tag;//标记信号点
    static int unsinged_data;//是否是无符号数据
    static int file_alldata;//是否显示文件所有数据
    static int show_dotcount;//显示的数据点数

    static double time_domian_amplitude_up;
    static double time_domian_amplitude_down;
    static double freq_domian_amplitude_up;
    static double freq_domian_amplitude_down;

    static string server_ip_address;
    static string broadcast_ip_address;

    static int VALUE_UPCONV_ATTEN;//上变频衰减值
    static int VALUE_DOWNCONV_RF_FREQ_ATTEN;//下变频射频衰减值
    static int VALUE_DOWNCONV_IF_ATTEN;//下变频中频衰减值
    static int VALUE_SWEEP_START_FREQPOINT;//扫频起始频率点
    static int VALUE_SWEEP_END_FREQPOINT;//扫频终止频率点
    static int VALUE_SWEEP_MODE;//扫频模式
    static int VALUE_SWEEP_FREQ_KEEP_PERIOD;//频率保持时间

    static double full_vpp;//量程电压
    static double source_freq;//采集频率
    static int fft_count;//参与运算的傅里叶变换点数

    static int voltage_show;//时域是否按电压值显示

    static int pcie_ppsperiod;//秒脉冲周期
    static int pcie_trigger_count;//触发次数
    static int pcie_trigger_channel;//触发通道

    static int pcie_clocksrc;//时钟源
    static int pcie_samplefreq;//采样频率
    static int pcie_period;//采集时长
    static int pcie_trigger;//触发模式
    static double pcie_risingedge_trigger_electric_potential;//上升沿触发电平
    static double pcie_fallingedge_trigger_electric_potential;//下降沿触发电平
    static int pcie_fpga_dmadata_transfer_size;//fpga单次传输数据大小
    static int pcie_savefile;//是否保存文件
    static string pcie_filepath;//文件保存路径

    static double axis_timedomain_voltage_min;
    static double axis_timedomain_voltage_max;
    static double axis_timedomain_code_min;
    static double axis_timedomain_code_max;

    static int trigger_1_output_delay_time;//触发1输出延迟时间(5ns)
    static int trigger_1_output_pulse_width;//触发1输出脉冲宽度(5ns)
    static int trigger_2_output_delay_time;//触发2输出延迟时间(5ns)
    static int trigger_2_output_pulse_width;//触发2输出脉冲宽度(5ns)
    static int trigger_3_output_delay_time;//触发3输出延迟时间(5ns)
    static int trigger_3_output_pulse_width;//触发3输出延脉冲宽度(5ns)
    static int trigger_4_output_delay_time;//触发4输出延迟时间(5ns)
    static int trigger_4_output_pulse_width;//触发4输出延脉冲宽度(5ns)

    static int accum_times;//累加次数
    static int accum_length;//累加长度

    static int trigger_enable;//是否启用触发
    static int trigger_enable2;//是否启用触发
    static int interface_type;//接口类型

    static int qt1142V3_offset_ch1;
    static int qt1142V3_offset_ch2;
    static int qt1142V3_offset_ch3;
    static int qt1142V3_offset_ch4;

    static string qt1142V3_label;//1142V3序列号
    static string customer_info;//客户信息

    //static uint32_t ETHERNET_DDR_ADDR;
};

#endif // CONFIGDATA_H
