#pragma once

#include <fQSM/state/_forwards.h>
#include <fQSM/processing/context.h>

namespace fqsm::processing {

    struct Transaction {
        virtual ~Transaction() = default;

        virtual operator Reading() const = 0;
        virtual operator Writing() = 0;

        // add own Context?
        

    protected:
        Transaction(Reading root) {
        }

        Transaction(Permit& writing) {
        };

        // Bypassing Permit privacy as friend (grant derived classes Permit access)     
        static Channel consume(Permit& permit) { return permit.consume(); }

        static Permit grantPermit(Channel channel) { return Permit{channel}; }

        // data:
        Channel context;
    };
}
