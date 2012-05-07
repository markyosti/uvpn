#include <execinfo.h>
#include <cxxabi.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define STACK_SIZE 50

// TODO: make this reasonable. It is not, right now.
// (malloc, fprintf, works only on linux, ...)
void PrintBacktrace() {
  void* trace[STACK_SIZE];
 
  int size = backtrace(trace, STACK_SIZE);
  char** messages = backtrace_symbols(trace, size);

  for (int i = 1; i < size && messages != NULL; ++i) {
    char* line = messages[i];

    // This code is best effort. Format of lines returned by backtrace_symbols
    // is not defined, could change over time and from platform to platform.
    // We try to decode them based on what those lines look like on a few random
    // systems we tried this on. If we succeed, we print the output. If we fail,
    // we print the original line.
    //   ./uvpn-user(_ZNK13FatalSeverityclEPKciS1_S1_RKPc+0x84) [0x8055c8e]

    char* filename = line;
    char* function_start = strchr(line, '(');
    char* offset_start, * offset_end, * address_start;
    if (!function_start ||
	!(offset_start = strchr(function_start, '+')) ||
	!(offset_end = strchr(offset_start, ')')) ||
	!(address_start = strchr(offset_end, '['))) {
      fprintf(stderr, "[%d]: %s\n", i, line);
      continue;
    }

    // Split the buffer in multiple independent strings.
    // This works, but is hacky.
    *function_start++ = '\0';
    *offset_start++ = '\0';
    *offset_end = '\0';

    int status;
    char* demangled = abi::__cxa_demangle(function_start, NULL, 0, &status);
    if (demangled) {
      fprintf(stderr, "[%d]: %s - %s +%s %s\n",
	      i, filename, demangled, offset_start, address_start);
    } else {
      fprintf(stderr, "[%d]: %s - %s +%s %s\n",
	      i, filename, function_start, offset_start, address_start);
    }
    free(demangled);
  }
  free(messages);
}
