
#ifndef _MT_MUTEX_H_
#define _MT_MUTEX_H_

namespace mt {

	/**
	 * ������
	 *
	 * �Եݹ鷽ʽ��������Σ�Ҳ����˵��ͬһ���߳��п��Ե���Lock���ж�μ�����
	 * ����������������
	 */
	class Mutex  
	{
	public:
		Mutex();
		virtual ~Mutex();

	private:
		void operator = (const Mutex& e);
		struct MutexData;
		MutexData * m_mutexData;

	public:
		/**
		 * Locks an existing mutex
		 */
		void Lock();

		/**
		 * Unlocks a mutex owned by the thread.
		 */
		void Unlock();
	};

}

#endif // _MT_MUTEX_H_
