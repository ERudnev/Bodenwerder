#pragma once

// all includes from DSL-declarations layer
#include <iQSM/q1/_gateway.h>

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

// Operations
#include <iQSM/operations/integration.h>
#include <iQSM/operations/transaction.h>