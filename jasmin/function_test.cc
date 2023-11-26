#include "jasmin/function.h"

#include "gtest/gtest.h"

namespace jasmin {
namespace {

struct PushImmediateBool : jasmin::Instruction<PushImmediateBool> {
  static constexpr bool execute(std::span<Value, 0>, bool b) { return b; }
};

using Instructions = jasmin::MakeInstructionSet<PushImmediateBool>;

TEST(Function, AppendIncorrectType) {
  bool converted = false;

  struct Converter {
    explicit Converter(bool* b) : converted_(b) {}
    operator bool() const {
      *converted_ = true;
      return true;
    }
    bool* converted_;
  };

  jasmin::Function<Instructions> func(0, 1);

  Converter c(&converted);
  EXPECT_FALSE(converted);
  // `c` Should be cast to `bool` in call to `append`.
  func.append<PushImmediateBool>(c);
  EXPECT_TRUE(converted);
}

}  // namespace
}  // namespace jasmin
