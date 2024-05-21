
#ifndef _MT_MUTEX_H_
#define _MT_MUTEX_H_

namespace mt {

	/**
	 * 互斥体
	 *
	 * 以递归方式锁定代码段，也就是说在同一个线程中可以调用Lock进行多次加锁，
	 * 而不会引起死锁。
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
