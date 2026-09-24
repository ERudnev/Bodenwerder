#pragma once

// Contexts: one Session per open change, thin handles over it.
// A Session is owned by a Realm, a Branch or a normalization wave, never by a handle; it outlives every handle.
// Handles count themselves on the session; when the last handle of a Realm session ends, the Realm accepts it.

#include <optional>
#include <string>

#include <fQSM/model/complex/future.h>
#include <fQSM/model/complex/reality.h>
#include <fQSM/processing/_forwards.h>
#include <fQSM/utility/bad_value.h>

namespace fqsm::processing {

    struct SessionOwner {
        virtual void release(Session&) = 0;
    protected:
        ~SessionOwner() = default;
    };

    class Session {
    public:
        // reality: set for a stewarding session, whose Direct handles mutate these lines in place
        Session(const model::complex::State& base, ref<model::complex::Patch> patch,
                SessionOwner* owner = nullptr, model::complex::Reality* reality = nullptr)
            : base(base), view(base, std::move(patch)), reality(reality), owner(owner) {}
        Session(const Session&) = delete;
        Session& operator=(const Session&) = delete;

        const model::complex::State& base;
        model::complex::Future view;         // base + patch
        model::complex::Reality* reality;
        Rtid::Set tainted;                   // aspects mutated in place through Direct
        bool silent = false;                 // do not log a rejection

        utility::BadValue refuse(std::string message) { view.summary().critical.push_back(std::move(message)); return {}; }
        void warning(std::string message) { view.summary().warning.push_back(std::move(message)); }

        void attach() { ++handles; }
        void detach() { if (--handles == 0 and owner) owner->release(*this); }

    private:
        SessionOwner* owner;
        unsigned handles = 0;
    };

    namespace detail {
        class Handle {
        public:
            explicit Handle(Session& session) : session(&session) { session.attach(); }
            Handle(const Handle& other) : session(other.session) { session->attach(); }
            Handle& operator=(const Handle& other) { other.session->attach(); session->detach(); session = other.session; return *this; }
            ~Handle() { session->detach(); }
        protected:
            Session* session;
        };
    }

    // Read access to a state: a Realm, a Branch or a session view.
    struct Reading {
        explicit Reading(const model::complex::State& state) : state(&state) {}
        const model::complex::State* operator->() const { return state; }
        const model::complex::State& operator*() const { return *state; }
    private:
        const model::complex::State* state;
    };

    // Reads base + patch, writes into the patch.
    struct Writing : detail::Handle {
        explicit Writing(Session& session) : Handle(session) {}
        operator Reading() const { return Reading(session->view); }
        const model::complex::State* operator->() const { return &session->view; }
        model::complex::WorkersInterface& workers_interface() const { return session->view; }
        utility::BadValue refuse(std::string message) const { return session->refuse(std::move(message)); }
        void warning(std::string message) const { session->warning(std::move(message)); }
    };

    // In-place access to Realm lines of one aspect; the aspect is marked tainted for the next normalization.
    template<meta::category::Any Meta>
    struct Direct : detail::Handle {
        using Container = model::linear::Items<Meta>;
        using Global = GlobalValue<Meta>;

        explicit Direct(Session& session)
            : Handle(session)
            , items(session.reality->aspect<Meta>().items())
            , global(session.reality->aspect<Meta>().global())
        {
            session.tainted.insert(TypeId<Meta>);
        }

        Container& items;
        Global& global;

        operator Reading() const { return Reading(*session->reality); }
    };

    // Writing plus Direct access in one session.
    struct Stewarding : Writing {
        using Writing::Writing;
        template<meta::category::Any Meta> auto direct() const -> Direct<Meta> { return Direct<Meta>(*session); }
        template<meta::category::Any Meta> operator Direct<Meta>() const { return direct<Meta>(); }
    };

    // Deletion reactions: reads the last stable state, writes into the corrections of the wave.
    struct Retrospecting : detail::Handle {
        explicit Retrospecting(Session& session) : Handle(session) {}
        operator Reading() const { return Reading(session->base); }
        const model::complex::State* operator->() const { return &session->base; }
        operator Writing() const { return Writing(*session); }
        model::complex::WorkersInterface& workers_interface() const { return session->view; }
    };

    // A reaction: reads the proposal under review, writes into a new correction patch.
    struct Reacting {
        Reacting(const model::complex::Future& proposal, Session& corrections, const model::complex::State& origin, std::optional<Session>& retrospective)
            : proposal(proposal), corrections(corrections), origin(origin), retrospective(retrospective) {}

        const model::complex::Future& proposal;

        template<meta::category::Any Meta> auto changes() const -> model::linear::Delta<Meta> { return proposal.delta<Meta>(); }
        template<meta::category::Any Meta> auto adjustments() const -> model::linear::WorkersInterface<Meta>& { return corrections.view.updates<Meta>(); }
        utility::BadValue refuse(std::string message) const { return corrections.refuse(std::move(message)); }
        void warning(std::string message) const { corrections.warning(std::move(message)); }

        operator Reading() const { return Reading(proposal); }
        operator Writing() const { return Writing(corrections); }

        // opened on first use: most waves have no deletion reaction
        auto retrospecting() const -> Retrospecting {
            if (not retrospective) retrospective.emplace(origin, corrections.view.patch());
            return Retrospecting(*retrospective);
        }

    private:
        Session& corrections;
        const model::complex::State& origin;
        std::optional<Session>& retrospective;
    };
}
