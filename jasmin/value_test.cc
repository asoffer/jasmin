#include "jasmin/value.h"

#include <concepts>

#include "gtest/gtest.h"
#include "nth/debug/log/log.h"
#include "nth/debug/log/stderr_log_sink.h"

namespace jasmin {
namespace {

bool init = [] {
  nth::RegisterLogSink(nth::stderr_log_sink);
  return false;
}();

TEST(Value, Construction) {
  EXPECT_TRUE((std::constructible_from<Value, char>));
  struct BarelyFits {
    char data[ValueSize];
  };
  struct Large {
    char data[ValueSize + 1];
  };
  EXPECT_TRUE((std::constructible_from<Value, BarelyFits>));
  EXPECT_FALSE((std::constructible_from<Value, Large>));

  struct alignas(2 * ValueAlignment)
      OverlyStrictAlignmentRequirement {};
  EXPECT_FALSE(
      (std::constructible_from<Value, OverlyStrictAlignmentRequirement>));

  struct NotTriviallyCopyable {
    NotTriviallyCopyable(NotTriviallyCopyable const &) {}
  };
  EXPECT_FALSE((std::constructible_from<Value, NotTriviallyCopyable>));
}

TEST(Value, Access) {
  Value v(0);
  auto copy = v.as<Value>();
  EXPECT_EQ(std::memcmp(&copy, &v, sizeof(Value)), 0);
  EXPECT_EQ(v.as<int>(), 0);
  if constexpr (internal::debug) {
    EXPECT_DEATH({ v.as<unsigned int>(); }, "Value type mismatch");
  }

  v    = 1;
  copy = v.as<Value>();
  EXPECT_EQ(std::memcmp(&copy, &v, sizeof(Value)), 0);
  EXPECT_EQ(v.as<int>(), 1);

  v    = true;
  copy = v.as<Value>();
  EXPECT_EQ(std::memcmp(&copy, &v, sizeof(Value)), 0);
  EXPECT_TRUE(v.as<bool>());
  if constexpr (internal::debug) {
    EXPECT_DEATH({ v.as<int>(); }, "Value type mismatch");
  }
}

TEST(Value, Address) {
  Value v(17);
  void const *ptr = v.address();
  EXPECT_EQ(*reinterpret_cast<int const *>(ptr), 17);
}

TEST(Value, Load) {
  int n           = 17;
  Value v         = Value::Load(&n, sizeof(int));
  void const *ptr = v.address();
  EXPECT_EQ(*reinterpret_cast<int const *>(ptr), 17);
}

TEST(Value, Store) {
  int n           = 0;
  Value v         = 17;
  Value::Store(v, &n, sizeof(int));
  EXPECT_EQ(n, 17);
}

TEST(Value, Raw) {
  Value v1 = 3.14159;
  Value v2 = 0;
  v2.set_raw_value(v1.raw_value());
  EXPECT_EQ(v1.as<double>(), v2.as<double>());
}

#if defined(JASMIN_INTERNAL_CONFIGURATION_DEBUG)

TEST(TypeId, Equality) {
  EXPECT_EQ(internal::type_id<int>, internal::type_id<int>);
  EXPECT_EQ(internal::type_id<int>, internal::type_id<signed int>);
  EXPECT_NE(internal::type_id<int>, internal::type_id<bool>);
  EXPECT_NE(internal::type_id<char>, internal::type_id<signed char>);
  EXPECT_NE(internal::type_id<char>, internal::type_id<unsigned char>);

  EXPECT_EQ(internal::type_id<char *>, internal::type_id<bool *>);
  EXPECT_EQ(internal::type_id<char *>, internal::type_id<char *>);
}

#endif  // defined(JASMIN_INTERNAL_CONFIGURATION_DEBUG)

}  // namespace
}  // namespace jasmin
