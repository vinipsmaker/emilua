/* Copyright (c) 2020, 2021 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

#include <string_view>
#include <charconv>
#include <iostream>

#include <CLI/CLI.hpp>

#include <boost/preprocessor/stringize.hpp>
#include <boost/predef/os/windows.h>
#include <boost/asio/io_context.hpp>

#include <emilua/state.hpp>

#if EMILUA_CONFIG_ENABLE_COLOR
#include <cstdlib>
#include <cstdio>

extern "C" {
#include <curses.h>
#include <term.h>
} // extern "C"
#endif // EMILUA_CONFIG_ENABLE_COLOR

namespace asio = boost::asio;
namespace fs = std::filesystem;

int main(int argc, char *argv[])
{
    LUAJIT_VERSION_SYM();

#if EMILUA_CONFIG_ENABLE_COLOR
    emilua::stdout_has_color = []() {
        using namespace std::literals::string_view_literals;
        auto env = std::getenv("EMILUA_COLORS");
        if (env) {
            if (env == "ON"sv || env == "1"sv || env == "YES"sv ||
                env == "TRUE"sv) {
                return true;
            } else if (env == "OFF"sv || env == "0"sv || env == "NO"sv ||
                       env == "FALSE"sv) {
                return false;
            }
        }

        // Emilua runtime by itself will only ever dirt stderr
        if (!isatty(fileno(stderr)))
            return false;

        int ec = 0;
        if (setupterm(NULL, fileno(stderr), &ec) == ERR)
            return false;

        bool ret = tigetnum("colors") > 0;
        del_curterm(cur_term);
        return ret;
    }();
#else
    emilua::stdout_has_color = false;
#endif // EMILUA_CONFIG_ENABLE_COLOR

    if (auto rawenv = std::getenv("EMILUA_LOG_LEVELS") ; rawenv) {
        std::string_view env = rawenv;
        int level;
        auto res = std::from_chars(
            env.data(), env.data() + env.size(), level);
        if (res.ec == std::errc{})
            emilua::log_domain<emilua::default_log_domain>::log_level = level;
    }

    CLI::App app{"Emilua: Execution engine for luaJIT"};

    std::string filename;
    int main_ctx_concurrency_hint = BOOST_ASIO_CONCURRENCY_HINT_DEFAULT;
    try {
        auto filename_opt = app.add_option("file", filename, "Script filename");
        app.add_option(
            "--main-context-concurrency-hint", main_ctx_concurrency_hint,
            "Concurrency hint for the main execution engine context");
        auto version_opt = app.add_flag(
            "--version", "Output version information and exit");

        app.parse(argc, argv);

        if (*version_opt) {
            std::cout << "Emilua " EMILUA_CONFIG_VERSION_STRING << std::endl;
            return 0;
        }

        if (!*filename_opt) {
            std::cerr << "Missing filename" << std::endl;
            return 1;
        }
    } catch (const CLI::ParseError &e) {
        return app.exit(e);
    }

    if (filename.size() == 0) {
        std::cerr << "Invalid filename given (empty)" << std::endl;
        return 1;
    }

    emilua::app_context appctx;
#if EMILUA_CONFIG_THREAD_SUPPORT_LEVEL == 2
    asio::io_context ioctx{main_ctx_concurrency_hint};
#elif EMILUA_CONFIG_THREAD_SUPPORT_LEVEL == 1
    asio::io_context ioctx{1};
#elif EMILUA_CONFIG_THREAD_SUPPORT_LEVEL == 0
    asio::io_context ioctx{BOOST_ASIO_CONCURRENCY_HINT_UNSAFE};
#else
# error Invalid thread support level
#endif

    {
        const char* env = std::getenv("EMILUA_PATH");
        if (env) {
            for (std::string_view spec{env} ;;) {
                std::string_view::size_type sepidx = spec.find(
#if BOOST_OS_WINDOWS
                    ';'
#else
                    ':'
#endif
                );
                if (sepidx == std::string_view::npos) {
                    appctx.emilua_path.emplace_back(
                        spec, fs::path::native_format);
                    break;
                } else {
                    appctx.emilua_path.emplace_back(
                        spec.substr(0, sepidx), fs::path::native_format);
                    spec.remove_prefix(sepidx + 1);
                }
            }
        }
    }

    // NOTE: Using `auto_format` because who knows how will meson give me this
    // path here?
    appctx.emilua_path.emplace_back(
        EMILUA_CONFIG_LIBROOTDIR, fs::path::auto_format);
    // TODO: Remove VERSION_MINOR from path components once emilua reaches
    // version 1.0.0 (versions that differ only in minor and patch numbers do
    // not break API).
    appctx.emilua_path.back() /=
        "emilua-" BOOST_PP_STRINGIZE(EMILUA_CONFIG_VERSION_MAJOR)
        "." BOOST_PP_STRINGIZE(EMILUA_CONFIG_VERSION_MINOR);

    try {
        auto vm_ctx = emilua::make_vm(ioctx, appctx, filename,
                                      emilua::ContextType::main);
        vm_ctx->strand().post([vm_ctx]() {
            vm_ctx->fiber_resume_trivial(vm_ctx->L());
        }, std::allocator<void>{});
    } catch (std::exception& e) {
        std::cerr << "Error starting the lua VM: " << e.what() << std::endl;
        return 1;
    }

    for (;;) {
        try {
            ioctx.run();
            break;
        } catch (const emilua::dead_vm_error&) {
            continue;
        }
    }

    {
        std::unique_lock<std::mutex> lk{appctx.extra_threads_count_mtx};
        while (appctx.extra_threads_count > 0)
            appctx.extra_threads_count_empty_cond.wait(lk);
    }

    return appctx.exit_code;
}
