//
// Created by Vincenzo Pellegrini on 28/11/22.
//

#include <tm.hpp>
#include <cstdlib>
#include <iostream>

int main() {
    shared_t shared = tm_create(32, 16);

    tx_t transaction = tm_begin(shared, false);

    void* space = malloc(16);
    for (int i=0; i<10; i++) {
        if (!tm_read(shared, transaction, tm_start(shared), 16, space)) {
            std::cout << "E che cazzo" << std::endl;
        }
        if (!tm_write(shared, transaction, space, 16, tm_start(shared))) {
            std::cout << "E che cazzo" << std::endl;
        }
    }

    return 0;
}