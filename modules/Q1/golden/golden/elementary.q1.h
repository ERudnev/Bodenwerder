#pragma once

#include <fQSM/api/interface.h>

namespace Q1_iQSM {
    using namespace fqsm::api;
    namespace Syntax {
        namespace Typization {

            using Index2 = index2;
            using Index3 = index3;
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
            // Invariant validators `!` are not allowed here (see DSL elementary.q1).
            struct StructWithMethods {
                using Scalar = float;
                Scalar x;
                Scalar y;

                Scalar length() const;
                void normalize(Scalar);
                static StructWithMethods fromScalar(Scalar argument);
                void add_to(StructWithMethods& target) const;
                void add_from(const StructWithMethods& source);
                static StructWithMethods build_from(const StructWithMethods& source);
            };

            struct MultistyleFieldsSyntax {
                static const integer myStaticConst = 7;
                static const integer myOtherStaticConst = 8;
                integer field1;
                integer field2;
                integer field3;
                integer field4;
                integer field5;
                integer field6;
                static integer staticMutable1;
                static integer staticMutable2;
                static integer staticMutable3;
                static integer staticMutable4;
                static const std::unordered_map<string, integer>& myTable();
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
