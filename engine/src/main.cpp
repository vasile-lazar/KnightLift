#include <iostream>

#include "attacks.hpp"
#include "uci.hpp"

int main() {
    KnightLift::init_attacks();
    KnightLift::uci_loop(std::cin, std::cout);
    return 0;
}