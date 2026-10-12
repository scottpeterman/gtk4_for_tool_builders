#include "doctest.h"
#include "core/session.h"

static Session remote(const char *host, const char *user = "", int port = 22) {
    Session s;
    s.name = host;
    s.host = host;
    s.user = user;
    s.port = port;
    return s;
}

TEST_CASE("ssh argv: default port, with user") {
    auto argv = ssh_argv(remote("core1", "admin"));
    CHECK(argv == std::vector<std::string>{"ssh", "admin@core1"});
}

TEST_CASE("ssh argv: custom port") {
    auto argv = ssh_argv(remote("core1", "admin", 2222));
    CHECK(argv == std::vector<std::string>{"ssh", "-p", "2222", "admin@core1"});
}

TEST_CASE("ssh argv: no user") {
    CHECK(ssh_argv(remote("edge1")) == std::vector<std::string>{"ssh", "edge1"});
}

TEST_CASE("ssh argv: local session runs the shell") {
    Session s;
    s.local = true;
    CHECK(ssh_argv(s, "/bin/bash") == std::vector<std::string>{"/bin/bash"});
    CHECK(ssh_argv(s, "") == std::vector<std::string>{"/bin/sh"});
}

TEST_CASE("parse: folders and sessions keep file order") {
    std::string err;
    auto folders = parse_sessions("[b1]\nfolder=Edge\nhost=b1\n"
                                  "[a1]\nfolder=Core\nhost=a1\n"
                                  "[b2]\nfolder=Edge\nhost=b2\nuser=ops\nport=2200\n",
                                  err);
    REQUIRE(err.empty());
    REQUIRE(folders.size() == 2);
    CHECK(folders[0].name == "Edge");
    CHECK(folders[1].name == "Core");
    REQUIRE(folders[0].sessions.size() == 2);
    CHECK(folders[0].sessions[1].name == "b2");
    CHECK(folders[0].sessions[1].port == 2200);
    CHECK(folders[0].sessions[1].user == "ops");
}

TEST_CASE("parse: default file has one local session") {
    std::string err;
    auto folders = parse_sessions(default_sessions_text(), err);
    REQUIRE(err.empty());
    REQUIRE(folders.size() == 1);
    CHECK(folders[0].sessions[0].local);
}

TEST_CASE("parse: malformed files return an error, not a crash") {
    std::string err;
    CHECK(parse_sessions("this is not a key file\n", err).empty());
    CHECK_FALSE(err.empty());

    CHECK(parse_sessions("[x]\nhost=h\nport=abc\n", err).empty());
    CHECK(err.find("port") != std::string::npos);

    CHECK(parse_sessions("[x]\nhost=h\nport=70000\n", err).empty());
    CHECK(err.find("range") != std::string::npos);

    CHECK(parse_sessions("[x]\nfolder=F\n", err).empty());
    CHECK(err.find("no host") != std::string::npos);
}

TEST_CASE("filter: case-insensitive on name, host and user; empty folders dropped") {
    std::string err;
    auto folders = parse_sessions("[core1]\nfolder=Core\nhost=10.0.0.1\n"
                                  "[edge1]\nfolder=Edge\nhost=10.9.0.1\nuser=Admin\n",
                                  err);
    auto f = filter_sessions(folders, "CORE");
    REQUIRE(f.size() == 1);
    CHECK(f[0].name == "Core");

    f = filter_sessions(folders, "admin");
    REQUIRE(f.size() == 1);
    CHECK(f[0].sessions[0].name == "edge1");

    CHECK(filter_sessions(folders, "").size() == 2);
    CHECK(filter_sessions(folders, "nomatch").empty());
}
