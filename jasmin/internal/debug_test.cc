#include "jasmin/internal/debug.h"

#include <type_traits>

#include "gtest/gtest.h"

namespace jasmin::internal {
namespace {

#if defined(JASMIN_DEBUG)

TEST(DebugAbort, Aborts) {
  EXPECT_DEATH({ DebugAbort("failure", "message"); }, "failure.*message");
}

TEST(TypeId, Equality) {
  EXPECT_EQ(type_id<int>, type_id<int>);
  EXPECT_EQ(type_id<int>, type_id<signed int>);
  EXPECT_NE(type_id<int>, type_id<bool>);
  EXPECT_NE(type_id<char>, type_id<signed char>);
  EXPECT_NE(type_id<char>, type_id<unsigned char>);

  EXPECT_EQ(type_id<char*>, type_id<bool*>);
  EXPECT_EQ(type_id<char*>, type_id<char*>);
}

#endif  // defined(JASMIN_DEBUG)

TEST(AssertMacro, AbortsOnlyOnFailure) {
  JASMIN_INTERNAL_DEBUG_ASSERT(true, "message");
#if defined(JASMIN_DEBUG)
  EXPECT_DEATH({ JASMIN_INTERNAL_DEBUG_ASSERT(false, "message"); },
               "false.*message");
#endif  // defined(JASMIN_DEBUG)
}

}  // namespace
}  // namespace jasmin::internal
