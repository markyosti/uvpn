#ifndef LOGGING_H
# define LOGGING_H

# include <typeinfo>
# include "errors.h"

class WithLogging {
 public:
  WithLogging() {}
  virtual ~WithLogging() {}

  void InitLogging() {
    LOG_DEBUG("initialized %s", typeid(*this).name());
  }
};

#endif /* LOGGING_H */
