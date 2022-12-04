#ifndef JASMIN_DEBUG_H
#define JASMIN_DEBUG_H
#if defined(JASMIN_DEBUG)

#include <concepts>
#include <cstdio>
#include <cstdlib>
#include <typeinfo>
#include <type_traits>

namespace jasmin::internal {

// A type-id mechanism which treats all pointers as equivalent. Strictly
// speaking, we would prefer to test compatibility, but this is generally not
// possible via this mechanism.
struct TypeId {
  void const *value = nullptr;
  char const *name  = "unknown";

  friend bool operator==(TypeId const &lhs, TypeId const &rhs) {
    return lhs.value == rhs.value;
  }
  friend bool operator!=(TypeId const &lhs, TypeId const &rhs) {
    return not(lhs == rhs);
  }
};

template <typename T>
inline TypeId type_id{
    .value = &type_id<std::conditional_t<std::is_pointer_v<T>, void *, T>>,
    .name  = typeid(T).name(),
};

// Writes the strings to stderr and aborts execution.
[[noreturn]] void DebugAbort(std::same_as<char const *> auto... args) {
  (std::fputs(args, stderr), ...);
  std::abort();
}

}  // namespace jasmin::internal

#define JASMIN_INTERNAL_DEBUG_ASSERT_IMPL_(expr, file, line, ...)              \
  do {                                                                         \
    if (not(expr)) {                                                           \
      ::jasmin::internal::DebugAbort("Failed assertion on at " file "(" #line  \
                                     "): " #expr "\n",                         \
                                     __VA_ARGS__);                             \
    }                                                                          \
  } while (false)

#define JASMIN_INTERNAL_DEBUG_ASSERT_IMPL(expr, file, line, ...)               \
  JASMIN_INTERNAL_DEBUG_ASSERT_IMPL_(expr, file, line, __VA_ARGS__)

#define JASMIN_INTERNAL_DEBUG_ASSERT(expr, ...)                                \
  JASMIN_INTERNAL_DEBUG_ASSERT_IMPL(expr, __FILE__, __LINE__, __VA_ARGS__)

#else

#define JASMIN_INTERNAL_DEBUG_ASSERT(expr, ...)

#endif  // defined(JASMIN_DEBUG)
#endif  // JASMIN_DEBUG_H
