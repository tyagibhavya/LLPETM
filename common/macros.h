#pragma once

#include <cstring>
#include <iostream>

/*
These macro definitions are used for branch prediction hints in C or C++ code. They are commonly used to convey
to the compiler the likelihood of a certain condition being true or false, which can help the compiler optimize the 
generated machine code for better performance.
*/

#define LIKELY(x) __builtin_expect(!!(x), 1)
/*
__builtin_expect is a compiler-specific built-in function that provides a hint to the compiler about the expected result of a condition.
LIKELY(x) is defined to use this built-in function with the argument !!(x) (the double negation is used to ensure the result is either 0 or 1).
The second argument of __builtin_expect is 1, indicating that the condition x is likely to be true.
This macro is typically used to annotate conditions that are expected to be true most of the time.
*/

#define UNLIKELY(x) __builtin_expect(!!(x), 0)

/*
Similar to LIKELY(x), UNLIKELY(x) also uses __builtin_expect with the argument !!(x).
The second argument of __builtin_expect here is 0, indicating that the condition x is unlikely to be true.
This macro is used for conditions that are expected to be false most of the time.
*/

inline auto ASSERT(bool cond, const std::string &msg) noexcept {
  if (UNLIKELY(!cond)) {
    //--------------------------------------------
    std::cerr << "ASSERT : " << msg << std::endl;
    //--------Bhai endl hata kr \n daal de--------

    exit(EXIT_FAILURE);
  }
}
/*
This ASSERT function is designed for "runtime assertions". If the provided condition (cond) is false,
it outputs a custom error message to the standard error stream and terminates the program with an exit
code indicating failure. The use of UNLIKELY suggests that the function is optimized for cases where
assertions are likely to fail infrequently. The noexcept specifier emphasizes that the function does
not throw exceptions.
*/
inline auto FATAL(const std::string &msg) noexcept {
  std::cerr << "FATAL : " << msg << std::endl;

  exit(EXIT_FAILURE);
}
