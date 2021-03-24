#include <iostream>
#include <dumb_tasks/error_handler.hpp>

namespace dts {
void error_handler_t::operator()(std::runtime_error const& err, std::uint64_t task_id) const {
	std::cerr << "[dtasks] Exception caught in task " << task_id << ": " << err.what() << std::endl;
}
} // namespace dts
