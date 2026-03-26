#pragma once

// all includes from DSL-declarations layer
#include <iQSM/api/_gateway.h>

// Umbrella header for non-generated (logic) code.
//
// Policy:
// - Generated headers are allowed to include `iQSM/q1/_gateway.h` (DSL surface).
// - Non-generated code should include this file instead of picking iQSM headers one-by-one.

// Core plumbing
#include <iQSM/_forwards.h>
#include <iQSM/types.h>

// Data model containers
#include <iQSM/field.h>
#include <iQSM/delta.h>
#include <iQSM/schema.h>
#include <iQSM/world.h>
#include <iQSM/resources.h>

// Operations
#include <iQSM/operations/integration.h>
#include <iQSM/operations/validation.h>
#include <iQSM/operations/cache.h>

// Repository (replaces Transaction/Context)
#include <iQSM/repository/commit.h>
#include <iQSM/repository/branch.h>
#include <iQSM/repository/sequence.h>
#include <iQSM/repository/accumulator.h>

// Helpers (non-generated code)
#include <iQSM/helpers/schema.h>
#include <iQSM/helpers/world.h>
#include <iQSM/helpers/particle.h>
#include <iQSM/helpers/global.h>
#include <iQSM/helpers/resource.h>