#pragma once

// Archive: Retrospection forms for Q1 Etalon aspects.
// Not included by live code — kept as a snapshot of the s11n experiment.

#include <Etalon.fqsm/aspects.q1.h>

namespace fqsm::aspect {

template<>
struct Retrospection<Q1_fQSM::Etalon::SampleEntity> {
    template<typename Desc>
    static void describe(Desc& d) {
        d.aspect("Q1_fQSM::Etalon::SampleEntity");
        d.one(field<&Q1_fQSM::Etalon::SampleEntity::Quantum::data_field>("data_field"));
        d.all(field<&Q1_fQSM::Etalon::SampleEntity::Global::common_data>("common_data"));
    }
};

template<>
struct Retrospection<Q1_fQSM::Etalon::Tag> {
    template<typename Desc>
    static void describe(Desc& d) {
        d.aspect("Q1_fQSM::Etalon::Tag");
        d.all(field<&Q1_fQSM::Etalon::Tag::Global::modulus>("modulus"));
    }
};

template<>
struct Retrospection<Q1_fQSM::Etalon::Reminder> {
    template<typename Desc>
    static void describe(Desc& d) {
        d.aspect("Q1_fQSM::Etalon::Reminder");
        d.one(field<&Q1_fQSM::Etalon::Reminder::Quantum::target>("target"));
        d.one(field<&Q1_fQSM::Etalon::Reminder::Quantum::trigger_value>("trigger_value"));
    }
};

template<>
struct Retrospection<Q1_fQSM::Etalon::Remnant> {
    template<typename Desc>
    static void describe(Desc& d) {
        d.aspect("Q1_fQSM::Etalon::Remnant");
        d.one(field<&Q1_fQSM::Etalon::Remnant::Quantum::power>("power"));
    }
};

template<>
struct Retrospection<Q1_fQSM::Etalon::SampleComponent> {
    template<typename Desc>
    static void describe(Desc& d) {
        d.aspect("Q1_fQSM::Etalon::SampleComponent");
    }
};

template<>
struct Retrospection<Q1_fQSM::Etalon::SampleAttribute> {
    template<typename Desc>
    static void describe(Desc& d) {
        d.aspect("Q1_fQSM::Etalon::SampleAttribute");
        d.one(field<&Q1_fQSM::Etalon::SampleAttribute::Quantum::main_anchor>("main_anchor"));
        d.one(field<&Q1_fQSM::Etalon::SampleAttribute::Quantum::main_dummy>("main_dummy"));
    }
};

template<>
struct Retrospection<Q1_fQSM::Etalon::Note> {
    template<typename Desc>
    static void describe(Desc& d) {
        d.aspect("Q1_fQSM::Etalon::Note");
        d.one(field<&Q1_fQSM::Etalon::Note::Quantum::text>("text"));
    }
};

}
