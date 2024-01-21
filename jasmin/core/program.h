#ifndef JASMIN_CORE_PROGRAM_H
#define JASMIN_CORE_PROGRAM_H

#include <string>
#include <string_view>

#include "absl/container/node_hash_map.h"
#include "jasmin/core/function.h"
#include "jasmin/core/instruction.h"
#include "nth/debug/debug.h"
#include "nth/utility/iterator_range.h"

namespace jasmin {

// A `Program` represents a collection of functions closed under the caller ->
// callee relationship, with a unique entry-point. It is the responsibility of
// the user to ensure that any function transitively invocable via functions in
// a program are also in the same program. Functions transitively invocable are
// assumed to be owned solely by the `Program`. They may be modified (by an
// optimizer, or a debugger), and could therefore cause unsupported, unexpected,
// or undefined behavior if this requirement is violated.
template <InstructionSetType Set>
struct Program {
  // Declares a function owned by this `Program` with the given name and
  // signature (number of inputs and outputs).
  Function<Set>& declare(std::string_view name, uint32_t inputs,
                         uint32_t outputs);

  // Returns a reference to the function declared with the given name. Behavior
  // is undefined if no such function exists.
  Function<Set> const& function(std::string_view name) const;
  Function<Set>& function(std::string_view name);

  // Returns the number of functions managed by this `Program`.
  size_t function_count() const { return functions_.size(); }

  auto functions() const {
    return nth::iterator_range(functions_.begin(), functions_.end());
  }

 private:
  absl::node_hash_map<std::string, Function<Set>> functions_;
};

template <InstructionSetType Set>
Function<Set>& Program<Set>::declare(std::string_view name, uint32_t inputs,
                                     uint32_t outputs) {
  auto [iter, inserted] = functions_.try_emplace(name, inputs, outputs);
  return iter->second;
}

template <InstructionSetType Set>
Function<Set>& Program<Set>::function(std::string_view name) {
  auto iter = functions_.find(name);
  NTH_REQUIRE((v.harden), iter != functions_.end());
  return iter->second;
}

template <InstructionSetType Set>
Function<Set> const& Program<Set>::function(std::string_view name) const {
  auto iter = functions_.find(name);
  NTH_REQUIRE((v.harden), iter != functions_.end());
  return iter->second;
}

}  // namespace jasmin

#endif  // JASMIN_CORE_PROGRAM_H
