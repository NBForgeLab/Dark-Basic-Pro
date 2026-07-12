#pragma once

#include <windows.h>
#include "DB3.h"
#include <mutex>
#include <condition_variable>
#include <thread>
#include <iostream>

#if _MSC_VER
# include <intrin.h>

# pragma intrinsic(_InterlockedIncrement)
# pragma intrinsic(_InterlockedDecrement)
# pragma intrinsic(_InterlockedExchange)
# pragma intrinsic(_InterlockedCompareExchange)
# pragma intrinsic(_InterlockedAnd)
# pragma intrinsic(_InterlockedOr)
# pragma intrinsic(_InterlockedXor)

# if _WIN64
#  pragma intrinsic(_InterlockedIncrement64)
#  pragma intrinsic(_InterlockedDecrement64)
#  pragma intrinsic(_InterlockedExchange64)
#  pragma intrinsic(_InterlockedCompareExchange64)
#  pragma intrinsic(_InterlockedAnd64)
#  pragma intrinsic(_InterlockedOr64)
#  pragma intrinsic(_InterlockedXor64)
# else
#  define _InterlockedIncrement64 InterlockedIncrement64
#  define _InterlockedDecrement64 InterlockedDecrement64
#  define _InterlockedExchange64 InterlockedExchange64
#  define _InterlockedCompareExchange64 InterlockedCompareExchange64
#  define _InterlockedAnd64 InterlockedAnd64
#  define _InterlockedOr64 InterlockedOr64
#  define _InterlockedXor64 InterlockedXor64
# endif

# if 0
#  pragma intrinsic(_InterlockedExchangePointer)
#  pragma intrinsic(_InterlockedCompareExchangePointer)
# else
#  define _InterlockedExchangePointer InterlockedExchangePointer
#  define _InterlockedCompareExchangePointer InterlockedCompareExchangePointer
# endif
#endif

DB_ENTER_NS()

//------------------------------------------------------------------------------
// 32-bit

__forceinline u32 atomic_inc(volatile u32 *x) {
#if _MSC_VER
	return _InterlockedIncrement((volatile LONG *)x);
#else
	return __sync_fetch_and_add(x, 1);
#endif
}
__forceinline u32 atomic_dec(volatile u32 *x) {
#if _MSC_VER
	return _InterlockedDecrement((volatile LONG *)x);
#else
	return __sync_fetch_and_sub(x, 1);
#endif
}
__forceinline u32 atomic_set(volatile u32 *x, u32 y) {
#if _MSC_VER
	return _InterlockedExchange((volatile LONG *)x, y);
#else
	return __sync_lock_test_and_set(x, y);
#endif
}
__forceinline u32 atomic_set_eq(volatile u32 *dst, u32 src, u32 cmp) {
#if _MSC_VER
	return _InterlockedCompareExchange((volatile LONG *)dst, src, cmp);
#else
	return __sync_val_compare_and_swap(dst, cmp, src);
#endif
}
__forceinline u32 atomic_and(volatile u32 *x, u32 y) {
#if _MSC_VER
	return _InterlockedAnd((volatile LONG *)x, y);
#else
	return __sync_fetch_and_and(x, y);
#endif
}
__forceinline u32 atomic_or(volatile u32 *x, u32 y) {
#if _MSC_VER
	return _InterlockedOr((volatile LONG *)x, y);
#else
	return __sync_fetch_and_or(x, y);
#endif
}
__forceinline u32 atomic_xor(volatile u32 *x, u32 y) {
#if _MSC_VER
	return _InterlockedXor((volatile LONG *)x, y);
#else
	return __sync_fetch_and_xor(x, y);
#endif
}

__forceinline u32 atomic_get(volatile u32 *x) {
	return atomic_or(x, 0);
}

//------------------------------------------------------------------------------
// 64-bit

__forceinline u64 atomic_inc(volatile u64 *x) {
#if _MSC_VER
	return _InterlockedIncrement64((volatile LONGLONG *)x);
#else
	return __sync_fetch_and_add(x, 1);
#endif
}
__forceinline u64 atomic_dec(volatile u64 *x) {
#if _MSC_VER
	return _InterlockedDecrement64((volatile LONGLONG *)x);
#else
	return __sync_fetch_and_sub(x, 1);
#endif
}
__forceinline u64 atomic_set(volatile u64 *x, u64 y) {
#if _MSC_VER
	return _InterlockedExchange64((volatile LONGLONG *)x, y);
#else
	return __sync_lock_test_and_set(x, y);
#endif
}
__forceinline u64 atomic_set_eq(volatile u64 *dst, u64 src, u64 cmp) {
#if _MSC_VER
	return _InterlockedCompareExchange64((volatile LONGLONG *)dst, src, cmp);
#else
	return __sync_val_compare_and_swap(dst, cmp, src);
#endif
}
__forceinline u64 atomic_and(volatile u64 *x, u64 y) {
#if _MSC_VER
	return _InterlockedAnd64((volatile LONGLONG *)x, y);
#else
	return __sync_fetch_and_and(x, y);
#endif
}
__forceinline u64 atomic_or(volatile u64 *x, u64 y) {
#if _MSC_VER
	return _InterlockedOr64((volatile LONGLONG *)x, y);
#else
	return __sync_fetch_and_or(x, y);
#endif
}
__forceinline u64 atomic_xor(volatile u64 *x, u64 y) {
#if _MSC_VER
	return _InterlockedXor64((volatile LONGLONG *)x, y);
#else
	return __sync_fetch_and_xor(x, y);
#endif
}

