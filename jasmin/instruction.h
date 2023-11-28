#ifndef JASMIN_INSTRUCTION_H
#define JASMIN_INSTRUCTION_H

#include <concepts>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>

#include "jasmin/internal/call_stack.h"
#include "jasmin/internal/function_base.h"
#include "jasmin/internal/function_state.h"
#include "jasmin/internal/instruction_traits.h"
#include "jasmin/value.h"
#include "jasmin/value_stack.h"
#include "nth/base/attributes.h"
#include "nth/debug/debug.h"
#include "nth/meta/concepts.h"
#include "nth/meta/sequence.h"
#include "nth/meta/type.h"

namespace jasmin {

// Template from which all stack machine instructions must inherit. This
// template is intended to be used with the curiously recurring template
// pattern, so any instruction should be derived from `jasmin::Instruction`
// instantiated with itself bound to the template parameter. That is,
//
// ```
// struct MyInstruction : jasmin::Instruction<MyInstruction> { ... };
// ```
template <typename>
struct Instruction {
  template <typename>
  static void ExecuteImpl(Value *, size_t, Value const *, internal::FrameBase *,
                          uint64_t);
};

// Every instruction `Inst` executable as part of Jasmin's stack machine
// interpreter must publicly inherit from `jasmin::Instruction<Inst>`. Moreover,
// other than the builtin-instructions forward-declared above, they must also
// have a static member function named either `execute` or `consume` (but not
// both). This function must not be part of an overload set so that either
// `&Inst::execute` or `&Inst::consume` is well-formed). The function must
// accept as its first parameter a reference to `typename Inst::function_state`,
// if this type is well-formed. It must then accept a `std::span<jasmin::Value,
// N>` for some non-negative integer value of `N`, followed by any number of
// values whose type is convertible to `jasmin::Value`. The function`s return
// type must either be `void`, or be convertible to `jasmin::Value`.
template <typename I>
concept InstructionType = (std::derived_from<I, Instruction<I>> and
                           (internal::BuiltinInstruction<I>() or
                            internal::UserDefinedInstruction<I>));

// A concept indicating which types constitute instruction sets understandable
// by Jasmin's interpreter.
template <typename S>
concept InstructionSetType = std::derived_from<S, internal::InstructionSetBase>;

// Returns the number of immediate values passed to the instruction `I`.
template <typename I>
constexpr size_t ImmediateValueCount();

// Returns the number of parameters passed from the stack to the instruction
// `I`.
template <typename I>
constexpr size_t ParameterCount();

// Return `true` if the instruction consumes its input (i.e., is defined via
// `consume`) and `false` otherwise (i.e., is defined via `execute`).
template <typename I>
constexpr bool ConsumesInput();

// Return `true` if the instruction returns a value and `false` otherwise.
template <typename I>
constexpr bool ReturnsValue();

// Returns a `std::string_view` representing the name of the instruction. This
// name should not be considered or even unique amongst instructions and should
// no be relied upon for anything other than debugging.
template <typename I>
constexpr std::string InstructionName();

// `Call` is a built-in instructions, available automatically in every
// instruction set. It pops the top value off the stack, interprets it as a
// function pointer, and begins execution at that function's entry point.
struct Call : Instruction<Call> {
  using Specification = internal::CallSpec;

