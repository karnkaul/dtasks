#include <algorithm>
#include <dumb_tasks/error_handler.hpp>
#include <dumb_tasks/task_queue.hpp>

namespace dts {
task_queue::worker::worker(task_status_t& status, kt::async_queue<task_entry_t>& queue) {
	thread = kt::kthread([&status, &queue]() {
		while (auto entry = queue.pop()) {
			if constexpr (catch_runtime_errors) {
				try {
					run(status, *entry);
				} catch (std::runtime_error const& err) { error(status, *entry, err); }
			} else {
				run(status, *entry);
			}
		}
	});
}

void task_queue::worker::run(task_status_t& status, task_entry_t const& entry) {
	auto const& [id, task] = entry;
	status.set(id.id, status_t::executing);
	task();
	status.set(id.id, status_t::done);
}

void task_queue::worker::error(task_status_t& status, task_entry_t const& entry, std::runtime_error const& err) {
	auto const& [id, task] = entry;
	status.set(id.id, status_t::error);
	if (g_error_handler) { (*g_error_handler)(err, id.id); }
}

task_queue::task_queue(std::uint8_t worker_count) {
	if (worker_count == 0) { worker_count = 1; }
	m_queue.active(true);
	m_next_task.store(0);
	for (std::uint8_t i = 0; i < worker_count; ++i) { m_workers.push_back(worker(m_status, m_queue)); }
}

task_queue::~task_queue() {
	m_queue.active(false);
	m_workers.clear();
}

task_id task_queue::enqueue(task_t const& task) {
	task_id const ret = next_task_id();
	m_queue.push({ret, task});
	return ret;
}

task_queue::status_t task_queue::task_status(task_id id) const {
	if (id.id > m_next_task.load()) { return status_t::unknown; }
	return m_status.get(id.id);
}

bool task_queue::task_done(task_id id) const { return task_status(id) == status_t::done; }

bool task_queue::wait(task_id id) {
	if (id.id == 0 || id.id > m_next_task.load()) { return false; }
	return m_status.wait(id.id);
}

void task_queue::wait_idle() {
	while (!m_queue.empty()) { kt::kthread::yield(); }
	bool idle = false;
	while (!idle) {
		kt::kthread::yield();
		auto lock = m_status.mutex.lock<std::shared_lock>();
		idle = std::all_of(m_status.map.begin(), m_status.map.end(), [](auto const& kvp) { return kvp.second == status_t::done; });
	}
}

task_id task_queue::next_task_id() noexcept {
	++m_next_task;
	m_status.set(m_next_task, status_t::enqueued);
	return {m_next_task};
}
} // namespace dts