__forceinline u64 atomic_get(volatile u64 *x) {
	return atomic_or(x, 0);
}

//------------------------------------------------------------------------------
// pointer

template<typename T>
__forceinline T *atomic_set(T *volatile *x, T *y) {
#if _MSC_VER
	return _InterlockedExchangePointer((void *volatile *)x, (void *)y);
#else
	return __sync_lock_test_and_set(x, y);
#endif
}
template<typename T>
__forceinline T *atomic_set_eq(T *volatile *dst, T *src, T *cmp) {
#if _MSC_VER
	return _InterlockedCompareExchangePointer((void *volatile *)dst,
		(void *)src, (void *)cmp);
#else
	return __sync_val_compare_and_swap(dst, cmp, src);
#endif
}

template<typename T>
__forceinline T *atomic_get(T *volatile *x) {
#if AXTEK_ARCH_BITS==64
	return (T *)atomic_get((volatile u64 *)x);
#elif AXTEK_ARCH_BITS==32
	return (T *)atomic_get((volatile u32 *)x);
#else
	if (sizeof(T *)==8)
		return (T *)atomic_get((volatile u64 *)x);

	return (T *)atomic_get((volatile u32 *)x);
#endif
}

class CLock {
protected:
	std::recursive_mutex m_mutex;

public:
	inline CLock() {}
	inline ~CLock() {}

	inline void Lock() {
		m_mutex.lock();
	}
	inline bool TryLock() {
		return m_mutex.try_lock();
	}
	inline void Unlock() {
		m_mutex.unlock();
	}
};

class CAutolock {
protected:
	CLock &m_lock;

public:
	inline CAutolock(CLock &l): m_lock(l) {
		m_lock.Lock();
	}
	inline ~CAutolock() {
		m_lock.Unlock();
	}
};

class CEvent {
protected:
	std::mutex m_mutex;
	std::condition_variable m_cv;
	bool m_signaled;

public:
	inline CEvent() : m_signaled(false) {}
	inline ~CEvent() {}

	inline void Signal() {
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_signaled = true;
		}
		m_cv.notify_all();
	}
	inline void Reset() {
		std::lock_guard<std::mutex> lock(m_mutex);
		m_signaled = false;
	}

	inline void Sync() {
		std::unique_lock<std::mutex> lock(m_mutex);
		m_cv.wait(lock, [this]() { return m_signaled; });
	}
	inline bool Wait(u32 milliseconds) {
		std::unique_lock<std::mutex> lock(m_mutex);
		return m_cv.wait_for(lock, std::chrono::milliseconds(milliseconds), [this]() { return m_signaled; });
	}
	inline bool TryWait() {
		std::lock_guard<std::mutex> lock(m_mutex);
		return m_signaled;
	}
};

class CSignal {
protected:
	uint m_cur, m_exp;
	CEvent m_event;

public:
	inline CSignal(uint exp=0): m_cur(0), m_exp(exp), m_event() {
	}
	inline ~CSignal() {
	}

	inline void Raise() {
		atomic_inc(&m_cur);
		if (atomic_get(&m_cur) >= atomic_get(&m_exp))
			m_event.Signal();
	}
	inline void Lower() {
		atomic_dec(&m_cur);
		if (atomic_get(&m_cur) < atomic_get(&m_exp))
			m_event.Reset();
	}

	inline void Clear() {
		atomic_set(&m_cur, 0);
		m_event.Reset();
	}
	inline void Reset(uint exp) {
		atomic_set(&m_exp, exp);
		atomic_set(&m_cur, 0);
		m_event.Reset();
	}
	inline void IncreaseTarget()
	{
		atomic_inc(&m_exp);
		if (atomic_get(&m_cur) < atomic_get(&m_exp))
			m_event.Reset();
	}
	inline void Sync() {
		m_event.Sync();
	}
	inline bool Wait(uint milliseconds) {
		return m_event.Wait(milliseconds);
	}
	inline bool TryWait() {
		return m_event.TryWait();
	}
};

inline void yield() {
#if _WIN32
	Sleep(0);
#else
	sched_yield(); //use nanosleep() instead?
#endif
}

#define THREADPROCAPI __stdcall
typedef DWORD result_t;

class CWorkQueue
{
public:
	enum
	{
		kMinimumThreads = 1,
		kInitialReserve = 128,
		kMaximumStreams = 4096
	};
	typedef void(*work_function_type)(void *parm);

	struct SWork
	{
		work_function_type Func;
		void *Parm;

