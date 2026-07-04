# Phase 14: Job System & Task-based Concurrency

## 🎯 Goal
Modernize multi-threading and concurrency. This phase replaces obsolete, raw Windows API threads (`CreateThread`) and manual sync primitives with a task-based job system utilizing standard C++ constructs. This allows games to scale across modern multi-core processors safely and efficiently.

---

## ⚠️ Current Issues
The engine was architected in the single-core CPU era:
* **Problem**:
  * Sound decoding and background loading spawn raw OS threads using `CreateThread` on-the-fly.
  * Thread synchronization uses legacy Win32 structures (`CRITICAL_SECTION` or `CreateMutex`) which are non-portable and error-prone.
  * Creating and destroying threads dynamically is expensive, wasting CPU cycles and leading to race conditions or deadlocks.

---

## 🛠️ Design: Thread Pool-based Job System

We propose a task scheduler that manages a fixed pool of threads matching the system's hardware concurrency:

### Shared Thread Pool (`JobSystem.h`)
```cpp
#pragma once
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>

class JobSystem {
public:
    static void Initialize() {
        size_t numThreads = std::thread::hardware_concurrency();
        if (numThreads == 0) numThreads = 4;
        
        for (size_t i = 0; i < numThreads; ++i) {
            m_workers.emplace_back([]() {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(m_queueMutex);
                        m_condition.wait(lock, [] { return m_stop || !m_tasks.empty(); });
                        
                        if (m_stop && m_tasks.empty()) return;
                        
                        task = std::move(m_tasks.front());
                        m_tasks.pop();
                    }
                    task();
                }
            });
        }
    }

    template<class F, class... Args>
    static auto Schedule(F&& f, Args&&... args) 
        -> std::future<typename std::invoke_result<F, Args...>::type> 
    {
        using return_type = typename std::invoke_result<F, Args...>::type;

        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        
        std::future<return_type> res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            if (m_stop) throw std::runtime_error("JobSystem is stopped.");
            m_tasks.emplace([task]() { (*task)(); });
        }
        m_condition.notify_one();
        return res;
    }

    static void Shutdown() {
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_stop = true;
        }
        m_condition.notify_all();
        for (std::thread &worker : m_workers) {
            if (worker.joinable()) worker.join();
        }
        m_workers.clear();
    }

private:
    static std::vector<std::thread> m_workers;
    static std::queue<std::function<void()>> m_tasks;
    static std::mutex m_queueMutex;
    static std::condition_variable m_condition;
    static bool m_stop;
};
```

---

## 🔄 Implementation Steps

1. **Add JobSystem to Engine Core**:
   * Implement and initialize `JobSystem` in `Core.dll` at startup.
2. **Replace `CreateThread`**:
   * Refactor background tasks (e.g., sound decompression, file loading) to use the job system:
     ```cpp
     JobSystem::Schedule([]() {
         // Background task logic
     });
     ```
3. **Use Standard Mutexes**:
   * Replace Win32 critical sections with `std::mutex` and RAII-based locks (`std::lock_guard`).

---

## 🚀 Benefits
* **High Efficiency**: Reusing thread pools eliminates thread creation overhead and leverages modern CPU cores.
* **Safer Mutex Management**: RAII locking automatically unlocks resources on exceptions, avoiding deadlocks.
* **Portable Codebase**: Standardizes threading, enabling future cross-platform builds.
