#include "doctest.h"
#include "core/theme.h"

static std::string full_theme(const char *bg = "#2e3440") {
    std::string s = "[theme]\nlabel=Test\ntype=dark\n[colors]\n";
    s += std::string("background=") + bg + "\nforeground=#d8dee9\ncursor=#d8dee9\nselection=#434c5e\n";
    for (const char *k : theme_palette_keys())
        s += std::string(k) + "=#112233\n";
    return s;
}

TEST_CASE("theme: a complete file parses") {
    Theme t;
    std::string err;
    REQUIRE(parse_theme("test", full_theme(), t, err));
    CHECK(t.label == "Test");
    CHECK(t.dark);
    CHECK(t.background == "#2e3440");
    CHECK(t.palette[15] == "#112233");
}

TEST_CASE("theme: bad colours are rejected with the key named") {
    Theme t;
    std::string err;
    CHECK_FALSE(parse_theme("test", full_theme("blue"), t, err));
    CHECK(err.find("background") != std::string::npos);
    CHECK_FALSE(parse_theme("test", "[theme]\nlabel=x\n", t, err));
}