		CSignal *Sync;
		SWork *Prev, *Next;
	};

protected:
	template<class T> struct remove_const          { typedef T type; };
	template<class T> struct remove_const<const T> { typedef T type; };

	std::condition_variable m_cv;
	bool m_terminate;

	std::mutex m_lock;

	SWork *m_liveWork;
	SWork *m_deadWork;

	std::thread m_threads[32];
	uint m_numThreads;

	inline void AddWork(SWork *&queue, SWork *item)
	{
		if (queue!=nullptr)
		{
			item->Next = queue;
			item->Prev = queue->Prev;
			queue->Prev->Next = item;
			queue->Prev = item;
		}
		else
		{
			queue = item;
			queue->Prev = item;
			queue->Next = item;
		}
	}
	inline SWork *RemoveWork(SWork *&queue)
	{
		SWork *item;

		if (queue==nullptr)
			return nullptr;

		item = queue;

		if (queue==queue->Next)
		{
			queue = nullptr;
		}
		else
		{
			queue->Next->Prev = queue->Prev;
			queue->Prev->Next = queue->Next;
			queue = queue->Next;
		}

		return item;
	}
	inline void ReserveDeadWork(uint count)
	{
		uint i;

		for(i=0; i<count; i++)
			AddWork(m_deadWork, new SWork);
	}

	static inline void ThreadFunc_f(CWorkQueue* q)
	{
		SWork *w;

		while(true)
		{
			{
				std::unique_lock<std::mutex> lock(q->m_lock);
				q->m_cv.wait(lock, [q]() { return q->m_terminate || q->m_liveWork != nullptr; });

				if (q->m_terminate && q->m_liveWork == nullptr)
				{
					break;
				}

				w = q->RemoveWork(q->m_liveWork);
			}

			if (w) {
				w->Prev = nullptr;
				w->Next = nullptr;

				if (w->Func) {
					w->Func(w->Parm);
				}

				if (w->Sync)
					w->Sync->Raise();

				{
					std::lock_guard<std::mutex> lock(q->m_lock);
					q->AddWork(q->m_deadWork, w);
				}
			}
		}
	}

public:
	inline CWorkQueue()
	: m_terminate(false), m_liveWork(nullptr), m_deadWork(nullptr), m_numThreads(0)
	{
	}
	inline ~CWorkQueue()
	{
		Fini();
	}

	inline bool Init(uint numThreads=0)
	{
		uint i;

		m_liveWork = nullptr;
		m_deadWork = nullptr;

		if (!numThreads)
		{
			numThreads = std::thread::hardware_concurrency();
			if (numThreads == 0) numThreads = 4;
			numThreads += numThreads/2; //optimal number of threads is about ~1.5 times the number of logical cores
		}

		if (numThreads > 32)
			numThreads = 32;
		else if(numThreads < kMinimumThreads)
			numThreads = kMinimumThreads;

		m_terminate = false;
		m_numThreads = numThreads;

		for(i=0; i<numThreads; i++)
		{
			m_threads[i] = std::thread(ThreadFunc_f, this);
		}

		ReserveDeadWork(kInitialReserve);
		return true;
	}
	inline void Fini()
	{
		uint i;

		{
			std::lock_guard<std::mutex> lock(m_lock);
			m_terminate = true;
		}
		m_cv.notify_all();

		for(i=0; i<m_numThreads; i++)
		{
			if (m_threads[i].joinable()) {
				m_threads[i].join();
			}
		}
		m_numThreads = 0;

		std::lock_guard<std::mutex> lock(m_lock);
		SWork *const *heads[] = { &m_liveWork, &m_deadWork };
		uint headIdx;

		for(headIdx=0; headIdx<sizeof(heads)/sizeof(heads[0]); headIdx++)
		{
			SWork *item, *next;

			item = *heads[headIdx];
			while(item != nullptr)
			{
				next = item->Next;
				delete item;
				item = next!=*heads[headIdx] ? next : nullptr;
			}
		}
		m_liveWork = nullptr;
		m_deadWork = nullptr;
	}

	template<typename T>
	inline bool Enqueue(void(*func)(T *), T *parm=nullptr, CSignal *signal=nullptr)
	{
		SWork *item;
		bool r = false;

		if (m_terminate)
			return false;

		{
			std::lock_guard<std::mutex> lock(m_lock);
			if (m_deadWork)
				item = RemoveWork(m_deadWork);
			else
				item = new SWork;

			if (item != nullptr)
			{
				item->Func = reinterpret_cast<work_function_type>(func);
				item->Parm = (void *)parm;
				item->Sync = signal;
				if (signal)
					signal->IncreaseTarget();
				AddWork(m_liveWork, item);

				r = true;
			}
		}

		if (!r)
			return false;

		m_cv.notify_one();
		return true;
	}
	inline void Reserve(uint count)
	{
		std::lock_guard<std::mutex> lock(m_lock);
		ReserveDeadWork(count);
	}

	inline uint GetThreadCount() const
	{
		return m_numThreads;
	}
};

DB_LEAVE_NS()
