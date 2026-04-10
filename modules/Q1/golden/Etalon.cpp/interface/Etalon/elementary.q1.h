#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <iQSM/api/_gateway.h>

namespace Q1CORE {
  namespace Syntax {
    using namespace iqsm::dsl_gateway;
    namespace Typization {
        
      using Index2Alias = index2;
      using Basic = integer;
      using Alias = Basic;

      struct Struct {
        Basic field1;
        Alias field2;
      };

      using AliasByField = decltype(Struct::field1);

      struct HasLocalType {
        using LocalType = integer;
        string field;
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

