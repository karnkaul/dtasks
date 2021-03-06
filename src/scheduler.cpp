#include <algorithm>
#include <dumb_tasks/scheduler.hpp>

namespace dts {
using namespace std::chrono_literals;

scheduler::scheduler(std::uint8_t worker_count) : task_queue(worker_count) {
	m_next_stage.store(0);
	m_thread = kt::kthread([this](kt::kthread::stop_t stop) {
		while (!stop.stop_requested()) {
			kt::kthread::sleep_for(1ms);
			for (auto it = m_running.begin(); it != m_running.end();) {
				auto const iter = std::remove_if(it->ids.begin(), it->ids.end(), [this](task_id id) { return task_status(id) >= status_t::done; });
				it->ids.erase(iter, it->ids.end());
				if (it->ids.empty()) {
					m_stage_status.set(it->id.id, status_t::done);
					it = m_running.erase(it);
				} else {
					++it;
				}
			}
			std::scoped_lock lock(m_mutex);
			for (auto it = m_waiting.begin(); it != m_waiting.end();) {
				auto const iter = std::remove_if(it->deps.begin(), it->deps.end(), [this](stage_id id) { return m_stage_status.get(id.id) >= status_t::done; });
				it->deps.erase(iter, it->deps.end());
				if (it->deps.empty()) {
					m_stage_status.set(it->id.id, status_t::executing);
					for (auto const& task : it->tasks) { it->ids.push_back(enqueue(task)); }
					it->tasks.clear();
					m_running.push_back(std::move(*it));
					it = m_waiting.erase(it);
				} else {
					++it;
				}
			}
		}
	});
	m_thread.m_join = kt::kthread::policy::stop;
}

scheduler::stage_id scheduler::stage(stage_t&& stage) {
	stage_entry_t entry;
	entry.id.id = ++m_next_stage;
	entry.tasks = std::move(stage.tasks);
	entry.deps = std::move(stage.deps);
	stage_id const ret = entry.id;
	m_stage_status.set(ret.id, status_t::enqueued);
	std::scoped_lock lock(m_mutex);
	m_waiting.push_back(std::move(entry));
	return ret;
}

scheduler::stage_id scheduler::stage(stage_t const& stage) { return this->stage(stage_t(stage)); }

scheduler::status_t scheduler::stage_status(stage_id id) const {
	if (id.identity() || id.id > m_next_stage.load()) { return status_t::unknown; }
	return m_stage_status.get(id.id);
}

bool scheduler::stage_done(stage_id id) const { return id.identity() || stage_status(id) == status_t::done; }

bool scheduler::wait(stage_id id) {
	if (id.identity()) { return true; }
	if (id.id > m_next_stage.load()) { return false; }
	return m_stage_status.wait(id.id);
}

void scheduler::clear() {
	std::scoped_lock lock(m_mutex);
	for (auto const& stage : m_waiting) { m_stage_status.set(stage.id.id, status_t::unknown); }
	m_waiting.clear();
}
} // namespace dts
