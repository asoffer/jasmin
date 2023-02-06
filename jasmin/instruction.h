#ifndef JASMIN_INSTRUCTION_H
#define JASMIN_INSTRUCTION_H

#include <concepts>
#include <limits>
#include <stack>
#include <type_traits>

#include "jasmin/call_stack.h"
#include "jasmin/execution_state.h"
#include "jasmin/instruction_pointer.h"
#include "jasmin/internal/attributes.h"
#include "jasmin/internal/function_base.h"
#include "jasmin/internal/type_traits.h"
#include "jasmin/value.h"
#include "jasmin/value_stack.h"
#include "nth/meta/concepts.h"
#include "nth/meta/sequence.h"
#include "nth/meta/type.h"

namespace jasmin {

template <typename T>
concept HasFunctionState = requires {
  typename T::JasminFunctionState;
};

namespace internal {

using exec_fn_type = void (*)(ValueStack &, InstructionPointer &, CallStack &,
                              void *);

template <typename Signature>
concept HasValueStack =
    std::is_same_v<void (*)(ValueStack &),
                   decltype(Signature::invoke_with_argument_types(
                       []<typename T, typename...>() -> void (*)(T) {
                         return nullptr;
                       }))>;

// Base class used solely to indicate that any struct inherting from it is an
// instruction set.
struct InstructionSetBase {};

template <typename Signature, typename I, bool HasExecState, bool HasFuncState>
constexpr bool ValidSignatureWithImmediatesImpl() {
  if (not std::is_void_v<typename Signature::return_type>) { return false; }
  if (not Signature::invoke_with_argument_types([]<typename T, typename...> {
        return std::is_same_v<T, ValueStack &>;
      })) {
    return false;
  }

  if constexpr (HasExecState) {
    if constexpr (HasFuncState) {
      return Signature::invoke_with_argument_types(
          []<typename VS, typename ES, typename FS, typename... Ts>() {
            return std::is_same_v<FS, typename I::JasminExecutionState &> and
                   std::is_same_v<FS, typename I::JasminFunctionState &> and
                   not(std::is_reference_v<Ts> or ...) and
                   (std::convertible_to<Ts, Value> and ...);
          });
    } else {
      return Signature::invoke_with_argument_types(
          []<typename VS, typename ES, typename... Ts>() {
            return std::is_same_v<ES, typename I::JasminExecutionState &> and
                   not(std::is_reference_v<Ts> or ...) and
                   (std::convertible_to<Ts, Value> and ...);
          });
    }
  } else {
    if constexpr (HasFuncState) {
      return Signature::invoke_with_argument_types(
          []<typename VS, typename FS, typename... Ts>() {
            return std::is_same_v<FS, typename I::JasminFunctionState &> and
                   not(std::is_reference_v<Ts> or ...) and
                   (std::convertible_to<Ts, Value> and ...);
          });

    } else {
      return Signature::invoke_with_argument_types(
          []<typename VS, typename... Ts>() {
            return not(std::is_reference_v<Ts> or ...) and
                   (std::convertible_to<Ts, Value> and ...);
          });
    }
  }
}

template <typename Signature, typename I, bool HasExecState, bool HasFuncState>
constexpr bool ValidSignatureWithoutImmediatesImpl() {
  using return_type = typename Signature::return_type;
  if (not std::is_void_v<return_type> and
      not std::convertible_to<return_type, Value>) {
    return false;
  }

  if constexpr (HasExecState) {
    if constexpr (HasFuncState) {
      return Signature::invoke_with_argument_types(
          []<typename ES, typename FS, typename... Ts>() {
            return std::is_same_v<ES, typename I::JasminExecutionState &> and
                   std::is_same_v<FS, typename I::JasminFunctionState &> and
                   not(std::is_reference_v<Ts> or ...) and
                   (std::convertible_to<Ts, Value> and ...);
          });
    } else {
      return Signature::invoke_with_argument_types(
          []<typename ES, typename... Ts>() {
            return std::is_same_v<ES, typename I::JasminExecutionState &> and
                   not(std::is_reference_v<Ts> or ...) and
                   (std::convertible_to<Ts, Value> and ...);
          });
    }
  } else {
    if constexpr (HasFuncState) {
      return Signature::invoke_with_argument_types(
          []<typename FS, typename... Ts>() {
            return std::is_same_v<FS, typename I::JasminFunctionState &> and
                   not(std::is_reference_v<Ts> or ...) and
                   (std::convertible_to<Ts, Value> and ...);
          });

    } else {
      return Signature::invoke_with_argument_types([]<typename... Ts>() {
        return not(std::is_reference_v<Ts> or ...) and
               (std::convertible_to<Ts, Value> and ...);
      });
    }
  }
}

// Implementation detail. A concept capturing that the `execute` static member
// function of the instruction adheres to one of the supported signatures.
template <typename I,
          typename Signature = ExtractSignature<decltype(&I::execute)>,
          bool HasExecState  = HasExecutionState<I>,
          bool HasFuncState  = HasFunctionState<I>>
concept HasValidSignature =
    ((HasValueStack<Signature> and
      ValidSignatureWithImmediatesImpl<Signature, I, HasExecState,
                                       HasFuncState>()) or
     (not HasValueStack<Signature> and
      ValidSignatureWithoutImmediatesImpl<Signature, I, HasExecState,
                                          HasFuncState>()));

// A list of all types required to represent the state of all functions in
// the instruction set `Set`.
template <typename Set>
constexpr auto FunctionStateList =
    Set::instructions
        .template transform<[](auto t) {
          using T = nth::type_t<t>;
          if constexpr (HasFunctionState<T>) {
            return nth::type<typename T::JasminFunctionState>;
          } else {
            return nth::type<void>;
          }
        }>()
        .unique()
        .template filter<[](auto t) { return t != nth::type<void>; }>();

}  // namespace internal

// A concept indicating which types constitute instruction sets understandable
// by Jasmin's interpreter.
template <typename T>
concept InstructionSet = std::derived_from<T, internal::InstructionSetBase>;

// A type-function accepting an instruction set and returning a type sufficient
// to hold all state required by all instructions in `Set`, or `void` if all
// instructions in `Set` are stateless.
template <InstructionSet Set>
using FunctionStateStack = nth::type_t<[](auto state_list) {
  if constexpr (state_list.empty()) {
    return nth::type<void>;
  } else {
    return state_list.reduce([](auto... ts) {
      return nth::type<std::stack<std::tuple<nth::type_t<ts>...>>>;
    });
  }
}(internal::FunctionStateList<Set>)>;

namespace internal {

template <typename FuncStateStack>
struct FuncStateImpl {
  FuncStateStack function_state_stack;
};

template <>
struct FuncStateImpl<void> {};

template <typename ExecutionState, typename FuncState>
struct StateImpl : FuncStateImpl<FuncState> {
  static constexpr bool has_function_state = not std::is_void_v<FuncState>;

