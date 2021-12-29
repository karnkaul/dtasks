#pragma once

namespace dts::detail {
template <typename T, T Z = T{}>
struct id_t {
	using type = T;
	static constexpr T zero = Z;

	T id = zero;

	constexpr bool identity() const noexcept { return id == zero; }

	constexpr bool operator==(id_t const&) const = default;
};
} // namespace dts::detail
