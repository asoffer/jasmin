#include <cstdio>
#include <iostream>

#include "jasmin/execute.h"
#include "jasmin/instructions/compare.h"
#include "jasmin/instructions/core.h"

// This file provides two relatively simple examples of defining
// `jasmin::Function`s and executing them. The first example in the function
// `HelloWorld` prints "Hello, World!" to the stdout. While there are of course
// easier ways to do this, the example does not show the most trivial solution,
// but rather, one that balances a simple first example with enough detail to
// give a clear picture of how Jasmin works.

// We start by defining an instruction that pops a null-terminated
// `char const *` off the stack and prints it to `stdout`.
struct PrintCString : jasmin::StackMachineInstruction<PrintCString> {
  // Jasmin knows that exactly one value should be popped off the stack, and
  // that that one value is a `const char *` because it inspects the parameters
  // of the function named `execute`.
  static void execute(char const *cstr) { std::fputs(cstr, stdout); }
};

void HelloWorld() {
  // Next, we need to define our instruction set. Along with the implicit
  // built-in instructions like `Return`, we need a `Push` instruction to put
  // values on the stack (found in "jasmin/instructions/core.h"), along with
  // the `PrintCString` instruction we just defined.
  using Instructions = jasmin::MakeInstructionSet<jasmin::Push, PrintCString>;

  // Now we can define our function. It takes no arguments and returns no
  // values.
  jasmin::Function<Instructions> func(0, 0);
  // We print the two strings separately. Of course, in this case it would be
  // easier to have one "Hello, world!\n" string. We could also push "Hello, "
  // and print it, and then push "world!\n" and print it. We are choosing to
  // push both strings (in reverse order!) to demonstrate that we are working
  // with a stack.
  func.append<jasmin::Push>("world!\n");
  func.append<jasmin::Push>("Hello, ");
  func.append<PrintCString>();
  func.append<PrintCString>();
  func.append<jasmin::Return>();

  // Now that our function has been defined, we can execute it.
  jasmin::Execute(func, {/* No arguments */});
}

// The next example reads in two integers from `stdin` and prints whether they
// are in increasing order or not. The goal of this example is to demonstrate
//   (1) Conditional and unconditional.
//   (2) The reuse of instructions across different instruction sets (we will
//       be reuusing PrintCString.
//
// Let's define the instruction that reads integers from `stdin` and writes them
// to the stack. The other instructions we will need are all provided by Jasmin
// itself (the comparison can be found in "jasmin/instructions/compare.h", and
// the conditional and unconditional jumps are automatically built-in to every
// instruction set.
struct ReadIntegerFromStdIn
    : jasmin::StackMachineInstruction<ReadIntegerFromStdIn> {
  // Just as with `PrintCString`, from the signature of `execute`, Jasmin
  // deduces that this instruction reads no values from the stack but writes a
  // single value of type `int` (the return type).
  static int execute() {
    int result;
    std::scanf("%d", &result);
    // Note: This function is not robust as it does nothing to validate that the
    // provided value was a valid integer. The author of an instruction is
    // responsible for handling this sort of error-checking for themselves.
    return result;
  }
};

void Ordered() {
  // Just as in `HelloWorld`, we need to define our instruction set.
  using Instructions =
      jasmin::MakeInstructionSet<ReadIntegerFromStdIn, jasmin::LessThan<int>,
                                 PrintCString, jasmin::Push>;

  jasmin::Function<Instructions> func(0, 0);
  // Request the first integer
  func.append<jasmin::Push>("Pick an integer: ");
  func.append<PrintCString>();
  func.append<ReadIntegerFromStdIn>();

  // Request the second integer
  func.append<jasmin::Push>("Pick an integer: ");
  func.append<PrintCString>();
  func.append<ReadIntegerFromStdIn>();

  // Pop the top two values from the stack and push the boolean value indicating
  // if the first is smaller than the second (first means pushed earlier; the
  // second is the value that is on the top of the stack).
  func.append<jasmin::LessThan<int>>();

  // `JumpIf` pops the top value (a boolean) from the stack and jumps if that
  // bool is true. Otherwise, it falls through to the next instruction. Not that
  // we are using `append_with_placeholders` rather than `append`. That's
  // because we do not yet know where we are jumping to. We simply leave the
  // slot blank so we can tell it how far to jump later. The
  // `append_with_placeholders` member function returns an `OpCodeRange`
  // representing the range of instruction slots covered by the jump that we can
  // update later.  function.
  jasmin::OpCodeRange jump_if = func.append_with_placeholders<jasmin::JumpIf>();

  // If we don't end up jumping, it's because the bool is false. We will push a
  // string we want to print (but not print it yet). We will follow this with
  // the code for the case where the numbers are in order, but we don't want
  // this path to execute that code, so we need another jump. This one can be
  // unconditional.
  func.append<jasmin::Push>("The numbers are out of order.\n");
  jasmin::OpCodeRange jump_unconditionally =
      func.append_with_placeholders<jasmin::Jump>();

  // This is where we are going to jump from the `JumpIf` instructions. The call
  // to `set_value` probably needs some explanation. Jumps are *relative* so we
  // want to say how far to jump. That's the difference between how big the
  // function was when we added the `JumpIf` instruction, and how big it is now.
  jasmin::OpCodeRange push =
      func.append<jasmin::Push>("The numbers are in order.\n");
  func.set_value(jump_if, 0, jasmin::OpCodeRange::Distance(push, jump_if));

  // Now we can add a print instruction and either fall through from the `Push`
  // above, or get here from the unconditional jump higher up above.
  jasmin::OpCodeRange print = func.append<PrintCString>();
  func.set_value(jump_unconditionally, 0,
                 jasmin::OpCodeRange::Distance(print, jump_unconditionally));
  func.append<jasmin::Return>();

  // Now that our function has been defined, we can execute it.
  jasmin::Execute(func, {/* No arguments */});
}

int main() {
  std::fputs(
      "Executing HelloWorld...\n"
      "=======================\n",
      stdout);
  HelloWorld();
  std::fputs(
      "=======================\n"
      "...done!\n",
      stdout);

  std::fputs(
      "Executing Ordered...\n"
      "=======================\n",
      stdout);
  Ordered();
  std::fputs(
      "=======================\n"
      "...done!\n",
      stdout);

  return 0;
}
