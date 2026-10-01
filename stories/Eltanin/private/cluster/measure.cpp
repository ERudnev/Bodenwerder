#include "cluster/measure.h"

#include <cmath>
#include <format>

namespace eltanin::cluster::measure {

    auto Format::altitude(double metres) -> std::string {
        if (std::abs(metres) >= eMm)
            return std::format("{:.2f} Mm", metres / eMm);
        return std::format("{:.1f} m", metres);
    }

    auto Format::distance(double metres) -> std::string {
        const double mag = std::abs(metres);
        if (mag >= eMm)
            return std::format("{:.2f} Mm", mag / eMm);
        return std::format("{:.1f} m", mag);
    }

}
