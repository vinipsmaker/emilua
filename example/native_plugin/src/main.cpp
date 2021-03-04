#include <emilua/plugin.hpp>
#include <iostream>

namespace asio = boost::asio;

class my_plugin : emilua::plugin
{
public:
    my_plugin()
    {
        std::cout << "Plugin loaded\n" << std::flush;
    }

    ~my_plugin() override
    {
        std::cout << "Plugin unloaded\n" << std::flush;
    }

    void init_appctx(emilua::app_context&) noexcept override
    {
        std::cout << "Plugin initialized\n" << std::flush;
    }

    std::error_code init_ioctx_services(asio::io_context&) noexcept override
    {
        std::cout <<
            "Plugin (re-)installed io services into current execution"
            " context\n" <<
            std::flush;
        return {};
    }

    std::error_code init_lua_module(emilua::vm_context&, lua_State* L) override
    {
        std::cout << "Plugin imported\n" << std::flush;
        lua_newtable(L);
        lua_pushliteral(L, "hello");
        lua_pushcfunction(L, hello);
        lua_rawset(L, -3);
        return {};
    }

private:
    static int hello(lua_State*)
    {
        std::cout << "Hello World\n" << std::flush;
        return 0;
    }
};

extern "C" BOOST_SYMBOL_EXPORT my_plugin EMILUA_PLUGIN_SYMBOL;
my_plugin EMILUA_PLUGIN_SYMBOL;
