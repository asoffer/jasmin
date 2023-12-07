#include "jasmin/instructions/common.h"

#include "jasmin/testing.h"
#include "nth/container/stack.h"
#include "nth/test/test.h"

namespace {

NTH_TEST("instruction/Push") {
  nth::stack<jasmin::Value> value_stack;

  jasmin::ExecuteInstruction<jasmin::Push>(value_stack, 17);
  NTH_EXPECT(value_stack.size() == 1);
  NTH_EXPECT(value_stack.top().as<int>() == 17);

  jasmin::ExecuteInstruction<jasmin::Push>(value_stack, 18);
  NTH_EXPECT(value_stack.size() == 2);
  NTH_EXPECT(value_stack.top().as<int>() == 18);
}

NTH_TEST("instruction/Drop") {
  nth::stack<jasmin::Value> value_stack{1, 2, 3, 4, 5};

  jasmin::ExecuteInstruction<jasmin::Drop>(value_stack);
  NTH_ASSERT(value_stack.size() == 4);
  std::span values = value_stack.top_span(4);
  NTH_EXPECT(values[0].as<int>() == 1);
  NTH_EXPECT(values[1].as<int>() == 2);
  NTH_EXPECT(values[2].as<int>() == 3);
  NTH_EXPECT(values[3].as<int>() == 4);

  jasmin::ExecuteInstruction<jasmin::Drop>(value_stack);
  NTH_ASSERT(value_stack.size() == 3);
  NTH_EXPECT(value_stack.top_span(3)[0].as<int>() == 1);
  NTH_EXPECT(value_stack.top_span(3)[1].as<int>() == 2);
  NTH_EXPECT(value_stack.top_span(3)[2].as<int>() == 3);
}

NTH_TEST("instruction/Duplicate") {
  nth::stack<jasmin::Value> value_stack{3};

  jasmin::ExecuteInstruction<jasmin::Duplicate>(value_stack);
  NTH_EXPECT(value_stack.size() == 2);
  NTH_EXPECT(value_stack.top().as<int>() == 3);

  jasmin::ExecuteInstruction<jasmin::Duplicate>(value_stack);
  NTH_EXPECT(value_stack.size() == 3);
  NTH_EXPECT(value_stack.top().as<int>() == 3);
}

NTH_TEST("instruction/DuplicateAt") {
  nth::stack<jasmin::Value> value_stack{3, 4, 5};

  jasmin::ExecuteInstruction<jasmin::DuplicateAt>(
      value_stack,
      jasmin::InstructionSpecification{.parameters = 2, .returns = 1});
  NTH_EXPECT(value_stack.size() == 4);
  NTH_EXPECT(value_stack.top().as<int>() == 4);

  jasmin::ExecuteInstruction<jasmin::DuplicateAt>(
      value_stack,
      jasmin::InstructionSpecification{.parameters = 4, .returns = 1});
  NTH_EXPECT(value_stack.size() == 5);
  NTH_EXPECT(value_stack.top().as<int>() == 3);
}

NTH_TEST("instruction/Swap") {
  nth::stack<jasmin::Value> value_stack;
  value_stack.push(1);
  value_stack.push(2);

  jasmin::ExecuteInstruction<jasmin::Swap>(value_stack);
  NTH_EXPECT(value_stack.size() == 2);
  NTH_EXPECT(value_stack.top().as<int>() == 1);
  value_stack.pop();
  NTH_EXPECT(value_stack.top().as<int>() == 2);
}

NTH_TEST("instruction/Load") {
  int8_t n8   = 8;
  int16_t n16 = 16;
  int32_t n32 = 32;
  int64_t n64 = 64;
  nth::stack<jasmin::Value> value_stack;

  value_stack.push(&n8);
  jasmin::ExecuteInstruction<jasmin::Load>(value_stack, sizeof(int8_t));

  value_stack.push(&n16);
  jasmin::ExecuteInstruction<jasmin::Load>(value_stack, sizeof(int16_t));

  value_stack.push(&n32);
  jasmin::ExecuteInstruction<jasmin::Load>(value_stack, sizeof(int32_t));

  value_stack.push(&n64);
  jasmin::ExecuteInstruction<jasmin::Load>(value_stack, sizeof(int64_t));

  NTH_EXPECT(value_stack.size() == 4);
  NTH_EXPECT(value_stack.top().as<int64_t>() == 64);
  value_stack.pop();
  NTH_EXPECT(value_stack.top().as<int32_t>() == 32);
  value_stack.pop();
  NTH_EXPECT(value_stack.top().as<int16_t>() == 16);
  value_stack.pop();
  NTH_EXPECT(value_stack.top().as<int8_t>() == 8);
}

NTH_TEST("instruction/Store") {
  int8_t n8   = 0;
  int16_t n16 = 0;
  int32_t n32 = 0;
  int64_t n64 = 0;
  nth::stack<jasmin::Value> value_stack;

  value_stack.push(&n8);
  value_stack.push(int8_t{8});
  jasmin::ExecuteInstruction<jasmin::Store>(value_stack, sizeof(int8_t));
  value_stack.push(&n16);
  value_stack.push(int16_t{16});
  jasmin::ExecuteInstruction<jasmin::Store>(value_stack, sizeof(int16_t));
  value_stack.push(&n32);
  value_stack.push(int32_t{32});
  jasmin::ExecuteInstruction<jasmin::Store>(value_stack, sizeof(int32_t));
  value_stack.push(&n64);
  value_stack.push(int64_t{64});
  jasmin::ExecuteInstruction<jasmin::Store>(value_stack, sizeof(int64_t));

  NTH_EXPECT(value_stack.size() == 0);
  NTH_EXPECT(n64 == 64);
  NTH_EXPECT(n32 == 32);
  NTH_EXPECT(n16 == 16);
  NTH_EXPECT(n8 == 8);
}

NTH_TEST("instruction/Rotate") {
  nth::stack<jasmin::Value> value_stack;
  for (size_t i = 0; i < 10; ++i) { value_stack.push(i); }
  jasmin::ExecuteInstruction<jasmin::Rotate>(
      value_stack,
      jasmin::InstructionSpecification{.parameters = 4, .returns = 0}, 1);
  NTH_ASSERT(value_stack.size() == 10);
  std::span values = value_stack.top_span(10);
  NTH_EXPECT(values[9].as<size_t>() == 6);
  NTH_EXPECT(values[8].as<size_t>() == 9);
  NTH_EXPECT(values[7].as<size_t>() == 8);
  NTH_EXPECT(values[6].as<size_t>() == 7);
  NTH_EXPECT(values[5].as<size_t>() == 5);
  NTH_EXPECT(values[4].as<size_t>() == 4);
  NTH_EXPECT(values[3].as<size_t>() == 3);
  NTH_EXPECT(values[2].as<size_t>() == 2);
  NTH_EXPECT(values[1].as<size_t>() == 1);
  NTH_EXPECT(values[0].as<size_t>() == 0);
}

}  // namespace
