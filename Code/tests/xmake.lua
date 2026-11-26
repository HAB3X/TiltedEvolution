
target("TPTests")
    set_kind("binary")
    set_group("Tests")
    add_includedirs(
        ".", "../encoding", "../server", "../../Libraries/TiltedConnect/Code/connect/include")
    add_headerfiles("**.h")
    add_files("*.cpp")
    -- The party quest native-load tests came in with a fork merge without the headers they test
    -- (Structs/Skyrim/PartyQuestRuntimeGenerationFence.h, PartyQuestSkyrimPapyrusRuntimeProfileResolver.h, ...),
    -- so they can't build in this tree. Remove this line if that code is merged.
    remove_files("party_quest_*.cpp")
    -- Has no game dependencies, so the client's remote playback timing can be tested here
    add_files("../client/Systems/PlaybackClock.cpp")
    -- Pure date maths, no game dependencies
    add_files("../common/DateTime.cpp")
    -- Voice chat's noise gate, falloff and jitter buffer, also game independent
    add_files("../client/Systems/VoiceDsp.cpp")
    add_deps("SkyrimEncoding")
    add_packages(
        "tiltedcore",
        "hopscotch-map",
        "catch2",
        "mimalloc",
        "glm",
        "entt",
        "gamenetworkingsockets")

target("FakePlayers")
    set_kind("binary")
    set_group("Tests")
    add_includedirs("../encoding", "../../Libraries/TiltedConnect/Code/connect/include")
    add_files("players/FakePlayers.cpp")
    add_deps("SkyrimEncoding", "TiltedConnect", "CommonLib")
    add_packages("tiltedcore", "hopscotch-map", "mimalloc", "glm", "entt", "gamenetworkingsockets")
    if is_plat("windows") then
        add_syslinks("ws2_32")
    end