  ExecutionState *exec_state;
};

template <InstructionSet Set>
using State = StateImpl<::jasmin::ExecutionState<Set>, FunctionStateStack<Set>>;

}  // namespace internal

// Forward declarations for instructions that need special treatement in
// Jasmin's interpreter and are built-in to every instruction set. Definitions
// appear below.
struct Call;
struct Jump;
struct JumpIf;
struct Return;

// Every instruction `Inst` executable as part of Jasmin's stack machine
// interpreter must publicly inherit from `StackMachineInstruction<Inst>`.
// Moreover, other than the builtin-instructions forward-declared above, they
// must also have a static member function `execute` that is not part of an
// overload set (so that `&Inst::execute` is well-formed) and this function
// adhere to one of the following:
//
//   (a) Returns void and accepts a `jasmin::ValueStack&`, then a mutable
//       reference to `typename I::JasminExecutionState` (if and only if that
//       type syntactically valid), then a mutable reference to
//       `typename I::JasminFunctionState` (if and only if that type
//       syntactically valid), and then some number of arguments convertible to
//       `Value`. If present the `typename I::JasminExecutionState&` parameter
//       is a reference to state shared throughout the entire execution. If
//       present the `typename I::JasminFunctionState&` parameter is a reference
//       to state shared during a function's execution. The arguments are
//       intperpreted as the immediate values for the instruction. The void
//       return type indicates that execution should fall through to the
//       following instruction.
//
//   (b) Accepts a mutable reference to `typename I::JasminExecutionState` (if
//       and only if that type syntactically valid), then a mutable reference to
//       `typename I::JasminFunctionState` (if and only if that type
//       syntactically valid), and then some number of arguments convertible to
//       `Value`. The function may return `void` or another type convertible to
//       `Value`. If present the `typename I::JasminExecutionState&` parameter
//       is a reference to state shared throughout the entire execution. If
//       present the `typename I::JasminFunctionState&` parameter is a reference
//       to state shared during a function's execution. The arguments are to be
//       popped from the value stack, and the returned value, if any, will be
//       pushed onto the value stack. Execution will fall through to the
//       following instruction.
//
template <typename Inst>
struct StackMachineInstruction {
 private:
  template <InstructionSet Set>
  static void ExecuteImpl(ValueStack &value_stack, InstructionPointer &ip,
                          CallStack &call_stack, void *state_void_ptr) {
    auto *state = static_cast<internal::State<Set> *>(state_void_ptr);
    if constexpr (std::is_same_v<Inst, Call>) {
      auto const *f = value_stack.pop<internal::FunctionBase const *>();
      call_stack.push(f, ip);
      ip = f->entry();
      if constexpr (internal::State<Set>::has_function_state) {
        state->function_state_stack.emplace();
      }
      JASMIN_INTERNAL_TAIL_CALL return ip->as<internal::exec_fn_type>()(
          value_stack, ip, call_stack, state);

    } else if constexpr (std::is_same_v<Inst, Jump>) {
      ip += (ip + 1)->as<ptrdiff_t>();

      JASMIN_INTERNAL_TAIL_CALL return ip->as<internal::exec_fn_type>()(
          value_stack, ip, call_stack, state);

    } else if constexpr (std::is_same_v<Inst, JumpIf>) {
      if (value_stack.pop<bool>()) {
        ip += (ip + 1)->as<ptrdiff_t>();
      } else {
        ip += 2;
      }

      JASMIN_INTERNAL_TAIL_CALL return ip->as<internal::exec_fn_type>()(
          value_stack, ip, call_stack, state);

    } else if constexpr (std::is_same_v<Inst, Return>) {
      ip = call_stack.pop();

      if constexpr (internal::State<Set>::has_function_state) {
        state->function_state_stack.pop();
      }
      ++ip;
      if (call_stack.empty()) [[unlikely]] {
        return;
      } else {
        JASMIN_INTERNAL_TAIL_CALL return ip->as<internal::exec_fn_type>()(
            value_stack, ip, call_stack, state);
      }
    } else {
      using signature = internal::ExtractSignature<decltype(&Inst::execute)>;

      constexpr bool ES = HasExecutionState<Inst>;
      constexpr bool FS = HasFunctionState<Inst>;
      constexpr bool VS = internal::HasValueStack<signature>;
      constexpr bool RV = std::is_void_v<typename signature::return_type>;
      static_assert(RV or not VS);

      if constexpr (VS and ES and FS) {
        signature::invoke_with_argument_types(
            [&]<std::same_as<ValueStack &>,
                std::same_as<typename Inst::JasminExecutionState &>,
                std::same_as<typename Inst::JasminFunctionState &>,
                std::convertible_to<Value>... Ts>() {
              // Brace-initialization forces the order of evaluation to be in
              // the order the elements appear in the list.
              std::apply(
                  Inst::execute,
                  std::tuple<ValueStack &,
                             typename Inst::JasminExecutionState &, Ts...>{
                      value_stack,
                      state->exec_state
                          ->template get<typename Inst::JasminExecutionState>(),
                      std::get<typename Inst::JasminFunctionState>(
                          state->function_state_stack.top()),
                      (++ip)->as<Ts>()...});
            });
      } else if constexpr (VS and ES and not FS) {
        signature::invoke_with_argument_types(
            [&]<std::same_as<ValueStack &>,
                std::same_as<typename Inst::JasminExecutionState &>,
                std::convertible_to<Value>... Ts>() {
              // Brace-initialization forces the order of evaluation to be in
              // the order the elements appear in the list.
              std::apply(
                  Inst::execute,
                  std::tuple<ValueStack &,
                             typename Inst::JasminExecutionState &, Ts...>{
                      value_stack,
                      state->exec_state
                          ->template get<typename Inst::JasminExecutionState>(),
                      (++ip)->as<Ts>()...});
            });
      } else if constexpr (VS and not ES and FS) {
        signature::invoke_with_argument_types(
            [&]<std::same_as<ValueStack &>,
                std::same_as<typename Inst::JasminFunctionState &>,
                std::convertible_to<Value>... Ts>() {
              // Brace-initialization forces the order of evaluation to be in
              // the order the elements appear in the list.
              std::apply(
                  Inst::execute,
                  std::tuple<ValueStack &, typename Inst::JasminFunctionState &,
                             Ts...>{
                      value_stack,
                      std::get<typename Inst::JasminFunctionState>(
                          state->function_state_stack.top()),
                      (++ip)->as<Ts>()...});
            });
      } else if constexpr (VS and not ES and not FS) {
        signature::
            invoke_with_argument_types([&]<std::same_as<ValueStack &>,
                                           std::convertible_to<Value>... Ts>() {
              // Brace-initialization forces the order of evaluation to be in
              // the order the elements appear in the list.
              std::apply(Inst::execute, std::tuple<ValueStack &, Ts...>{
                                            value_stack, (++ip)->as<Ts>()...});
            });
      } else if constexpr (RV and not VS and ES and FS) {
        signature::invoke_with_argument_types(
            [&]<std::same_as<typename Inst::JasminExecutionState &>,
                std::same_as<typename Inst::JasminFunctionState &>,
                std::convertible_to<Value>... Ts>() {
              std::apply(
                  [&](auto... values) {
                    Inst::execute(state->exec_state->template get<
                                      typename Inst::JasminExecutionState>(),
                                  std::get<typename Inst::JasminFunctionState>(
                                      state->function_state_stack.top()),
                                  values...);
                  },
                  value_stack.pop_suffix<Ts...>());
            });
      } else if constexpr (RV and not VS and ES and not FS) {
        signature::invoke_with_argument_types(
            [&]<std::same_as<typename Inst::JasminExecutionState &>,
                std::convertible_to<Value>... Ts>() {
              std::apply(
                  [&](auto... values) {
                    Inst::execute(state->exec_state->template get<
                                      typename Inst::JasminExecutionState>(),
                                  values...);
                  },
                  value_stack.pop_suffix<Ts...>());
            });
      } else if constexpr (RV and not VS and not ES and FS) {
        signature::invoke_with_argument_types(
            [&]<std::same_as<typename Inst::JasminFunctionState &>,
                std::convertible_to<Value>... Ts>() {
              std::apply(
                  [&](auto... values) {
                    Inst::execute(std::get<typename Inst::JasminFunctionState>(
                                      state->function_state_stack.top()),
                                  values...);
                  },
                  value_stack.pop_suffix<Ts...>());
            });
      } else if constexpr (RV and not VS and not ES and not FS) {
        signature::
            invoke_with_argument_types([&]<std::convertible_to<Value>... Ts>() {
              std::apply(Inst::execute, value_stack.pop_suffix<Ts...>());
            });
      } else if constexpr (not RV and not VS and ES and FS) {
        signature::invoke_with_argument_types(
            [&]<std::same_as<typename Inst::JasminExecutionState &>,
                std::same_as<typename Inst::JasminFunctionState &>,
                std::convertible_to<Value>... Ts>() {
              value_stack.call_on_suffix<&Inst::execute, Ts...>(
                  state->exec_state
                      ->template get<typename Inst::JasminExecutionState>(),
                  std::get<typename Inst::JasminFunctionState>(
                      state->function_state_stack.top()));
            });
      } else if constexpr (not RV and not VS and ES and not FS) {
        signature::invoke_with_argument_types(
            [&]<std::same_as<typename Inst::JasminExecutionState &>,
                std::convertible_to<Value>... Ts>() {
              value_stack.call_on_suffix<&Inst::execute, Ts...>(
                  state->exec_state
                      ->template get<typename Inst::JasminExecutionState>());
            });
      } else if constexpr (not RV and not VS and not ES and FS) {
        signature::invoke_with_argument_types(
            [&]<std::same_as<typename Inst::JasminFunctionState &>,
                std::convertible_to<Value>... Ts>() {
              value_stack.call_on_suffix<&Inst::execute, Ts...>(
                  std::get<typename Inst::JasminFunctionState>(
                      state->function_state_stack.top()));
            });
      } else if constexpr (not RV and not VS and not ES and not FS) {
        signature::
            invoke_with_argument_types([&]<std::convertible_to<Value>... Ts>() {
              value_stack.call_on_suffix<&Inst::execute, Ts...>();
            });
      }
      ++ip;
    }

    JASMIN_INTERNAL_TAIL_CALL return ip->as<internal::exec_fn_type>()(
        value_stack, ip, call_stack, state);
  }
};

// Built-in instructions to every instruction-set.
struct Call : StackMachineInstruction<Call> {};
struct Jump : StackMachineInstruction<Jump> {};
struct JumpIf : StackMachineInstruction<JumpIf> {};
struct Return : StackMachineInstruction<Return> {};

// The `Instruction` concept indicates that a type `I` represents an instruction
// which Jasmin is capable of executing. Instructions must either be one of the
// builtin instructions `jasmin::Call`, `jasmin::Jump`, `jasmin::JumpIf`, or
// `jasmin::Return`, or publicly inherit from
// `jasmin::StackMachineInstruction<I>` and have a static member function
// `execute` that is not part of an overload set (so that `&Inst::execute` is
// well-formed) and this function adhere to one of the following:
//
//   (a) Returns void and accepts a `jasmin::ValueStack&`, then a mutable
//       reference to `typename I::JasminExecutionState` (if and only if that
//       type syntactically valid), then a mutable reference to
//       `typename I::JasminFunctionState` (if and only if that type
//       syntactically valid), and then some number of arguments convertible to
//       `Value`. If present the `typename I::JasminExecutionState&` parameter
//       is a reference to state shared throughout the entire execution. If
//       present the `typename I::JasminFunctionState&` parameter is a reference
//       to state shared during a function's execution. The arguments are
//       intperpreted as the immediate values for the instruction. The void
//       return type indicates that execution should fall through to the
//       following instruction.
//
//   (b) Accepts a mutable reference to `typename I::JasminExecutionState` (if
//       and only if that type syntactically valid), then a mutable reference to
//       `typename I::JasminFunctionState` (if and only if that type
//       syntactically valid), and then some number of arguments convertible to
//       `Value`. The function may return `void` or another type convertible to
//       `Value`. If present the `typename I::JasminExecutionState&` parameter
//       is a reference to state shared throughout the entire execution. If
//       present the `typename I::JasminFunctionState&` parameter is a reference
//       to state shared during a function's execution. The arguments are to be
//       popped from the value stack, and the returned value, if any, will be
//       pushed onto the value stack. Execution will fall through to the
//       following instruction.
//
template <typename I>
concept Instruction = (nth::any_of<I, Call, Jump, JumpIf, Return> or
                       (std::derived_from<I, StackMachineInstruction<I>> and
                        internal::HasValidSignature<I>));

namespace internal {
template <typename I>
concept InstructionOrInstructionSet = Instruction<I> or InstructionSet<I>;

// Constructs an InstructionSet type from a list of instructions. Does no
// checking to validate that `Is` do not contain repeats.
template <Instruction... Is>
struct MakeInstructionSet : InstructionSetBase {
  using self_type                    = MakeInstructionSet;
  static constexpr auto instructions = nth::type_sequence<Is...>;

