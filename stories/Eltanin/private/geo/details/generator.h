#pragma once

namespace eltanin::locality::planet {
    struct Planet;
}

namespace eltanin::locality::geo {

    void generate(planet::Planet&);
    void generateSurfaceWeights(planet::Planet&);

}
