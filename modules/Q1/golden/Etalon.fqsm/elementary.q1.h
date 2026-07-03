#pragma once

#include <fQSM/api/interface.h>

namespace Q1_iQSM {
    using namespace fqsm::api;
    namespace Syntax {
        namespace Typization {

          using Index2Alias = index2;
          using Basic = integer;
          using Alias = Basic;
          using OptionalInt = std::optional<integer>;

          struct Struct {
              Basic field1;
              Alias field2;
          };

          using AliasByField = decltype(Struct::field1);

          struct HasLocalType {
              using LocalType = integer;
              string field;
          };

          // Struct body: local types, fields, and three of four operation kinds (`?` `=` `>`).
          // Invariant validators `!` are not allowed here (see DSL elementary.q1.types).
          struct StructWithMethods {
              using Scalar = float;
              Scalar x;
              Scalar y;

              Scalar length() const;
              void normalize(Scalar);
              static StructWithMethods fromScalar(Scalar argument);
          };
        }

        namespace Namespaces {
            namespace Some {
                using Type = integer;
            }
            namespace Other {
                using Type = string;
            }
            using Type = Some::Type;
        }
    }
}
