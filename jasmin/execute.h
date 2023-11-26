#ifndef JASMIN_EXECUTE_H
#define JASMIN_EXECUTE_H

#include <initializer_list>
#include <type_traits>

#include "jasmin/function.h"
#include "jasmin/instruction.h"
#include "jasmin/value.h"
#include "jasmin/value_stack.h"
#include "nth/debug/debug.h"

namespace jasmin {

// Executes the given function `f` with an initial stack of values given by the
// object referenced by `value_stack`. `value_stack` is modified in place.
template <InstructionSetType Set>
void Execute(Function<Set> const &f, ValueStack &value_stack) {
  Value head = value_stack.end();
  Value left = value_stack.space_remaining();
  internal::Ignore(value_stack);

  using frame_type = internal::Frame<typename internal::FunctionState<Set>>;
  frame_type *call_stack =
      static_cast<frame_type *>(operator new(sizeof(frame_type) * 8));
  Value const *ip = f.entry();
  // These aren't actually instruction pointers, but we're using them during
  // execution to store information about the ValueStack so it can be
  // reconstituted after the fact.
  call_stack[0].ip = &head;
  call_stack[1].ip = &left;
  call_stack[2].ip = ip;

  ip->as<internal::exec_fn_type>()(head.as<Value *>(), left.as<size_t>(), ip,
                                   &call_stack[2], 0x00000008'00000005);
  value_stack = internal::Concoct(head.as<Value *>(), left.as<size_t>());
}

// Interprets the given function `f` with arguments provided in the
// initializer-list `arguments`. Return values are written to the passed-in
// references `return_values...`. In general, behavior is undefined if the
// number of return values of `f` is not `sizeof...(return_values)`, or if the
// values returned to not match the types
// `std::decay_t<decltype(return_values)...>`. However, in debug and hardened
// builds (see `//jasmin/configuration` for details) the behavior is defined to
// report an error message to `stderr` and abort program execution.
template <InstructionSetType Set>
void Execute(Function<Set> const &f, std::initializer_list<Value> arguments,
             SmallTrivialValue auto &...return_values) {
  NTH_REQUIRE((v.when(internal::harden)),
              arguments.size() == f.parameter_count())
      .Log<"Argument/parameter count mismatch.">();
  NTH_REQUIRE((v.when(internal::harden)),
              sizeof...(return_values) == f.return_count())
      .Log<"Return value count mismatch.">();
  ValueStack value_stack(arguments);
  int dummy;

  Execute(f, value_stack);
  static_cast<void>(
      (dummy = ... =
           (return_values =
                value_stack.pop<std::decay_t<decltype(return_values)>>(),
            0)));
}

}  // namespace jasmin

#endif  // JASMIN_EXECUTE_H
