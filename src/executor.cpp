#include <dumb_tasks/executor.hpp>
#include <ktl/async/async_queue.hpp>
#include <ktl/async/kmutex.hpp>
#include <ktl/async/kthread.hpp>
#include <algorithm>
#include <atomic>

namespace dts {
namespace {
template <typename T>
std::vector<T> make_vec(std::span<T> const span) {
	std::vector<T> ret;
	ret.reserve(span.size());
	std::move(span.begin(), span.end(), std::back_inserter(ret));
	return ret;
}

bool erase_ready(std::vector<future_t>& vec) {
	if (vec.empty()) { return true; }
	std::erase_if(vec, [](future_t const& f) { return !f.busy(); });
	return vec.empty();
}

// execution context
struct context_t {
	// unit of work
	struct unit_t {
		task_t task{};
		ktl::kpromise<void> promise{};
	};

	// current exception
	ktl::strict_tmutex<std::exception_ptr> exception;

	std::exception_ptr get_exception() { return *ktl::klock(exception); }
	void clear_exception() { *ktl::klock(exception) = {}; }

	void operator()(unit_t&& unit) {
		try {
			unit.task();
		} catch (...) {
			// store first caught exception
			if (auto lock = ktl::klock(exception); !*lock) { *lock = std::current_exception(); }
		}
		unit.promise.set_value();
	}
};

// async queue of work
using queue_t = ktl::async_queue<context_t::unit_t>;

future_t push(queue_t& queue, task_t&& task) {
	auto unit = context_t::unit_t{std::move(task)};
	auto ret = unit.promise.get_future();
	queue.push(std::move(unit));
	return ret;
}

// worker agent
struct agent_t {
	ktl::kthread thread;
	std::atomic<bool> busy;
	queue_t& queue;
	context_t& ctx;

	agent_t(queue_t& queue, context_t& ctx) noexcept : queue(queue), ctx(ctx) { busy.store(false); }

	void run() {
		thread = ktl::kthread([this]() {
			while (auto unit = queue.pop()) {
				if (!unit) { break; }
				busy.store(true);
				ctx(std::move(*unit));
				busy.store(false);
			}
		});
	}
};

// job scheduler
class scheduler_t {
  public:
	scheduler_t(queue_t& queue) : m_queue(queue) {
		m_thread = ktl::kthread([this](ktl::kthread::stop_t stop) {
			while (!stop.stop_requested()) {
				ktl::kthread::yield();
				update();
			}
		});
		m_thread.m_join = ktl::kthread::policy::stop;
	}

	void update() {
		auto signal_done = [](job_t& job) {
			// remove completed task handles (futures)
			if (erase_ready(job.futures)) {
				// all tasks completed
				job.promise.set_value();
				return true;
			}
			// tasks pending completion
			return false;
		};
		std::vector<job_t> ready;
		auto transfer_ready = [this](job_t& job) {
			// remove completed dependencies
			if (erase_ready(job.futures)) {
				// tasks ready to execute
				for (auto& task : job.tasks) { job.futures.push_back(push(m_queue, std::move(task))); }
				job.tasks.clear();
				// append to running
				m_running.push_back(std::move(job));
				return true;
			}
			// dependencies pending completion
			return false;
		};
		auto lock = std::scoped_lock(m_mutex);
		if (!m_running.empty()) { std::erase_if(m_running, signal_done); }
		if (!m_waiting.empty()) { std::erase_if(m_waiting, transfer_ready); }
	}

	future_t operator()(std::span<task_t> const tasks, std::span<future_t> const deps) {
		auto job = job_t::make(tasks, deps);
		auto ret = job.promise.get_future();
		auto lock = std::scoped_lock(m_mutex);
		m_waiting.push_back(std::move(job));
		return ret;
	}

	bool empty() const {
		auto lock = std::scoped_lock(m_mutex);
		return m_waiting.empty() && m_running.empty();
	}

