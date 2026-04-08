#pragma once

// Extension gateway: adds value-comparison helpers (PFR-backed).
// Usage (intended):
//   #include <iQSM/api/_gateway.h>
//   #include <iQSM/api/_gateway_equal.h>
//
// Intentionally NOT included from _gateway.h to avoid pulling Boost.PFR into most TUs.
#include <iQSM/helpers/particle_equal.h>

