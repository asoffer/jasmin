#ifndef JASMIN_TESTING_H
#define JASMIN_TESTING_H

#include "jasmin/instruction.h"

namespace jasmin {

template <InstructionType Inst>
requires(not internal::HasFunctionState<Inst>)  //
    void ExecuteInstruction(ValueStack &value_stack, auto... immediates) {
  constexpr bool consumes  = requires { &Inst::consume; };
  constexpr auto inst      = internal::InstructionFunctionPointer<Inst>();
  constexpr auto signature = internal::InstructionFunctionType<Inst>();
  if constexpr (internal::ImmediateValueDetermined(signature.parameters())) {
    void *p;
    (void)((p = std::addressof(immediates), true) or ...);
    auto [ins, outs] = *static_cast<InstructionSpecification *>(p);

    std::span<Value> in_span(value_stack.head() - ins, ins);
    std::vector<Value> v(outs, Value::Uninitialized());
    std::apply([&](auto, auto... args) { inst(in_span, v, args...); },
               std::tuple(immediates...));
    for (auto value : v) { value_stack.push(value); }

  } else {
    constexpr auto parameter_count =
        nth::type_t<signature.parameters().template get<0>()>::extent;
    std::span<Value, parameter_count> span(value_stack.head() - parameter_count,
                                           parameter_count);
    if constexpr (signature.return_type() == nth::type<void>) {
      inst(span, immediates...);
      if constexpr (consumes) {
        value_stack.erase(value_stack.size() - parameter_count,
                          value_stack.size());
      }
    } else {
      auto v = inst(span, immediates...);
      if constexpr (consumes) {
        value_stack.erase(value_stack.size() - parameter_count,
                          value_stack.size());
      }
      value_stack.push(v);
    }
  }
}

template <InstructionType Inst>
requires(internal::HasFunctionState<Inst>)  //
    void ExecuteInstruction(ValueStack &value_stack,
                            typename Inst::function_state &state,
                            auto... immediates) {
  constexpr bool consumes  = requires { &Inst::consume; };
  constexpr auto inst      = internal::InstructionFunctionPointer<Inst>();
  constexpr auto signature = internal::InstructionFunctionType<Inst>();
  if constexpr (internal::ImmediateValueDetermined(
                    signature.parameters().template drop<1>())) {
    void *p;
    (void)((p = std::addressof(immediates), true) or ...);
    auto [ins, outs] = *static_cast<InstructionSpecification *>(p);

    std::span<Value> in_span(value_stack.head() - ins, ins);
    std::vector<Value> v(outs, Value::Uninitialized());
    std::apply([&](auto, auto... args) { inst(state, in_span, v, args...); },
               std::tuple(immediates...));
    for (auto value : v) { value_stack.push(value); }

  } else {
    constexpr auto parameter_count =
        nth::type_t<signature.parameters().template get<1>()>::extent;
    std::span<Value, parameter_count> span(value_stack.head() - parameter_count,
                                           parameter_count);
    if constexpr (signature.return_type() == nth::type<void>) {
      inst(state, span, immediates...);
      if constexpr (consumes) {
        value_stack.erase(value_stack.size() - parameter_count,
                          value_stack.size());
      }
    } else {
      auto v = inst(state, span, immediates...);
      if constexpr (consumes) {
        value_stack.erase(value_stack.size() - parameter_count,
                          value_stack.size());
      }
      value_stack.push(v);
    }
  }
}

}  // namespace jasmin

#endif  // JASMIN_TESTING_H
