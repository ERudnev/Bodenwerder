#pragma once

#include <boost/pfr/ops_fields.hpp>

#include <iQSM/helpers/particle.h>

namespace iqsm::helpers::particle {
  template<meta::HasQuantum Meta>
  bool equal(const Quantum<Meta>& a, const Quantum<Meta>& b) {
    return boost::pfr::eq_fields(a, b);
  }
}