  // Returns the number of instructions in the instruction set.
  static constexpr size_t size() { return sizeof...(Is); }

  // Returns a `uint64_t` indicating the op-code for the given template
  // parameter instruction `I`.
  template <nth::any_of<Is...> I>
  static constexpr uint64_t OpCodeFor() {
    constexpr size_t value = OpCodeForImpl<I>();
    return value;
  }

  static auto InstructionFunction(uint64_t op_code) {
    JASMIN_INTERNAL_DEBUG_ASSERT(op_code < sizeof...(Is),
                                 "Out-of-bounds op-code");
    return table[op_code];
  }

 private:
  static constexpr exec_fn_type table[sizeof...(Is)] = {
      &Is::template ExecuteImpl<MakeInstructionSet>...};

  template <nth::any_of<Is...> I>
  static constexpr uint64_t OpCodeForImpl() {
    // Because the fold-expression below unconditionally adds one to `i` on its
    // first evaluation, we start `i` at its maximum value and allow it to wrap
    // around.
    uint64_t i = std::numeric_limits<uint64_t>::max();
    static_cast<void>(((++i, std::is_same_v<I, Is>) or ...));
    return i;
  }
};

// Given a list of `Instruction`s or `InstructionSet`s, `FlattenInstructionList`
// computes the list of all instructions in the list, in an InstructionSet in
// the list transitively.
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
    } else if constexpr (Instruction<nth::type_t<head>>) {
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

constexpr auto BuiltinInstructionList =
    nth::type_sequence<Call, Jump, JumpIf, Return>;

}  // namespace internal

template <internal::InstructionOrInstructionSet... Is>
using MakeInstructionSet = nth::type_t<
    internal::FlattenInstructionList(
        /*unprocessed=*/nth::type_sequence<Is...>,
        /*processed=*/internal::BuiltinInstructionList)
        .reduce([](auto... vs) {
          return nth::type<internal::MakeInstructionSet<nth::type_t<vs>...>>;
        })>;

namespace internal {

template <Instruction I>
constexpr size_t ImmediateValueCount() {
  if constexpr (nth::any_of<I, Call, Return>) {
    return 0;
  } else if constexpr (nth::any_of<I, Jump, JumpIf>) {
    return 1;
  } else {
    using signature = ExtractSignature<decltype(&I::execute)>;

    size_t immediate_value_count = signature::invoke_with_argument_types(
        []<typename... Ts>() { return sizeof...(Ts); });

    constexpr bool ES = HasExecutionState<I>;
    constexpr bool FS = HasFunctionState<I>;
    constexpr bool VS = internal::HasValueStack<signature>;

    if (not VS) { return 0; }
    --immediate_value_count;  // Ignore the `ValueStack&` parameter.
    if (ES) { --immediate_value_count; }
    if (FS) { --immediate_value_count; }
    return immediate_value_count;
  }
}

}  // namespace internal
}  // namespace jasmin

#endif  // JASMIN_INSTRUCTION_H
