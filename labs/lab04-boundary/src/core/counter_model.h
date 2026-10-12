#pragma once
// Pure application logic: no GTK, no GLib. The meson target that builds the
// tests is not given GTK, so an #include <gtk/gtk.h> here would not compile.
#include <string>

class CounterModel {
public:
    int value() const { return count_; }
    void increment() { ++count_; }
    void reset() { count_ = 0; }
    std::string description() const { return "Operations recorded: " + std::to_string(count_); }

private:
    int count_ = 0;
};