	std::size_t size() const {
		auto lock = std::scoped_lock(m_mutex);
		return m_waiting.size();
	}

	void clear() {
		auto lock = std::scoped_lock(m_mutex);
		m_waiting.clear();
		m_running.clear();
	}

  private:
	// job: unit of work + futures (dependencies / task handles)
	struct job_t {
		std::vector<task_t> tasks{};
		std::vector<future_t> futures{};
		ktl::kpromise<void> promise{};

		static job_t make(std::span<task_t> const tasks, std::span<future_t> const deps) { return {make_vec(tasks), make_vec(deps)}; }
	};

	ktl::kthread m_thread;
	mutable std::mutex m_mutex;
	std::vector<job_t> m_waiting;
	std::vector<job_t> m_running;
	queue_t& m_queue;
};
} // namespace

struct thread_pool::impl {
	queue_t queue;
	context_t ctx;
	std::vector<std::unique_ptr<agent_t>> agents;

	void start() {
		// start queue
		queue.active(true);
		for (auto& agent : agents) {
			// start all agent threads
			if (!agent->thread.active()) { agent->run(); }
		}
	}

	void stop() {
		// stop queue
		queue.clear(false);
		// wait idle
		while (!idle()) { ktl::kthread::yield(); }
		// join all agent threads
		for (auto& agent : agents) { agent->thread.join(); }
		assert(std::all_of(agents.begin(), agents.end(), [](auto const& agent) { return !agent->thread.active(); }));
		// clear context exception
		ctx.clear_exception();
	}

	bool idle() const {
		return queue.empty() && std::all_of(agents.begin(), agents.end(), [](auto const& agent) { return !agent->busy.load(); });
	}
};

struct executor::impl {
	scheduler_t scheduler;
	thread_pool& pool;

	impl(thread_pool& pool) noexcept : scheduler(pool.m_impl->queue), pool(pool) {}
};

thread_pool::thread_pool(std::size_t size) : m_impl(std::make_unique<impl>()) {
	size = std::clamp(size, std::size_t(1U), std::size_t(std::thread::hardware_concurrency()));
	for (std::size_t i = 0; i < size; ++i) { m_impl->agents.emplace_back(std::make_unique<agent_t>(m_impl->queue, m_impl->ctx)); }
}

thread_pool::~thread_pool() {
	if (m_impl) { m_impl->stop(); }
}

executor::executor(ktl::not_null<thread_pool*> const thread_pool) : m_impl(std::make_unique<impl>(*thread_pool)) { start(); }

executor::executor(executor&&) noexcept = default;
executor& executor::operator=(executor&&) noexcept = default;

executor::~executor() { stop(); }

bool executor::running() const noexcept { return m_impl && m_impl->pool.m_impl->queue.active(); }

void executor::start() {
	if (!m_impl) { return; }
	m_impl->pool.m_impl->start();
}

void executor::stop() {
	if (!m_impl) { return; }
	m_impl->pool.m_impl->stop();
	m_impl->scheduler.clear();
}

future_t executor::enqueue(task_t task) {
	if (running()) { return push(m_impl->pool.m_impl->queue, std::move(task)); }
	return {};
}

future_t executor::schedule(std::span<task_t> const tasks, std::span<future_t> const dependencies) {
	if (running()) { return m_impl->scheduler(tasks, dependencies); }
	return {};
}

bool executor::idle() const { return !m_impl || (m_impl->pool.m_impl->idle() && m_impl->scheduler.empty()); }
std::size_t executor::scheduled() const { return m_impl ? m_impl->scheduler.size() : 0U; }

void executor::wait_idle() {
	while (!idle()) { ktl::kthread::yield(); }
}

bool executor::has_exception() const { return static_cast<bool>(m_impl->pool.m_impl->ctx.get_exception()); }

void executor::rethrow() {
	if (auto except = m_impl->pool.m_impl->ctx.get_exception()) { std::rethrow_exception(except); }
}
} // namespace dts
