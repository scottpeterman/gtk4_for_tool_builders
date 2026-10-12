// Runs with no display and no GTK: `meson test -C build-debug`.
#include "core/counter_model.h"
#include <cassert>
#include <cstdio>

int main() {
    CounterModel model;
    assert(model.value() == 0);
    model.increment();
    model.increment();
    assert(model.value() == 2);
    assert(model.description() == "Operations recorded: 2");
    model.reset();
    assert(model.value() == 0);
    std::puts("counter model: all checks passed");
    return 0;
}
