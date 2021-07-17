#pragma once

namespace dts::detail {
template <typename T, T Z = T{}>
struct id_t {
	using type = T;
	static constexpr T zero = Z;

	T id = zero;

	constexpr bool identity() const noexcept { return id == zero; }

	friend constexpr bool operator==(id_t const& lhs, id_t const& rhs) noexcept { return lhs.id == rhs.id; }
	friend constexpr bool operator!=(id_t const& lhs, id_t const& rhs) noexcept { return !(lhs == rhs); }
};
} // namespace dts::detail
