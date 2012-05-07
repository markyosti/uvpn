#ifndef STACKTRACE_H
# define STACKTRACE_H

// TODO: this only works in linux, we need to find a more potrable system.
// libunwind? whatever. The output also sucks bad times. Especially for c++.
extern void PrintBacktrace();

#endif /* STACKTRACE_H */
