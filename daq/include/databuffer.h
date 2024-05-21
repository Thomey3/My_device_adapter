#pragma once

#include <stdint.h>

class databuffer
{
public:
	databuffer();
	~databuffer();
public:
    int m_iBufferID;//缓存ID
	uint8_t * m_bufferAddr;//缓存地址
	bool m_bAvailable;//缓存是否可用
	bool m_bAllocateMem;//是否已分配空间
    int m_iBufferSize;//申请的空间大小
    int m_iBufferIndex;//缓存索引
	int m_iTotalSize;//内存总空间
};

