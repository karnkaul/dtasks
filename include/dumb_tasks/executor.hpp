#pragma once
#include <ktl/async/kfuture.hpp>
#include <ktl/not_null.hpp>
#include <cstdint>
#include <memory>
#include <span>

namespace dts {
using task_t = ktl::kfunction<void()>;
using future_t = ktl::kfuture<void>;

class thread_pool {
  public:
	thread_pool(std::size_t size = 2U);
	~thread_pool();

	std::size_t size() const noexcept;

  private:
	struct impl;
	std::unique_ptr<impl> m_impl;
	friend class executor;
};

class executor {
  public:
	executor(ktl::not_null<thread_pool*> thread_pool);

	executor(executor&&) noexcept;
	executor& operator=(executor&&) noexcept;
	~executor();

	bool running() const noexcept;
	void start();
	void stop();

	future_t enqueue(task_t task);
	future_t schedule(std::span<task_t> tasks, std::span<future_t> dependencies);

	bool idle() const;
	std::size_t scheduled() const;
	void wait_idle();

	bool has_exception() const;
	void rethrow();

  private:
	struct impl;
	std::unique_ptr<impl> m_impl;
};
} // namespace dts
