#include <algorithm>
#include <dtasks/task_scheduler.hpp>

namespace dts {
using namespace std::chrono_literals;

task_scheduler::task_scheduler(std::uint8_t worker_count) : task_queue(worker_count) {
	m_work.store(true);
	m_next_stage.store(0);
	m_thread = kt::kthread([this]() {
		while (m_work.load()) {
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
			auto lock = m_mutex.lock();
			for (auto it = m_waiting.begin(); it != m_waiting.end();) {
				auto const iter = std::remove_if(it->deps.begin(), it->deps.end(), [this](stage_id id) { return m_stage_status.get(id.id) >= status_t::done; });
				it->deps.erase(iter, it->deps.end());
				if (it->deps.empty()) {
					m_stage_status.set(it->id.id, status_t::executing);
					for (auto const& task : it->tasks) {
						it->ids.push_back(enqueue(task));
					}
					it->tasks.clear();
					m_running.push_back(std::move(*it));
					it = m_waiting.erase(it);
				} else {
					++it;
				}
			}
		}
	});
}

task_scheduler::~task_scheduler() {
	m_work.store(false);
	m_thread = {};
}

task_scheduler::stage_id task_scheduler::stage(stage_t&& stage) {
	stage_entry_t entry;
	entry.id.id = ++m_next_stage;
	entry.tasks = std::move(stage.tasks);
	entry.deps = std::move(stage.deps);
	stage_id const ret = entry.id;
	m_stage_status.set(ret.id, status_t::enqueued);
	auto lock = m_mutex.lock();
	m_waiting.push_back(std::move(entry));
	return ret;
}

task_scheduler::stage_id task_scheduler::stage(stage_t const& stage) {
	return this->stage(stage_t(stage));
}

task_scheduler::status_t task_scheduler::stage_status(stage_id id) const {
	if (id.id == 0 || id.id > m_next_stage.load()) {
		return status_t::unknown;
	}
	return m_stage_status.get(id.id);
}

bool task_scheduler::stage_done(stage_id id) const {
	return stage_status(id) == status_t::done;
}

bool task_scheduler::wait(stage_id id) {
	if (id.id == 0 || id.id > m_next_stage.load()) {
		return false;
	}
	return m_stage_status.wait(id.id);
}

void task_scheduler::clear() {
	auto lock = m_mutex.lock();
	for (auto const& stage : m_waiting) {
		m_stage_status.set(stage.id.id, status_t::unknown);
	}
	m_waiting.clear();
}
} // namespace dts
