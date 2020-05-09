#pragma once

#include <boost/asio/io_context.hpp>

#include <system_error>
#include <string_view>
#include <filesystem>
#include <mutex>

extern "C" {
#include <lauxlib.h>
#include <luajit.h>
#include <lua.h>
}

namespace emilua {

namespace detail {
template<class Executor>
class remap_post_to_defer: private Executor
{
public:
    remap_post_to_defer(const remap_post_to_defer&) = default;
    remap_post_to_defer(remap_post_to_defer&&) = default;

    explicit remap_post_to_defer(const Executor& ex)
        : Executor(ex)
    {}

    explicit remap_post_to_defer(Executor&& ex)
        : Executor(std::move(ex))
    {}

    bool operator==(const remap_post_to_defer& o) const noexcept
    {
        return static_cast<const Executor&>(*this) ==
            static_cast<const Executor&>(o);
    }

    bool operator!=(const remap_post_to_defer& o) const noexcept
    {
        return static_cast<const Executor&>(*this) !=
            static_cast<const Executor&>(o);
    }

    decltype(std::declval<Executor>().context())
    context() const noexcept
    {
        return Executor::context();
    }

    void on_work_started() const noexcept
    {
        Executor::on_work_started();
    }

    void on_work_finished() const noexcept
    {
        Executor::on_work_finished();
    }

    template<class F, class A>
    void dispatch(F&& f, const A& a) const
    {
        Executor::dispatch(std::forward<F>(f), a);
    }

    template<class F, class A>
    void post(F&& f, const A& a) const
    {
        Executor::defer(std::forward<F>(f), a);
    }

    template<class F, class A>
    void defer(F&& f, const A& a) const
    {
        Executor::defer(std::forward<F>(f), a);
    }
};
} // namespace detail

class service: public boost::asio::io_context::service
{
private:
    struct path_hash
    {
        std::size_t operator()(const std::filesystem::path& p) const noexcept
        {
            return std::filesystem::hash_value(p);
        }
    };

public:
    using key_type = service;
    explicit service(boost::asio::io_context& ctx)
        : boost::asio::io_context::service(ctx)
    {}

    std::unordered_map<std::filesystem::path, std::string, path_hash>
        modules_cache_registry;
    std::mutex modules_cache_registry_mtx;

    static boost::asio::io_context::id id;
};

class vm_context: public std::enable_shared_from_this<vm_context>
{
public:
    vm_context(boost::asio::io_context::strand strand);
    ~vm_context();

    vm_context(const vm_context&) = delete;
    vm_context(vm_context&&) = delete;

    vm_context& operator=(const vm_context&) = delete;
    vm_context& operator=(vm_context&&) = delete;

    const boost::asio::io_context::strand& strand()
    {
        return strand_;
    }

    detail::remap_post_to_defer<boost::asio::io_context::strand>
    strand_using_defer()
    {
        return detail::remap_post_to_defer<boost::asio::io_context::strand>{
            strand_
        };
    }

    lua_State* L()
    {
        return L_;
    }

    void close();

    lua_State* current_fiber()
    {
        return current_fiber_;
    }

    bool valid()
    {
        return valid_;
    }

    void fiber_prologue(lua_State* new_current_fiber);
    void fiber_epilogue(int resume_result);

    void notify_errmem();

private:
    boost::asio::io_context::strand strand_;
    bool valid_;
    bool lua_errmem;
    lua_State* L_;
    lua_State* current_fiber_;
};

vm_context& get_vm_context(lua_State* L);

void push(lua_State* L, const std::error_code& ec);

inline void push(lua_State* L, std::errc ec)
{
    return push(L, make_error_code(ec));
}

inline void push(lua_State* L, std::string_view str)
{
    lua_pushlstring(L, str.data(), str.size());
}

inline void push(lua_State* L, const std::string& str)
{
    push(L, std::string_view{str});
}

inline void push(lua_State* L, const std::filesystem::path& path)
{
    auto p = path.native();
    lua_pushlstring(L, p.data(), p.size());
}

inline std::string_view tostringview(lua_State* L, int index = -1)
{
    std::size_t len;
    const char* buf = lua_tolstring(L, index, &len);
    return std::string_view{buf, len};
}

inline void rawgetp(lua_State* L, int index, const void* p)
{
    lua_pushlightuserdata(L, const_cast<void*>(p));
    lua_rawget(L, index);
}

enum class lua_errc
{
    file = LUA_ERRFILE,
    syntax = LUA_ERRSYNTAX,
    run = LUA_ERRRUN,
    err = LUA_ERRERR,
    mem = LUA_ERRMEM,
};

const std::error_category& lua_category();

inline std::error_code make_error_code(lua_errc e)
{
    return std::error_code{static_cast<int>(e), lua_category()};
}

class lua_exception: public std::system_error
{
public:
    lua_exception(int ev);
    lua_exception(int ev, const std::string& what_arg);
    lua_exception(int ev, const char* what_arg);
    lua_exception(lua_errc ec);
    lua_exception(lua_errc ec, const std::string& what_arg);
    lua_exception(lua_errc ec, const char* what_arg);
    lua_exception(const lua_exception&) noexcept = default;

    lua_exception& operator=(const lua_exception&) noexcept = default;
};

enum class errc {
    invalid_module_name = 1,
    module_not_found,
    root_cannot_import_parent,
    cyclic_import,
    leaf_cannot_import_child,
    failed_to_load_module,
    only_main_fiber_may_import,
    bad_root_context,
    bad_index,
};

const std::error_category& category();

inline std::error_code make_error_code(errc e)
{
    return std::error_code{static_cast<int>(e), category()};
}

class exception: public std::system_error
{
public:
    exception(int ev);
    exception(int ev, const std::string& what_arg);
    exception(int ev, const char* what_arg);
    exception(errc ec);
    exception(errc ec, const std::string& what_arg);
    exception(errc ec, const char* what_arg);
    exception(const exception&) noexcept = default;

    exception& operator=(const exception&) noexcept = default;
};

} // namespace emilua

template<>
struct std::is_error_code_enum<emilua::lua_errc>: std::true_type {};

template<>
struct std::is_error_code_enum<emilua::errc>: std::true_type {};
