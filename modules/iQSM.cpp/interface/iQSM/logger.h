#pragma once

#include <base/logging.h>
#include <iQSM/q1/builtins.h>

namespace iqsm::logger {

  // Backward compatible facade. The implementation lives in `base`.
  using base::message;
  using base::fatal;
  using base::check;
  using base::report;
  using base::to_string;

  inline iqsm::q1::timepoint now() { return base::now(); }
}