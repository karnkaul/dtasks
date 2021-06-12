#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <shared_mutex>
#include <unordered_map>
#include <vector>
#include <kt/async_queue/async_queue.hpp>
#include <kt/kthread/kthread.hpp>

namespace dts {
#if defined(DTASKS_CATCH_RUNTIME_ERRORS)
constexpr bool catch_runtime_errors = true;
#else
constexpr bool catch_runtime_errors = false;
#endif

///
/// \brief Type safe ID per task
///
struct task_id {
	using type = std::uint64_t;

	type id = 0;
};

///
/// \brief Central task manager, uses thread pool/workers and an async task queue
///
class task_queue {
  public:
	///
	/// \brief Status of a task (identified by a task_id)
	///
	enum class status_t {
		unknown,
		enqueued,
		executing,
		done,
		error,
	};
	///
	/// \brief Alias for tasks
	///
	using task_t = std::function<void()>;

	///
	/// \brief Constructor
	/// \param worker_count Number of threads and workers to create (min 1)
	///
	explicit task_queue(std::uint8_t worker_count = 4);
	///
	/// \brief Destructor
	///
	virtual ~task_queue();

	///
	/// \brief Enqueue a task
	/// \param task Task to enqueue
	/// \returns task_id instance identifying this task
	///
	task_id enqueue(task_t const& task);
	///
	/// \brief Enqueue a (sequence) container of tasks
	///
	template <typename C>
	std::vector<task_id> enqueue_all(C&& container);

	///
	/// \brief Obtain status of task_t identified by id
	///
	status_t task_status(task_id id) const;
	///
	/// \brief Check if stage_t identified by id is done
	///
	bool task_done(task_id id) const;
	///
	/// \brief Check if all stages in container are done
	///
	template <typename C>
	bool tasks_done(C const& container) const;
	///
	/// \brief Wait for task_t identified by id to complete (or throw); blocks calling thread
	///
	bool wait(task_id id);
	///
	/// \brief Wait for container of task_id to complete (or throw); blocks calling thread
	///
	template <typename C>
	void wait_tasks(C const& container);
	///
	/// \brief Wait for queue to drain and all workers to be idle
	/// Warning: do not enqueue tasks (on other threads) while blocked on this call!
	///
	void wait_idle();

  protected:
	template <typename K>
	struct status_map {
		std::unordered_map<K, status_t> map;
		mutable std::shared_mutex mutex;

		void set(K key, status_t value);
		status_t get(K key) const;
		bool wait(K key);
	};

  private:
	using task_entry_t = std::pair<task_id, task_t>;
	using task_status_t = status_map<task_id::type>;
	using queue_t = kt::async_queue<task_entry_t>;

	struct worker {
		kt::kthread thread;

		worker(task_status_t* status, queue_t* queue);

		static void run(task_status_t& out_status, task_entry_t const& entry);
		static void error(task_status_t& out_status, task_entry_t const& entry, std::runtime_error const& err);
	};

	task_id next_task_id() noexcept;

	queue_t m_queue;
	task_status_t m_status;
	std::vector<worker> m_workers;
	std::atomic<task_id::type> m_next_task;
};

// impl

template <typename K>
void task_queue::status_map<K>::set(K key, status_t value) {
	std::unique_lock lock(mutex);
	if (value == status_t::done) {
		map.erase(key);
	} else {
		map[key] = value;
	}
}
template <typename K>
task_queue::status_t task_queue::status_map<K>::get(K key) const {
	std::shared_lock lock(mutex);
	if (auto it = map.find(key); it != map.end()) { return it->second; }
	return status_t::done;
}
template <typename K>
bool task_queue::status_map<K>::wait(K key) {
	std::unique_lock lock(mutex);
	auto it = map.find(key);
	while (it != map.end() && it->second < status_t::done) {
		lock.unlock();
		kt::kthread::yield();
		lock.lock();
		it = map.find(key);
	}
	if (it != map.end() && it->second == status_t::done) {
		map.erase(it);
		it = map.end();
	}
	return it == map.end() || it->second >= status_t::done;
}
template <typename C>
std::vector<task_id> task_queue::enqueue_all(C&& container) {
	static_assert(std::is_same_v<typename std::decay_t<C>::value_type, task_t>, "Invalid type");
	std::vector<task_id> ret;
	std::vector<task_entry_t> entries;
	ret.reserve(container.size());
	entries.reserve(container.size());
	for (auto const& task : container) {
		task_id const id = next_task_id();
		entries.push_back({id, task});
		ret.push_back(id);
	}
	m_queue.push(std::move(entries));
	return ret;
}
template <typename C>
bool task_queue::tasks_done(C const& container) const {
	static_assert(std::is_same_v<typename std::decay_t<C>::value_type, task_id>, "Invalid type");
	return std::all_of(std::begin(container), std::end(container), [this](auto id) { return task_done(id); });
}
template <typename C>
void task_queue::wait_tasks(C const& container) {
	static_assert(std::is_same_v<typename std::decay_t<C>::value_type, task_id>, "Invalid type");
	for (auto const& id : container) { wait(id); }
}
} // namespace dts
