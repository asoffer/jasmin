#ifndef JASMIN_FUNCTION_H
#define JASMIN_FUNCTION_H

#include <type_traits>

#include "jasmin/call_stack.h"
#include "jasmin/instruction.h"
#include "jasmin/instruction_pointer.h"
#include "jasmin/internal/debug.h"
#include "jasmin/internal/function_base.h"
#include "jasmin/internal/type_list.h"

namespace jasmin {
namespace internal_function {

template <
    internal_instruction::SignatureSatisfiesRequirementsWithImmediateValues E,
    typename... Vs>
constexpr bool ConvertibleArguments = E::invoke_with_argument_types([
]<std::same_as<ValueStack&>, typename... Ts>() {
  return (std::convertible_to<Vs, Ts> and ...);
});

}  // namespace internal_function

// A representation of a function that ties op-codes to instructions (via an
// InstructionSet template parameter).
template <InstructionSet Set>
struct Function final : internal::FunctionBase {
  // Constructs an empty `Function` given a `parameter_count` representing the
  // number of parameters to the function, and a `return_count` representing the
  // number of return values for the function.
  explicit constexpr Function(uint8_t parameter_count, uint8_t return_count)
      : FunctionBase(parameter_count, return_count) {}

  // Appends an op-code for the given `Instruction I` template parameter,
  // followed by `Value`s for each of the passed arguments.
  template <internal::ContainedIn<typename Set::jasmin_instructions*> I,
            typename... Vs>
  constexpr OpCodeRange append(Vs... vs) requires(
      internal_function::ConvertibleArguments<
          internal::ExtractSignature<decltype(&I::execute)>, Vs...>) {
    return internal::FunctionBase::append(
        {Value(&I::template ExecuteImpl<Set>), Value(vs)...});
  }

  // Same as `append` above for instructions with no immediate values.
  template <internal::ContainedIn<typename Set::jasmin_instructions*> I>
  constexpr OpCodeRange append() requires(
      internal_instruction::ImmediateValueCount<I>() == 0) {
    return internal::FunctionBase::append(
        {Value(&I::template ExecuteImpl<Set>)});
  }

  // Appends an intsruction followed by space for `placeholder_count` values
  // which are left uninitialized. They may be initialized later via calls to
  // `Function<...>::set_value`. Returns the corresponding OpCodeRange.
  template <internal::ContainedIn<typename Set::jasmin_instructions*> I>
  constexpr OpCodeRange append_with_placeholders() {
    return internal::FunctionBase::append(
        Value(&I::template ExecuteImpl<Set>),
        internal_instruction::ImmediateValueCount<I>());
  }
};

}  // namespace jasmin

#endif  // JASMIN_FUNCTION_H
