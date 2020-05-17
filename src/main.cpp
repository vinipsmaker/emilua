#include <iostream>

#include <CLI/CLI.hpp>

#include <boost/asio/io_context.hpp>

#include <emilua/state.hpp>

namespace asio = boost::asio;

int main(int argc, char *argv[])
{
    LUAJIT_VERSION_SYM();

    // TODO: add detection
    emilua::stdout_has_color = true;

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

    ioctx.run();

    return exit_code;
}
