#include "doctest.h"
#include "core/settings.h"

TEST_CASE("settings: empty text gives defaults") {
    Settings s = parse_settings("");
    CHECK(s.width == 900);
    CHECK(s.height == 600);
    CHECK(s.sidebar_visible);
}

TEST_CASE("settings: round trip") {
    Settings a;
    a.width = 1200;
    a.sidebar_visible = false;
    a.app_variant = "dark";
    a.font_size = 14;
    Settings b = parse_settings(settings_to_text(a));
    CHECK(b.width == 1200);
    CHECK_FALSE(b.sidebar_visible);
    CHECK(b.app_variant == "dark");
    CHECK(b.font_size == 14);
}

TEST_CASE("settings: hand-edited nonsense is clamped") {
    Settings s = parse_settings("[window]\nwidth=10\nheight=abc\n[appearance]\napp-variant=purple\n");
    CHECK(s.width == 400);
    CHECK(s.height == 600);
    CHECK(s.app_variant == "light");
}