  static std::string debug(std::span<Value const, 1> immediate_values) {
    return "call " +
           std::to_string(immediate_values[0].as<Specification>().parameters) +
           " -> " +
           std::to_string(immediate_values[0].as<Specification>().returns);
  }
};

// `Jump` is a built-in instructions, available automatically in every
// instruction set. It accepts a single `std::ptrdiff_t` immediate value,
// increments the instruction pointer by that amount and resumes execution.
struct Jump : Instruction<Jump> {
  static std::string debug(std::span<Value const, 1> immediate_values) {
    ptrdiff_t n = immediate_values[0].as<ptrdiff_t>();
    if (n < 0) {
      return "jump -" + std::to_string(-n);
    } else {
      return "jump +" + std::to_string(n);
    }
  }
};

// `JumpIf` is a built-in instructions, available automatically in every
// instruction set. It accepts a single `std::ptrdiff_t` immediate value. It
// pops the top value off the stack and interprets it as a `bool`. If that bool
// is false, execution proceeds normally. Otherwise, the instruction pointer is
// incremented by the immediate value and execution resumes.
struct JumpIf : Instruction<JumpIf> {
  static std::string debug(std::span<Value const, 1> immediate_values) {
    ptrdiff_t n = immediate_values[0].as<ptrdiff_t>();
    if (n < 0) {
      return "jump-if -" + std::to_string(-n);
    } else {
      return "jump-if +" + std::to_string(n);
    }
  }
};

// `Return` is a built-in instructions, available automatically in every
// instruction set. It returns control back to the calling function at the
// instruction pointer immediately following the `Call` instruction that invoked
// this function.
struct Return : Instruction<Return> {
  static constexpr std::string_view debug(std::span<Value const, 0>) {
    return "return";
  }
};

namespace internal {

// Constructs an InstructionSet type from a list of instructions. Does no
// checking to validate that `Is` do not contain repeats.
template <InstructionType... Is>
struct MakeInstructionSet : InstructionSetBase {
  static constexpr auto instructions = nth::type_sequence<Is...>;
};

// Given a list of `Instruction`s or `InstructionSet`s,
// `FlattenInstructionList` computes the list of all instructions in the list,
// in an InstructionSet in the list transitively.
constexpr auto FlattenInstructionList(nth::Sequence auto unprocessed,
                                      nth::Sequence auto processed) {
  if constexpr (unprocessed.empty()) {
    return processed;
  } else {
    constexpr auto head = unprocessed.head();
    constexpr auto tail = unprocessed.tail();
    if constexpr (processed.reduce(
                      [&](auto... vs) { return ((vs == head) or ...); })) {
      return FlattenInstructionList(tail, processed);
    } else if constexpr (InstructionType<nth::type_t<head>>) {
      // TODO: Is this a bug in Clang? `tail` does not work but
      // `decltype(tail){}` does.
      return FlattenInstructionList(decltype(tail){},
                                    processed + nth::sequence<head>);
    } else {
      return FlattenInstructionList(tail + nth::type_t<head>::instructions,
                                    processed);
    }
  }
}

}  // namespace internal

template <typename... Is>
using MakeInstructionSet = nth::type_t<
    internal::FlattenInstructionList(
        /*unprocessed=*/nth::type_sequence<Is...>,
        /*processed=*/nth::type_sequence<Call, Jump, JumpIf, Return>)
        .reduce([](auto... vs) {
          return nth::type<internal::MakeInstructionSet<nth::type_t<vs>...>>;
        })>;

// Implementation details.

template <typename Inst>
template <typename Set>
void Instruction<Inst>::ExecuteImpl(Value *value_stack_head, size_t vs_left,
                                    Value const *ip,
                                    internal::FrameBase *call_stack,
                                    uint64_t cap_and_left) {
  using frame_type = internal::Frame<typename internal::FunctionState<Set>>;
  constexpr auto inst_type = nth::type<Inst>;
  if constexpr (inst_type == nth::type<Call>) {
    if ((cap_and_left & 0xffffffff) == 0) [[unlikely]] {
      return internal::ReallocateCallStack<sizeof(frame_type)>(
          value_stack_head, vs_left, ip, call_stack, cap_and_left);
    } else {
      --value_stack_head;
      (++call_stack)->ip = ip;
      ip = value_stack_head->as<internal::FunctionBase const *>()->entry();
      NTH_ATTRIBUTE(tailcall)
      return ip->as<internal::exec_fn_type>()(value_stack_head, vs_left + 1, ip,
                                              call_stack, cap_and_left - 1);
    }
  } else if constexpr (inst_type == nth::type<Jump>) {
    ip += (ip + 1)->as<ptrdiff_t>();
    NTH_ATTRIBUTE(tailcall)
    return ip->as<internal::exec_fn_type>()(value_stack_head, vs_left, ip,
                                            call_stack, cap_and_left);
  } else if constexpr (inst_type == nth::type<JumpIf>) {
    --value_stack_head;
    if (value_stack_head->as<bool>()) {
      ip += (ip + 1)->as<ptrdiff_t>();
      NTH_ATTRIBUTE(tailcall)
      return ip->as<internal::exec_fn_type>()(value_stack_head, vs_left + 1, ip,
                                              call_stack, cap_and_left);
    } else {
      ip += 2;
      NTH_ATTRIBUTE(tailcall)
      return ip->as<internal::exec_fn_type>()(value_stack_head, vs_left + 1, ip,
                                              call_stack, cap_and_left);
    }
  } else if constexpr (inst_type == nth::type<Return>) {
    ip = (call_stack--)->ip + 2;
    ++cap_and_left;
    if (internal::EmptyCallStack(cap_and_left)) [[unlikely]] {
      const_cast<Value &>(*call_stack->ip) = vs_left;
      --call_stack;
      const_cast<Value &>(*call_stack->ip) = value_stack_head;
      operator delete(call_stack);
      return;
    } else {
      NTH_ATTRIBUTE(tailcall)
      return ip->as<internal::exec_fn_type>()(value_stack_head, vs_left, ip,
                                              call_stack, cap_and_left);
    }
  } else {
    constexpr auto inst      = internal::InstructionFunctionPointer<Inst>();
    constexpr auto inst_type = internal::InstructionFunctionType<Inst>();
    constexpr bool HasFunctionState = internal::HasFunctionState<Inst>;
    constexpr bool ReturnsVoid = (inst_type.return_type() == nth::type<void>);
    constexpr auto parameter_types =
        inst_type.parameters().template drop<1 + HasFunctionState>();
    using span_type =
        nth::type_t<inst_type.parameters().template get<HasFunctionState>()>;
    constexpr size_t ValueCount = span_type::extent;

    if constexpr (ValueCount == 0 and not ReturnsVoid) {
      if (vs_left == 0) [[unlikely]] {
        NTH_ATTRIBUTE(tailcall)
        return internal::ReallocateValueStack(value_stack_head, vs_left, ip,
                                              call_stack, cap_and_left);
      }
    }

#define JASMIN_INTERNAL_GET(p, Ns)                                             \
  (p + Ns)->template as<nth::type_t<parameter_types.template get<Ns>()>>()

    if constexpr (HasFunctionState) {
      auto &fn_state = std::get<typename Inst::function_state>(
          static_cast<frame_type *>(call_stack)->state);

      [&]<size_t... Ns>(std::index_sequence<Ns...>) {
        if constexpr (ReturnsVoid) {
          inst(fn_state, span_type(value_stack_head - ValueCount, ValueCount),
               JASMIN_INTERNAL_GET((ip + 1), Ns)...);
        } else {
          *(value_stack_head - (ConsumesInput<Inst>() ? ValueCount : 0)) = inst(
              fn_state, span_type(value_stack_head - ValueCount, ValueCount),
              JASMIN_INTERNAL_GET((ip + 1), Ns)...);
        }
      }
      (std::make_index_sequence<ImmediateValueCount<Inst>()>{});
    } else {
      [&]<size_t... Ns>(std::index_sequence<Ns...>) {
        if constexpr (ReturnsVoid) {
          inst(span_type(value_stack_head - ValueCount, ValueCount),
               JASMIN_INTERNAL_GET((ip + 1), Ns)...);
        } else {
          *(value_stack_head - (ConsumesInput<Inst>() ? ValueCount : 0)) =
              inst(span_type(value_stack_head - ValueCount, ValueCount),
                   JASMIN_INTERNAL_GET((ip + 1), Ns)...);
        }
      }
      (std::make_index_sequence<ImmediateValueCount<Inst>()>{});
    }
#undef JASMIN_INTERNAL_GET

    if constexpr (not ReturnsVoid) {
      ++value_stack_head;
      --vs_left;
    }

    if constexpr (ConsumesInput<Inst>()) {
      value_stack_head -= ValueCount;
      vs_left += ValueCount;
    }

    ip += ImmediateValueCount<Inst>() + 1;
    NTH_ATTRIBUTE(tailcall)
    return ip->as<internal::exec_fn_type>()(value_stack_head, vs_left, ip,
                                            call_stack, cap_and_left);
  }
}

template <typename I>
constexpr size_t ImmediateValueCount() {
  if constexpr (nth::any_of<I, Return>) {
    return 0;
  } else if constexpr (nth::any_of<I, Call, Jump, JumpIf>) {
    return 1;
  } else {
    return internal::InstructionFunctionType<I>().parameters().size() -
           (internal::HasFunctionState<I> ? 2 : 1);
  }
}

template <typename I>
constexpr size_t ParameterCount() {
  if constexpr (nth::any_of<I, Jump, Return>) {
    return 0;
  } else if constexpr (nth::any_of<I, Call, JumpIf>) {
    return 1;
  } else {
    return nth::type_t<
        internal::InstructionFunctionType<I>()
            .parameters()
            .template get<internal::HasFunctionState<I>>()>::extent;
  }
}

template <typename I>
constexpr bool ConsumesInput() {
  if constexpr (nth::any_of<I, JumpIf>) {
    return false;
  } else {
    return requires { &I::consume; };
  }
}

template <typename I>
constexpr bool ReturnsValue() {
  return internal::InstructionFunctionType<I>().return_type() !=
         nth::type<void>;
}

template <typename I>
constexpr std::string InstructionName() {
  return std::string(nth::type<I>.name());
}

}  // namespace jasmin

#endif  // JASMIN_INSTRUCTION_H
