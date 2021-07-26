#include <algorithm>
#include <dumb_tasks/error_handler.hpp>
#include <dumb_tasks/task_queue.hpp>

namespace dts {
task_queue::agent_t::agent_t(task_status_t* status, queue_t* queue, std::vector<queue_id> qids)
	: thread([status, queue, qids = std::move(qids)]() { run(queue, status, qids); }) {}

void task_queue::agent_t::run(queue_t* queue, task_status_t* status, std::vector<queue_id> const& qids) {
	while (auto entry = queue->pop_any(qids)) {
		if constexpr (catch_runtime_errors) {
			try {
				execute(*status, *entry);
			} catch (std::runtime_error const& err) { error(*status, *entry, err); }
		} else {
			execute(*status, *entry);
		}
	}
}

void task_queue::agent_t::execute(task_status_t& out_status, task_entry_t const& entry) {
	auto const& [id, task] = entry;
	out_status.set(id.id, status_t::executing);
	task();
	out_status.set(id.id, status_t::done);
}

void task_queue::agent_t::error(task_status_t& out_status, task_entry_t const& entry, [[maybe_unused]] std::runtime_error const& err) {
	auto const& [id, task] = entry;
	out_status.set(id.id, status_t::error);
#if defined(DTASKS_CATCH_RUNTIME_ERRORS)
	if (g_error_handler) { (*g_error_handler)(err, id.id); }
#endif
}

task_queue::task_queue(std::uint8_t agent_count) {
	m_queue.active(true);
	m_next_task.store(0);
	for (std::uint8_t i = 0; i < agent_count; ++i) { add_agent(); }
}

task_queue::~task_queue() { m_queue.active(false); }

void task_queue::add_agent(std::vector<queue_id> qids) { m_agents.push_back({&m_status, &m_queue, std::move(qids)}); }

task_id task_queue::enqueue(task_t const& task, queue_id qid) {
	task_id const ret = next_task_id();
	m_queue.push({ret, task}, qid);
	return m_queue.active() ? ret : task_id{};
}

task_queue::status_t task_queue::task_status(task_id id) const {
	if (id.id > m_next_task.load()) { return status_t::unknown; }
	return m_status.get(id.id);
}

bool task_queue::task_done(task_id id) const { return id.identity() || task_status(id) == status_t::done; }

bool task_queue::wait(task_id id) {
	if (id.identity()) { return true; }
	if (id.id > m_next_task.load()) { return false; }
	return m_status.wait(id.id);
}

void task_queue::wait_idle() {
	while (!m_queue.empty()) { kt::kthread::yield(); }
	bool idle = false;
	while (!idle) {
		kt::kthread::yield();
		std::shared_lock lock(m_status.mutex);
		idle = std::all_of(m_status.map.begin(), m_status.map.end(), [](auto const& kvp) { return kvp.second == status_t::done; });
	}
}

task_id task_queue::next_task_id() noexcept {
	task_id ret;
	ret.id = ++m_next_task;
	m_status.set(ret.id, status_t::enqueued);
	return ret;
}
} // namespace dts
