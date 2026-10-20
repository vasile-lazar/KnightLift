#pragma once

#include <iosfwd>

namespace KnightLift {

    // Runs the UCI command loop until "quit" or end of input.
    void uci_loop(std::istream& in, std::ostream& out);

}  // namespace KnightLift