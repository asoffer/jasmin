#ifndef JASMIN_INSTRUCTIONS_STACK_H
#define JASMIN_INSTRUCTIONS_STACK_H

#include <cstddef>
#include <memory>

#include "jasmin/instruction.h"
#include "jasmin/value_stack.h"

namespace jasmin {
namespace internal {

struct StackFrame {
  std::byte *allocate_once(size_t size_in_bytes) {
    JASMIN_INTERNAL_DEBUG_ASSERT(
        data() == nullptr,
        "`allocate_once` must only be called once per stack frame.");
    data_.reset(new std::byte[size_in_bytes]);
    return data_.get();
  }

  std::byte const *data() const { return data_.get(); }
  std::byte *data() { return data_.get(); }

 private:
  std::unique_ptr<std::byte[]> data_;
};

}  // namespace internal

// Initializes sufficient space in the stack frame. Must be called before any
// other stack-related instructions on the current `jasmin::Function`. Must not
// be called more than once on any `jasmin::Function`.
struct StackAllocate : StackMachineInstruction<StackAllocate> {
  using function_state = internal::StackFrame;
  static constexpr void execute(ValueStack &, function_state &frame,
                                size_t size_in_bytes) {
    frame.allocate_once(size_in_bytes);
  }
  static std::string debug(std::span<Value const, 1> immediates) {
    return "stack-allocate" + std::to_string(immediates[0].as<size_t>()) +
           " byte(s)";
  }
};

// Returns a pointer into the stack frame associated with the current function,
// offset by the amount `offset`.
struct StackOffset : StackMachineInstruction<StackOffset> {
  using function_state = internal::StackFrame;
  static constexpr void execute(ValueStack &value_stack, function_state &frame,
                                size_t offset) {
    value_stack.push(frame.data() + offset);
  }
  static std::string debug(std::span<Value const, 2> immediates) {
    return "stack-offset" + std::to_string(immediates[0].as<size_t>()) +
           " byte(s)";
  }
};

}  // namespace jasmin

#endif  // JASMIN_INSTRUCTIONS_STACK_H
