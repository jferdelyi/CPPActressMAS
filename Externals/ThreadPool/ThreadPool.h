#pragma once
#ifndef THREADPOOL_H_
#define THREADPOOL_H_

#include <vector>
#include <queue>
#include <thread>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>

class ThreadPool
{
public:
	explicit ThreadPool(size_t = 0);
	template <typename F, typename... Args>
	decltype(auto) addWorkFunc(F&& f, Args&&... args);
	~ThreadPool();
	void add_thread(size_t threads);
	[[nodiscard]] size_t get_number_of_threads() const  { return m_number_of_threads; }

	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;
	ThreadPool(ThreadPool&&) = delete;
	ThreadPool& operator=(ThreadPool&&) = delete;

private:
	// need to keep track of threads so we can join them
	std::vector<std::thread> m_workerTDvec;
	// the task queue
	std::queue<std::function<void()>> m_tasksque;
	// number of threads
	size_t m_number_of_threads{};

	// synchronization
	std::mutex m_tasksque_mutex;
	std::condition_variable m_condition;
	bool m_stopflag{ false };
};

// the constructor just launches some amount of workers
inline ThreadPool::ThreadPool(size_t threads)
{
	add_thread(threads);
}

// the destructor joins all threads
inline ThreadPool::~ThreadPool()
{
	{
		std::unique_lock<std::mutex> lock(m_tasksque_mutex);
		m_stopflag = true;
	}
	m_condition.notify_all();
	for (std::thread& worker : m_workerTDvec) { worker.join(); }
}

// add new work item to the pool
template <typename F, typename... Args>
decltype(auto) ThreadPool::addWorkFunc(F&& f, Args&&...)
{
#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
#if _MSVC_LANG<=201402L
	using return_type = std::result_of_t<F(Args...)>;
#else
	//typedef typename std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...> return_type;
	using return_type = std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>;
#endif
#elif __linux__ || __APPLE__
#if __cplusplus <=201402L
	using return_type = std::result_of_t<F(Args...)>;
#else
	using return_type = std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>;
#endif
#endif
	auto task = std::make_shared<std::packaged_task<return_type()>>([Func = std::forward<F>(f)] { return Func(); });

	std::future<return_type> res = task->get_future();
	{
		std::unique_lock<std::mutex> lock(m_tasksque_mutex);

		// don't allow enqueueing after stopping the pool
		if (m_stopflag) { throw std::runtime_error("enqueue on stopped ThreadPool"); }
		m_tasksque.emplace([task]() { (*task)(); });
	}
	m_condition.notify_one();
	return res;
}

#include <iostream>
inline void ThreadPool::add_thread(size_t threads)
{
	for (size_t i = 0; i < threads; ++i) {
		m_workerTDvec.emplace_back(
			[this]
			{
				for (;;)
				{
					std::function<void()> task;
					{
						std::unique_lock lock(this->m_tasksque_mutex);
						this->m_condition.wait(lock, [this] { return this->m_stopflag || !this->m_tasksque.empty(); });
						if (this->m_stopflag && this->m_tasksque.empty()) { return; }
						task = std::move(this->m_tasksque.front());
						this->m_tasksque.pop();
					}
					task();
				}
			}
		);
	}
}


#endif