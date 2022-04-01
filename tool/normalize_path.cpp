#include <string_view>
#include <filesystem>
#include <string>
#include <cstdio>

#include <boost/predef/os/windows.h>

extern "C" {
#include <lauxlib.h>
#include <lualib.h>
#include <lua.h>
}

// this function leaks memory, but it doesn't matter for this program
std::string_view as_lua_prints(std::string u8path)
{
    static constexpr std::string_view lua_source = "error('', 0)";

    lua_State* L = luaL_newstate();
    lua_pushcfunction(L, luaopen_base);
    lua_call(L, 0, 0);
    u8path.insert(0, "@");
    luaL_loadbuffer(L, lua_source.data(), lua_source.size(), u8path.data());
    lua_resume(L, 0);
    luaL_traceback(L, L, nullptr, /*level=*/1);
    auto traceback = [L](){
        std::size_t len;
        const char* buf = lua_tolstring(L, -1, &len);
        return std::string_view{buf, len};
    }();
    traceback.remove_prefix(traceback.find('\n') + 2);
    traceback = traceback.substr(0, traceback.find(":1: "));
    return traceback;
}

void write_ors()
{
    // NIL is a good RS as it doesn't appear in UTF-8 streams
    fwrite("\0", 1, 1, stdout);
}

int main()
{
    std::string buffer;
    buffer.resize(8192);

    {
        std::size_t nread;
        std::size_t used = 0;
        do {
            if (used == buffer.size()) buffer.resize(buffer.size() * 2);
            nread = fread(buffer.data() + used, sizeof(char),
                          buffer.size() - used, stdin);
            used += nread;
        } while (nread > 0);
        buffer.resize(used);
    }

#if BOOST_OS_WINDOWS
    {
        std::filesystem::path p{buffer, std::filesystem::path::native_format};
        p.make_preferred();
        buffer = p.u8string();
    }
#endif // BOOST_OS_WINDOWS

    buffer += ".lua";
    auto ret = as_lua_prints(buffer);
    ret.remove_suffix(4);
    buffer.resize(buffer.size() - 4);
    fwrite(ret.data(), ret.size(), 1, stdout);
    write_ors();

    buffer += "_foo.lua";
    ret = as_lua_prints(buffer);
    ret.remove_suffix(8);
    buffer.resize(buffer.size() - 8);
    fwrite(ret.data(), ret.size(), 1, stdout);
    write_ors();
}
