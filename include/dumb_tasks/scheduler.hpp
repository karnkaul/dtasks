#pragma once
#include <list>
#include <dumb_tasks/task_queue.hpp>

namespace dts {
///
/// \brief Task scheduler: enqueues batches of tasks with dependencies
///
class scheduler : public task_queue {
  public:
	using task_queue::status_t;
	using task_queue::task_t;
	using task_queue::wait;

	///
	/// \brief Type safe ID per stage_t
	///
	struct stage_id {
		using type = std::uint64_t;

		type id = 0;
	};
	///
	/// \brief Batch of tasks and dependencies (as a stage_id each)
	///
	struct stage_t {
		std::vector<stage_id> deps;
		std::vector<task_t> tasks;
	};

	///
	/// \brief Constructor
	/// \param worker_count Number of threads and workers to create (min 1)
	///
	explicit scheduler(std::uint8_t worker_count = 4);
	///
	/// \brief Destructor
	///
	~scheduler() override;

	///
	/// \brief Stage a batch of tasks
	/// \param stage Tasks to stage
	/// \returns stage_id instance identifying this batch
	///
	stage_id stage(stage_t&& stage);
	///
	/// \brief Stage a batch of tasks
	/// \param stage Tasks to stage
	/// \returns stage_id instance identifying this batch
	///
	stage_id stage(stage_t const& stage);

	///
	/// \brief Obtain status of stage_t identified by id
	///
	status_t stage_status(stage_id id) const;
	///
	/// \brief Check if stage_t identified by id is done
	///
	bool stage_done(stage_id id) const;
	///
	/// \brief Check if all stages in container are done
	///
	template <typename C>
	bool stages_done(C const& container) const;
	///
	/// \brief Wait for stage_t identified by id to complete (or throw); blocks calling thread
	///
	bool wait(stage_id id);
	///
	/// \brief Wait for container of task_id to complete (or throw); blocks calling thread
	///
	template <typename C>
	void wait_stages(C&& container);
	///
	/// \brief Clear all waiting stages
	///
	void clear();

  private:
	struct stage_entry_t {
		std::vector<task_t> tasks;
		std::vector<task_id> ids;
		std::vector<stage_id> deps;
		stage_id id;
	};

	using stage_status_t = status_map<stage_id::type>;

	stage_status_t m_stage_status;
	std::list<stage_entry_t> m_waiting;
	std::list<stage_entry_t> m_running;
	std::atomic<bool> m_work;
	std::atomic<stage_id::type> m_next_stage;
	kt::kthread m_thread;
	mutable kt::lockable_t<> m_mutex;
};

// impl

template <typename C>
bool scheduler::stages_done(C const& container) const {
	static_assert(std::is_same_v<typename std::decay_t<C>::value_type, stage_id>, "Invalid type");
	return std::all_of(std::begin(container), std::end(container), [this](auto id) { return stage_done(id); });
}
template <typename C>
void scheduler::wait_stages(C&& container) {
	static_assert(std::is_same_v<typename std::decay_t<C>::value_type, stage_id>, "Invalid type");
	for (auto const& id : container) {
		wait(id);
	}
}
} // namespace dts
