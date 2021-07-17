#pragma once
#include <cstdint>
#include <stdexcept>

namespace dts {
///
/// \brief Callback type for handling errors (unused if DTASKS_CATCH_RUNTIME_ERRORS is not defined)
///
using on_err_t = void (*)(std::runtime_error const& err, std::uint64_t task_id);
#if defined(DTASKS_CATCH_RUNTIME_ERRORS)
///
/// \brief Default error handler
///
extern void on_err_default(std::runtime_error const& err, std::uint64_t task_id);

///
/// \brief Error dispatch: set to custom callback / nullptr to customize
///
inline on_err_t g_error_handler = &on_err_default;
#else
///
/// \brief Unused, define DTASKS_CATCH_RUNTIME_ERRORS to enable
///
inline on_err_t g_error_handler = nullptr;
#endif
} // namespace dts
