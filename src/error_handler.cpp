#if defined(DTASKS_CATCH_RUNTIME_ERRORS)
#include <iostream>
#include <dumb_tasks/error_handler.hpp>

namespace dts {
void on_err_default(std::runtime_error const& err, std::uint64_t task_id) {
	std::cerr << "[dtasks] Exception caught in task " << task_id << ": " << err.what() << std::endl;
}
} // namespace dts
#endif
