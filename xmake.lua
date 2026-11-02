set_xmakever("3.0.0")

-- If newer version of xmake, remove ccache until it actually works
if set_policy ~= nil then
    set_policy("build.ccache", false)
end

-- c code will use c99,
set_languages("c99", "cxx20")

if is_plat("windows") then
    add_cxflags("/bigobj")
    add_syslinks("kernel32")
    set_arch("x64")
    set_runtimes("MT")
end

if is_plat("linux") then
    add_cxflags("-fPIC")
end

set_warnings("all")
add_vectorexts("sse", "sse2", "sse3", "ssse3")
add_vectorexts("neon")

-- build configurations
add_rules("mode.debug", "mode.releasedbg", "mode.release")

option("skyrim_version")
    set_default("latest")
    set_values("latest", "1.6.1170")
    set_description("Skyrim SE runtime the client is built for")
    after_check(function (option)
        local values = option:get("values")
        if not table.contains(values, option:value()) then
            raise("unknown skyrim_version \"%s\" (expected %s)", tostring(option:value()), table.concat(values, ", "))
        end
    end)
option_end()

-- Stamps a fixed version into BuildInfo.h instead of `git describe`, so a local diagnostic build
-- from a branch can still join a server running that release (the protocol has to match).
option("build_version")
    set_default("")
    set_showmenu(true)
    set_description("Version string to report instead of git describe, e.g. v1.8.7-10")
option_end()

if has_config("unitybuild") then
    add_rules("c.unity_build")
    add_rules("c++.unity_build", {batchsize = 12})
end

-- direct dependencies version pinning 
add_requires(
    "entt v3.10.0", 
    "tiltedcore 0.2.9", 
    "cryptopp 8.9.0", 
    "spdlog v1.13.0", 
    "cpp-httplib 0.14.0",
    "gtest v1.14.0", 
    "mem 1.0.0", 
    "glm 0.9.9+8", 
    "zlib v1.3.1"
)
if is_plat("windows") then
    add_requires(
        "discord 3.2.1", 
        "imgui v1.89.7",
        -- built-in voice chat: Opus codec (BSD-3-Clause) and audio devices (miniaudio, MIT-0 or public domain)
        "libopus 1.5.2",
        "miniaudio 0.11.22"
    )
end

-- dependencies' dependencies version pinning
-- LOCAL FORK CHANGE (diverges from upstream v1.8.0): force ONE tiltedcore version.
--
-- Libraries/TiltedConnect/xmake.lua and Libraries/TiltedHooks/xmake.lua are git
-- SUBMODULES pointing at TiltedPhoques repos we do not control, and both still say
--   add_requires("tiltedcore v0.2.7", ...)
-- Because they are includes()-ed into this same xmake project, the build resolves two
-- package instances: tiltedcore v0.2.7 and tiltedcore#1 v0.2.9. Bumping the pins in
-- this file and in Code/client/xmake.lua is therefore NOT enough -- v0.2.7 still gets
-- built, and its nested xmake build requires "mimalloc" with no version, which now
-- resolves to mimalloc v3.5.0, where mi_malloc_size is gone:
--   MimallocAllocator.cpp:(.text+0x29): undefined reference to `mi_malloc_size'
-- On Windows this happened to survive (v0.2.9 was installed first, so the unpinned
-- nested require reused the already-present mimalloc 2.2.4); on Linux it does not.
--
-- This override forces every tiltedcore require in the project -- including the two
-- submodule ones -- to v0.2.9, which pins mimalloc 2.2.4 itself. It avoids having to
-- fork the submodules just to edit one version string.
--
-- REVERT WHEN: the TiltedConnect/TiltedHooks submodules are updated to commits that
-- require tiltedcore >= v0.2.9 themselves. Then this line can go.
add_requireconfs("tiltedcore", { version = "v0.2.9", override = true })
add_requireconfs("*.mimalloc", { version = "2.2.4", override = true })
add_requireconfs("*.cmake", { version = "3.30.2", override = true })
add_requireconfs("*.openssl", { version = "1.1.1-w", override = true })
add_requireconfs("*.zlib", { version = "v1.3.1", override = true })
if is_plat("linux") then
    add_requireconfs("*.libcurl", { version = "8.7.1", override = true })
end

-- HTTPS only serves the server's public-list announcement. Mac cross builds skip it: OpenSSL's Windows build
-- needs nmake and NASM, which don't run on macOS.
if not is_host("macosx") then
    add_requireconfs("cpp-httplib", {configs = {ssl = true}})
end

-- Builds a release (IS_MASTER: no console window, no debug views) from any branch, e.g. a packaged Mac build.
option("release_build")
    set_default(false)
    set_showmenu(true)
    set_description("Build as a release whatever the branch")
option_end()

before_build(function (target)
    import("modules.version")
    local branch, commitHash = version()
    bool_to_number={ [true]=1, [false]=0 }
    local contents = string.format([[
    #pragma once
    #define IS_MASTER %d
    #define IS_BRANCH_BETA %d
    #define IS_BRANCH_PREREL %d
    ]], 
    bool_to_number[branch == "master" or has_config("release_build")], 
    bool_to_number[branch == "bluedove"], 
    bool_to_number[branch == "prerel"])

    -- fix always-compiles problem by updating the file only if content has changed.
    local filepath = "build/BranchInfo.h"
    local old_content = nil
    if os.exists(filepath) then
        old_content = io.readfile(filepath)
    end
    if old_content ~= contents then
        print("Updating file:", filepath)
        io.writefile(filepath, contents)
    end
end)

if is_mode("debug") then
    add_defines("DEBUG")
end

if is_plat("windows") then
    add_defines("NOMINMAX")
end

-- Mac cross builds use a newer clang than fmt 9.1 (bundled in spdlog 1.13) handles in its compile-time format checks:
-- check format strings at run time instead (same output).
if is_host("macosx") then
    add_defines("FMT_CONSTEVAL=")
    add_defines("TP_NO_HTTPS") -- see the cpp-httplib note above
end

-- add projects
includes("Libraries")
includes("Code")
