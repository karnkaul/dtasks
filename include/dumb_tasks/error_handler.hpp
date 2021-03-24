#pragma once
#include <cstdint>
#include <stdexcept>

namespace dts {
///
/// \brief Base type for handling errors (only when DTASKS_CATCH_RUNTIME_ERRORS is defined)
///
struct error_handler_t {
	///
	/// \brief Customization point: override in derived handlers
	///
	virtual void operator()(std::runtime_error const& err, std::uint64_t task_id) const;
};
///
/// \brief Base handler instance
///
inline error_handler_t const g_default_error_handler;
///
/// \brief Error dispatch: set to custom derived instance / nullptr to customize
///
inline error_handler_t const* g_error_handler = &g_default_error_handler;
} // namespace dts
