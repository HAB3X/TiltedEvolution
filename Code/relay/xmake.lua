-- A standalone relay for hosts whose router can't be opened (see Relay.cpp); plain sockets only, so it also builds
-- on Linux for a small server
target("SkyrimTogetherRelay")
    set_kind("binary")
    set_group("Server")
    add_includedirs("../encoding", "../common")
    add_files("Relay.cpp")
    if is_plat("windows") then
        add_syslinks("ws2_32")
    end
