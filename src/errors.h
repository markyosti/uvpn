#ifndef ERRORS_H
# define ERRORS_H

# include <stdlib.h>
# include <stdarg.h>
# include <stdio.h>
# include <errno.h>
# include <string.h>

# include "macros.h"
# include "backtrace.h"

class PErrorSeverity {
 public:
  inline void operator()(
      const char* file, int line, const char* function,
      const char* format, const va_list& arg) const {
    printf("%s:%d %s ", file, line, function);
    vprintf(format, arg);
    printf(" - [%d:%s]\n", errno, strerror(errno));
  }
};

class ErrorSeverity {
 public:
  inline void operator()(
      const char* file, int line, const char* function,
      const char* format, const va_list& arg) const {
    printf("%s:%d %s ", file, line, function);
    vprintf(format, arg);
    printf("\n");
  }
};

class FatalSeverity {
 public:
  FatalSeverity(const char* message) : message_(message) {}

  inline void operator()(
      const char* file, int line, const char* function,
      const char* format, const va_list& arg) const {
    printf("%s:%d %s %s\n  ", file, line, function, message_);
    vprintf(format, arg);
    printf("\n");
  
    PrintBacktrace();
    abort();
  }

 private:
  const char* message_;
};

class AssertSeverity : public FatalSeverity {
 public:
  AssertSeverity(bool result, const char* message)
      : FatalSeverity("ASSERTION FAILED"), 
	result_(result), message_(message) {}

  inline void operator()(
      const char* file, int line, const char* function,
      const char* format, const va_list& arg) const {
    if (result_)
      return;

    FatalSeverity::operator()(file, line, function, format, arg);
  }

 private:
  bool result_;
  const char* message_;
};

template<int T_LINE, typename T_SEVERITY>
class Log {
 public:
  Log(const char* file, const char* function, const T_SEVERITY& handler = T_SEVERITY())
      : file_(file), function_(function), handler_(handler) {
  }

  inline void operator()() const {
    va_list arg; 
    handler_(file_, T_LINE, function_, "", arg);
  }

  inline void operator()(const char* format, ...) const PRINTF(2, 3) {
    va_list arg; 
    va_start(arg, format);
    handler_(file_, T_LINE, function_, format, arg);
    va_end(arg);
  }

 private:
  const char* const file_;
  const char* const function_;
  const T_SEVERITY& handler_;
};

inline void DiscardMessage(const char* message, ...) {
}

# define PRINT_MESSAGE(TYPE, INSTANCE) Log<__LINE__, TYPE>(__FILE__, __PRETTY_FUNCTION__, INSTANCE)
# define DISCARD_MESSAGE DiscardMessage

# ifdef PRINT_DEBUG
#  define LOG_DEBUG PRINT_MESSAGE(ErrorSeverity, ErrorSeverity())
# else
#  define LOG_DEBUG DISCARD_MESSAGE
# endif

# define LOG_INFO PRINT_MESSAGE(ErrorSeverity, ErrorSeverity())
# define LOG_ERROR PRINT_MESSAGE(ErrorSeverity, ErrorSeverity())
# define LOG_PERROR PRINT_MESSAGE(PErrorSeverity, PErrorSeverity())
# define LOG_FATAL PRINT_MESSAGE(FatalSeverity, FatalSeverity("FATAL ERROR"))

# define DEBUG_FATAL_UNLESS(condition) PRINT_MESSAGE(AssertSeverity, AssertSeverity(condition, #condition))
# define RUNTIME_FATAL_UNLESS(condition) PRINT_MESSAGE(AssertSeverity, AssertSeverity(condition, #condition))

inline void HandleRuntimeFatalUnless(
    const char* file, int line, const char* function, const char* condition, bool status) {
  if (status)
    return;

  PRINT_MESSAGE(FatalSeverity, FatalSeverity("ASSERTION FAILED"))(
    "Condition %s is false!", condition);
} 

#endif /* ERRORS_H */
