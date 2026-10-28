// Syntax-only check of D7's API surface (declarations, stub bodies): every standard integer type and
// float and double encode; bool, the character types and long double do not.
// c++ -std=c++20 -fsyntax-only overloads.cc
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>

namespace sortable {

template <class T>
inline constexpr bool is_character_v = std::same_as<T, char> || std::same_as<T, wchar_t> || std::same_as<T, char8_t> ||
	std::same_as<T, char16_t> || std::same_as<T, char32_t>;

template <class T>
concept encodable = (std::integral<T> && !std::same_as<T, bool> && !is_character_v<T>) || std::same_as<T, float> ||
	std::same_as<T, double>;

inline constexpr std::size_t max_size = 10;

template <encodable T>
std::size_t encode(T v, char* out) noexcept {
	(void)v;
	(void)out;
	return 0;
}

template <encodable T>
std::string encode(T v) {
	char buf[max_size];
	return std::string(buf, encode(v, buf));
}

}  // namespace sortable

template <class T>
inline constexpr bool can_encode = requires(T v, char* p) {
	sortable::encode(v, p);
	sortable::encode(v);
};

#define OK(expr) static_assert(can_encode<decltype(expr)>)
#define NO(expr) static_assert(!can_encode<decltype(expr)>)

OK(5);
OK(5L);
OK(5LL);
OK(5u);
OK(5UL);
OK(5ULL);
OK(std::size_t{5});
OK(std::ptrdiff_t{5});
OK(std::int8_t{5});
OK(std::uint8_t{5});
OK(std::int16_t{5});
OK(std::uint16_t{5});
OK(std::int32_t{5});
OK(std::uint32_t{5});
OK(std::int64_t{5});
OK(std::uint64_t{5});
OK(static_cast<signed char>(5));
OK(static_cast<unsigned char>(5));
OK(5.0f);
OK(5.0);
NO(true);
NO('a');
NO(L'a');
NO(u8'a');
NO(u'a');
NO(U'a');
NO(5.0L);

int main() {
	char buf[sortable::max_size];
	sortable::encode(5, buf);
	sortable::encode(5.0f, buf);
	(void)sortable::encode(std::size_t{5});
}
