#include "placeholder.q1.h"

namespace placeholder {

    struct MyAttribute::Internals : MyAttribute::DefaultInternals {

        static void ruleOne(Retrospecting context, Id, const Quantum& last) {
            base::message("i am destroyed!");
        }

        static auto ruleTwo(const Quantum& inspected)
        ->PossibleChange {
            base::message("i am normalized from {}", base::encoded(inspected));
            return {};
        }

        static void localRule(Reacting context) {
            int changesHappen = context.changes<MyAttribute>().addedOrUpdated().size();
            if (changesHappen > Always::recommendedUpdateSize)
                context.result.critical.push_back(std::format("demo: MyAttributes {} updated at one, while limited by {}. aborting", changesHappen, Always::recommendedUpdateSize));
        }

        static void globalRule(Reacting context) {
            const auto boldsAround = context.proposal.aspect<Minimal>().items().size();
            for (const auto& newBold : context.changes<Minimal>().added())
                base::message("family of MyAttribute sees new Bold: {}, total: {}", newBold.id, boldsAround);
        }
    };

    void MyAttribute::Actions::justlog(Reading context, Id id) {
        base::message(std::format("MyAttribute holds {}", base::encoded(get(context, id))));
    }

    auto MyAttribute::customAspectReactions()-> const Behavior {
        return {
            reaction::deletion<MyAttribute>(&MyAttribute::Internals::ruleOne),
            reaction::constraint::element<MyAttribute>(&MyAttribute::Internals::ruleTwo),
            reaction::aspect_wide<MyAttribute>(&MyAttribute::Internals::localRule),
            reaction::aspect_wide<MyAttribute, Minimal>(&MyAttribute::Internals::globalRule),
        };
    };
    /*const Behavior MyAttribute::own_reactions_exp = {
        reaction::deletion<MyAttribute>(&MyAttribute::Internals::ruleOne),
        reaction::constraint::element<MyAttribute>(&MyAttribute::Internals::ruleTwo),
         //reaction::custom<MyEntity>(&localRule),
    };*/
}