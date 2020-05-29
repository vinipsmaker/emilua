#include <iostream>

#include <CLI/CLI.hpp>

#include <boost/asio/io_context.hpp>

#include <emilua/state.hpp>

#if ENABLE_COLOR
#include <cstdlib>
#include <cstdio>

extern "C" {
#include <curses.h>
#include <term.h>
} // extern "C"
#endif // ENABLE_COLOR

namespace asio = boost::asio;

int main(int argc, char *argv[])
{
    LUAJIT_VERSION_SYM();

#if ENABLE_COLOR
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
#endif // ENABLE_COLOR

    CLI::App app{"Emilua: Execution engine for luaJIT"};

    std::string filename;
    int main_ctx_concurrency_hint = BOOST_ASIO_CONCURRENCY_HINT_DEFAULT;
    app.add_option("file", filename, "Script filename")->required();
    app.add_option("--main-context-concurrency-hint", main_ctx_concurrency_hint,
                   "Concurrency hint for the main execution engine context");

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError &e) {
        return app.exit(e);
    }

    if (filename.size() == 0) {
        std::cerr << "Invalid filename given (empty)" << std::endl;
        return 1;
    }

    int exit_code = 0;
    asio::io_context ioctx{main_ctx_concurrency_hint};

    try {
        auto vm_ctx = emilua::make_vm(ioctx, exit_code, filename,
                                      emilua::ContextType::main);
        vm_ctx->strand().post([vm_ctx]() {
            if (!vm_ctx->valid())
                return;

            vm_ctx->fiber_prologue(vm_ctx->L());
            int res = lua_resume(vm_ctx->L(), 0);
            vm_ctx->fiber_epilogue(res);
        }, std::allocator<void>{});
    } catch (std::exception& e) {
        std::cerr << "Error starting the lua VM: " << e.what() << std::endl;
        return 1;
    }

    for (;;) {
        try {
            ioctx.run();
            break;
        } catch (const emilua::lua_exception& e) {
            assert(e.code() == emilua::lua_errc::mem);
            boost::ignore_unused(e);
        }
    }

    return exit_code;
}
