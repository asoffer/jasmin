#ifndef JASMIN_INSTRUCTION_H
#define JASMIN_INSTRUCTION_H

#include <concepts>

#include "jasmin/call_stack.h"
#include "jasmin/instruction_pointer.h"
#include "jasmin/internal/attributes.h"
#include "jasmin/internal/function_base.h"
#include "jasmin/internal/type_traits.h"
#include "jasmin/value.h"
#include "jasmin/value_stack.h"

namespace jasmin {
namespace internal_instruction {

// Base class used solely to indicate that any struct inherting from it is an
// instruction set.
struct InstructionSetBase {};

// Implementation detail. A concept capturing valid return types for one way a
// type can adhere to the `Instruction` concept defined below.
template <typename T>
concept VoidOrSmallTrivialValue = std::is_void_v<T> or SmallTrivialValue<T>;

// Implementation detail. A concept capturing one way a type can adhere to the
// `Instruction` concept defined below.
template <typename Signature>
concept SignatureSatisfiesSimpleRequirements =
    VoidOrSmallTrivialValue<typename Signature::return_type> and
    Signature::invoke_with_argument_types([]<SmallTrivialValue... Ts>() {
      return true;
    });

// Implementation detail. A concept capturing one way a type can adhere to the
// `Instruction` concept defined below.
template <typename Signature>
concept SignatureSatisfiesGeneralRequirements =
    std::is_void_v<typename Signature::return_type> and
    Signature::invoke_with_argument_types(
        []<std::same_as<ValueStack &>, std::same_as<InstructionPointer &>,
           SmallTrivialValue... Ts>() { return true; });

// Implementation detail. A concept capturing that the signature for an
// `execute` static member function satisfies the requirements for the
// Instruction concept.
template <typename Signature>
concept SignatureSatisfiesRequirements =
    (SignatureSatisfiesSimpleRequirements<Signature> or
     SignatureSatisfiesGeneralRequirements<Signature>);

}  // namespace internal_instruction

// A concept indicating which types constitute instruction sets understandable
// by Jasmin's interpreter.
template <typename T>
concept InstructionSet =
    std::derived_from<T, internal_instruction::InstructionSetBase>;

// Forward declarations for instructions that need special treatement in
// Jasmin's interpreter and are built-in to every instruction set. Definitions
// appear below.
struct Return;
struct Call;

// Every instruction `Inst` executable as part of Jasmin's stack machine
// interpreter must publicly inherit from `StackMachineInstruction<Inst>`.
// Moreover, other than the builtin-instructions forward-declared above, they
// must also have a static member function `execute` that is not part of an
// overload set (so that `&Inst::execute` is well-formed) and this function must
// either:
//
//   (a) Return void and accept a `jasmin::ValueStack&`, then a
//   `jasmin::InstructionPointer&`, and then some number of arguments satisfying
//   whose types satisfy the `jasmin::SmallTrivialValue` concept.
//
//   (b) Accept some number of arguments satisfying `jasmin::SmallTrivialValue`
//   and
//       return either for or another type satisfying
//       `jasmin::SmallTrivialValue`.
//
template <typename Inst>
struct StackMachineInstruction {
 private:
  template <InstructionSet Set>
  static void ExecuteImpl(ValueStack &value_stack, InstructionPointer &ip,
                          CallStack &call_stack) {
    if constexpr (std::is_same_v<Inst, Call>) {
      auto const *f =
          value_stack.pop<internal_function_base::FunctionBase const *>();
      call_stack.push(f, value_stack.size(), ip);
      ip = f->entry();
      JASMIN_INTERNAL_TAIL_CALL return Set::InstructionFunction(ip->op_code())(
          value_stack, ip, call_stack);

    } else if constexpr (std::is_same_v<Inst, Return>) {
      // When a call instruction is executed, all the arguments are pushed onto
      // the stack followed by the to-be-called function.
      auto [start, end] = call_stack.erasable_range(value_stack.size());
      value_stack.erase(start, end);
      ip = call_stack.pop();
      ++ip;
      if (call_stack.empty()) {
        return;
      } else {
        JASMIN_INTERNAL_TAIL_CALL return Set::InstructionFunction(
            ip->op_code())(value_stack, ip, call_stack);
      }
    } else {
      using signature =
          internal_type_traits::ExtractSignature<decltype(&Inst::execute)>;
      if constexpr (internal_instruction::SignatureSatisfiesGeneralRequirements<signature>) {
        signature::invoke_with_argument_types(
            [&]<std::same_as<ValueStack &>, std::same_as<InstructionPointer &>,
                SmallTrivialValue... Ts>() {
              auto ip_copy = ip;
              // Brace-initialization forces the order of evaluation to be in
              // the order the elements appear in the list.
              std::apply(
                  Inst::execute,
                  std::tuple<ValueStack &, InstructionPointer &, Ts...>{
                      value_stack, ip, (ip_copy++)->value().as<Ts>()...});
            });
      } else {
        if constexpr (std::is_void_v<typename signature::return_type>) {
          signature::invoke_with_argument_types([&]<SmallTrivialValue... Ts>() {
            std::apply(Inst::execute, value_stack.pop_suffix<Ts...>());
          });
        } else {
          signature::invoke_with_argument_types([&]<SmallTrivialValue... Ts>() {
            value_stack.push(
                std::apply(Inst::execute, value_stack.pop_suffix<Ts...>()));
          });
        }
        ++ip;
      }
    }

    JASMIN_INTERNAL_TAIL_CALL return Set::InstructionFunction(ip->op_code())(
        value_stack, ip, call_stack);
  }
};

// Built-in instructions to every instruction-set.
struct Call : StackMachineInstruction<Call> {};
struct Return : StackMachineInstruction<Return> {};

// The `Instruction` concept indicates that a type `I` represents an instruction
// which Jasmin is capable of executing. Instructions must either be one of the
// builtin instructions `jasmin::Call` or `jasmin::Return`, or publicly inherit
// from `jasmin::StackMachineInstruction<I>` and have a static member function
// `execute` that is not part of an overload set, and that adheres to one of the
// following:
//
//   (a) Have the signature
//       `void execute(jasmin::ValueStack&, jasmin::InstructionPointer&)`.
//
//   (b) Accept some number of arguments satisfying `jasmin::SmallTrivialValue`
//   and
//       return either for or another type satisfying
//       `jasmin::SmallTrivialValue`.
//
template <typename I>
concept Instruction =
    (std::is_same_v<I, Call> or std::is_same_v<I, Return> or
     (std::derived_from<I, StackMachineInstruction<I>> and
      internal_instruction::SignatureSatisfiesRequirements<
          internal_type_traits::ExtractSignature<decltype(&I::execute)>>));

template <Instruction... Is>
struct MakeInstructionSet final : internal_instruction::InstructionSetBase {
  // Returns a `uint64_t` indicating the op-code for the given template
  // parameter instruction `I`.
  template <Instruction I>
  static constexpr uint64_t OpCodeFor() requires((std::is_same_v<I, Is> or
                                                  ...)) {
    constexpr size_t value = OpCodeForImpl<I>();
    return value;
  }

  static auto InstructionFunction(uint64_t op_code) { return table[op_code]; }

 private:
  static constexpr void (*table[sizeof...(Is)])(ValueStack &,
                                                InstructionPointer &,
                                                CallStack &) = {
      &Is::template ExecuteImpl<MakeInstructionSet>...};

  template <Instruction I>
  static constexpr uint64_t OpCodeForImpl() requires((std::is_same_v<I, Is> or
                                                      ...)) {
    // Because the fold-expression below unconditionally adds one to `i` on its
    // first evaluation, we start `i` at its maximum value and allow it to wrap
    // around.
    uint64_t i = std::numeric_limits<uint64_t>::max();
    static_cast<void>(((++i, std::is_same_v<I, Is>) or ...));
    return i;
  }
};

}  // namespace jasmin

#endif  // JASMIN_INSTRUCTION_H
