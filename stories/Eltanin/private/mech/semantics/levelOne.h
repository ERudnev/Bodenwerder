#pragma once

#include "shapes.h"
#include "slots.h"

#include <string>

// LevelOne meshpack entry names (layer / OBJ stem). Empty = no mesh yet — skip spawn.
// Fill after LightWave; never fall back onto a real mesh name.

namespace eltanin::mech::levelOne {

    inline auto mesh(frame::shape shape) -> std::string {
        switch (shape) {
            case frame::shape::k8: return "k8";
            case frame::shape::k7: return "k7";
            case frame::shape::k6: return "k6";
            case frame::shape::k4: return "k4";
        }
        return {};
    }

    inline auto mesh(plate::shape shape) -> std::string {
        switch (shape) {
            case plate::shape::p1111: return "p1111";
            case plate::shape::p121: return "p121";
            case plate::shape::p2121: return "p2121";
            case plate::shape::p222A: return "p222A";
            case plate::shape::p222V: return "p222V";
        }
        return {};
    }

    inline auto mesh(wing::shape shape) -> std::string {
        switch (shape) {
            case wing::shape::w1111: return "w1111";
            case wing::shape::w121: return "w121";
            case wing::shape::w2121: return "w2121";
            case wing::shape::w321: return "w321";
            case wing::shape::w222: return "w222";
        }
        return {};
    }

    inline auto mesh(inner::shape shape) -> std::string {
        switch (shape) {
            case inner::shape::full: return {};
            case inner::shape::quarter: return {};
            case inner::shape::octa: return {};
        }
        return {};
    }

    inline auto mesh(slot::inner role) -> std::string {
        switch (role) {
            case slot::inner::multi: return {};
            case slot::inner::engine: return {};
            case slot::inner::power: return {};
            case slot::inner::battery: return {};
            case slot::inner::hardpoint: return {};
            case slot::inner::hangar: return {};
            case slot::inner::cargo: return {};
            case slot::inner::logistic: return {};
            case slot::inner::emissive: return {};
            case slot::inner::control: return {};
            case slot::inner::living: return {};
        }
        return {};
    }

    inline auto mesh(slot::plate role) -> std::string {
        switch (role) {
            case slot::plate::armor: return {};
            case slot::plate::thruster: return {};
            case slot::plate::cooling: return {};
            case slot::plate::turret: return {};
            case slot::plate::barrel: return {};
            case slot::plate::pd: return {};
            case slot::plate::hatch: return {};
            case slot::plate::bay: return {};
            case slot::plate::antenna: return {};
            case slot::plate::cockpit: return {};
            case slot::plate::windowed: return {};
            case slot::plate::agfe: return {};
            case slot::plate::utility: return {};
            case slot::plate::logistic: return {};
        }
        return {};
    }

    inline auto mesh(slot::wing role) -> std::string {
        switch (role) {
            case slot::wing::radiance: return {};
            case slot::wing::agfe: return {};
            case slot::wing::pylon: return {};
        }
        return {};
    }

} // namespace eltanin::mech::levelOne
