#pragma once
#include <string>

namespace cviz {

// Generates a random program inside the supported subset, in the spirit of
// Csmith but far smaller. Every generated program is deterministic,
// terminating (bounded loops, no recursion, calls only to earlier
// functions) and free of the undefined behaviour that would make a
// comparison against gcc meaningless: values are kept well inside the range
// of int, divisors are non-zero constants, left shifts are masked, and every
// variable and array is initialised before it is read.
std::string generate_program(unsigned seed);

}  // namespace cviz
