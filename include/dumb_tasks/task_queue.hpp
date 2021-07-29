#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <shared_mutex>
#include <unordered_map>
#include <vector>
#include <dumb_tasks/detail/id.hpp>
#include <ktl/async_queue.hpp>
#include <ktl/kthread.hpp>

namespace dts {
constexpr bool catch_runtime_errors =
#if defined(DTASKS_CATCH_RUNTIME_ERRORS)
	true;
#else
	false;
#endif

///
/// \brief Type safe ID per task
///
struct task_id : detail::id_t<std::uint64_t> {};

///
/// \brief Central task manager, uses thread pool/agents and an async task queue
///
class task_queue {
	using task_entry_t = std::pair<task_id, std::function<void()>>;
	using queue_t = ktl::async_queue<task_entry_t>;

  public:
	///
	/// \brief Alias for queue index
	///
	using queue_id = queue_t::queue_id;

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
	/// \param agent_count Number of threads and agents to create
	///
	explicit task_queue(std::uint8_t agent_count = 2);
	///
	/// \brief Destructor
	///
	virtual ~task_queue();

	///
	/// \brief Add a new task queue and obtain its qid
	///
	queue_id add_queue() { return m_queue.add_queue(); }
	///
	/// \brief Add a new agent that polls the given qids
	///
	void add_agent(std::vector<queue_id> qids = {});
	///
	/// \brief Enqueue a task
	/// \param task Task to enqueue
	/// \returns task_id instance identifying this task
	///
	task_id enqueue(task_t const& task, queue_id qid = 0);
	///
	/// \brief Enqueue a (sequence) container of tasks
	///
	template <typename C>
	std::vector<task_id> enqueue_all(C&& container, queue_id qid = 0);

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
	/// \brief Wait for queue to drain and all agents to be idle
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
	using task_status_t = status_map<task_id::type>;

	struct agent_t {
		ktl::kthread thread;

		agent_t(task_status_t* status, queue_t* queue, std::vector<queue_id> qids);

		static void run(queue_t* queue, task_status_t* status, std::vector<queue_id> const& qids);
		static void execute(task_status_t& out_status, task_entry_t const& entry);
		static void error(task_status_t& out_status, task_entry_t const& entry, std::runtime_error const& err);
	};

	task_id next_task_id() noexcept;

	queue_t m_queue;
	task_status_t m_status;
	std::vector<agent_t> m_agents;
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
		ktl::kthread::yield();
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
std::vector<task_id> task_queue::enqueue_all(C&& container, queue_id qid) {
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
	m_queue.push(std::move(entries), qid);
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
