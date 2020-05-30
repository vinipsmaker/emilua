#include <cstdio>
#include <new>

#include <fmt/format.h>

#include <boost/hana/maximum.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/plus.hpp>

#include <emilua/detail/core.hpp>
#include <emilua/fiber.hpp>

namespace emilua {

bool stdout_has_color;
char raw_unpack_key;

namespace detail {
char context_key;
char error_code_key;
char error_category_key;
} // namespace detail

boost::asio::io_context::id service::id;

vm_context::vm_context(asio::io_context::strand strand)
    : strand_(std::move(strand))
    , valid_(true)
    , lua_errmem(false)
    , L_(luaL_newstate())
    , current_fiber_(nullptr)
{
    if (!L_)
        throw std::bad_alloc{};
}

vm_context::~vm_context()
{
    if (valid_)
        close();
}

void vm_context::close()
{
    if (valid_) {
        if (lua_errmem) {
            constexpr auto spec{FMT_STRING(
                "{}VM {:p} forcibly closed due to '{}LUA_ERRMEM{}'{}\n"
            )};

            std::string_view red{"\033[31;1m"};
            std::string_view underline{"\033[4m"};
            std::string_view reset_red{"\033[22;39m"};
            std::string_view reset_underline{"\033[24m"};
            if (!stdout_has_color)
                red = underline = reset_red = reset_underline = {};

            fmt::print(
                stderr,
                spec,
                red,
                static_cast<const void*>(L_),
                underline, reset_underline,
                reset_red);
        }

        lua_close(L_);
    }

    valid_ = false;
    L_ = nullptr;
}

[[gnu::flatten]]
void vm_context::fiber_prologue(lua_State* new_current_fiber)
{
    if (!valid_)
        throw dead_vm_error{};

    assert(lua_status(new_current_fiber) == 0 ||
           lua_status(new_current_fiber) == LUA_YIELD);
    current_fiber_ = new_current_fiber;
    if (!lua_checkstack(current_fiber_, LUA_MINSTACK)) {
        lua_errmem = true;
        close();
        throw dead_vm_error{dead_vm_error::reason::mem};
    }
    enable_reserved_zone();
}

void vm_context::fiber_epilogue(int resume_result)
{
    assert(valid_);
    if (lua_errmem) {
        close();
        return;
    }
    switch (resume_result) {
    case LUA_YIELD:
        // Nothing to do. Fiber is awakened by the completion handler.
        break;
    case 0: //< finished without errors
    case LUA_ERRRUN: { //< a runtime error
        constexpr int extra = /*common path=*/3 +
            /*code branches=*/hana::maximum(hana::tuple_c<int, 2, 1, 2>);
        if (!lua_checkstack(current_fiber_, extra)) {
            lua_errmem = true;
            close();
            return;
        }

        rawgetp(current_fiber_, LUA_REGISTRYINDEX, &fiber_list_key);

        lua_pushthread(current_fiber_);
        lua_rawget(current_fiber_, -2);
        lua_rawgeti(current_fiber_, -1, FiberDataIndex::JOINER);
        int joiner_type = lua_type(current_fiber_, -1);
        if (joiner_type == LUA_TBOOLEAN) {
            // Detached
            assert(lua_toboolean(current_fiber_, -1) == 0);

            if (resume_result == LUA_ERRRUN) {
                luaL_traceback(current_fiber_, current_fiber_, nullptr, 1);
                lua_pushvalue(current_fiber_, -5);
                result<std::string, std::bad_alloc> err_str =
                    errobj_to_string(current_fiber_);
                if (!err_str) {
                    lua_errmem = true;
                    close();
                    return;
                }
                print_panic(current_fiber_, /*is_main=*/false, err_str.value(),
                            tostringview(current_fiber_, -2));
                lua_pop(current_fiber_, 2);
            }

            lua_pushthread(current_fiber_);
            lua_pushnil(current_fiber_);
            lua_rawset(current_fiber_, -5);
            // TODO (?): force a full GC round on `L()` now
        } else if (joiner_type == LUA_TTHREAD) {
            // Joined
            lua_State* joiner = lua_tothread(current_fiber_, -1);

            lua_pushinteger(
                current_fiber_,
                (resume_result == 0)
                ? FiberStatus::FINISHED_SUCCESSFULLY
                : FiberStatus::FINISHED_WITH_ERROR);
            lua_rawseti(current_fiber_, -3, FiberDataIndex::STATUS);

            lua_pop(current_fiber_, 3);

            int nret = (resume_result == 0) ? lua_gettop(current_fiber_) : 1;
            if (!lua_checkstack(joiner, nret + 1)) {
                lua_errmem = true;
                close();
                return;
            }
            lua_pushboolean(joiner, resume_result == 0);
            lua_xmove(current_fiber_, joiner, nret);

            fiber_prologue(joiner);
            int res = lua_resume(joiner, nret + 1);
            // I'm assuming the compiler will eliminate this tail recursive call
            // or else we may experience stack overflow on really really long
            // join()-chains. Still better than the round-trip of post
            // semantics.
            return fiber_epilogue(res);
        } else {
            // Handle still alive
            if (resume_result == LUA_ERRRUN && L() == current_fiber_) {
                // TODO: call uncaught-hook
                luaL_traceback(L(), L(), nullptr, 1);
                lua_pushvalue(L(), -5);
                result<std::string, std::bad_alloc> err_str =
                    errobj_to_string(L());
                if (!err_str)
                    err_str = outcome::success();
                print_panic(L(), /*is_main=*/true, err_str.value(),
                            tostringview(L(), -2));
                close();
                return;
            } else {
                lua_pushinteger(
                    current_fiber_,
                    (resume_result == 0)
                    ? FiberStatus::FINISHED_SUCCESSFULLY
                    : FiberStatus::FINISHED_WITH_ERROR);
                lua_rawseti(current_fiber_, -3, FiberDataIndex::STATUS);
                lua_pop(current_fiber_, 3);
            }
        }
        break;
    }
    case LUA_ERRMEM: //< memory allocation error
        lua_errmem = true;
        close();
        break;
    case LUA_ERRERR:
    default:
        assert(false);
    }
}

void vm_context::notify_errmem()
{
    lua_errmem = true;
}

void vm_context::enable_reserved_zone()
{
    // TODO: when per-VM allocator is ready
    // it should be safe to be called multiple times in a row
}

void vm_context::reclaim_reserved_zone()
{
    // TODO: when per-VM allocator is ready
    // this function may throw LUA_ERRMEM and call close()
}

vm_context& get_vm_context(lua_State* L)
{
    rawgetp(L, LUA_REGISTRYINDEX, &detail::context_key);
    auto ret = reinterpret_cast<vm_context*>(lua_touserdata(L, -1));
    assert(ret);
    lua_pop(L, 1);
    return *ret;
}

result<void, std::bad_alloc> push(lua_State* L, const std::error_code& ec)
{
    if (!lua_checkstack(L, 4))
        return std::bad_alloc{};

    if (!ec) {
        lua_pushnil(L);
        return outcome::success();
    }

    lua_createtable(L, /*narr=*/0, /*nrec=*/2);

    lua_pushliteral(L, "val");
    lua_pushinteger(L, ec.value());
    lua_rawset(L, -3);

    lua_pushliteral(L, "cat");
    {
        *reinterpret_cast<const std::error_category**>(
            lua_newuserdata(L, sizeof(void*))) = &ec.category();
        rawgetp(L, LUA_REGISTRYINDEX, &detail::error_category_key);
        lua_setmetatable(L, -2);
    }
    lua_rawset(L, -3);

    rawgetp(L, LUA_REGISTRYINDEX, &detail::error_code_key);
    lua_setmetatable(L, -2);
    return outcome::success();
}

result<std::string, std::bad_alloc> errobj_to_string(lua_State* L)
{
    if (!lua_checkstack(L, 4))
        return std::bad_alloc{};

    std::string ret;

    switch (lua_type(L, -1)) {
    case LUA_TSTRING:
        ret = tostringview(L, -1);
        break;
    case LUA_TTABLE: {
        int ev;
        std::error_category* cat;
        lua_pushliteral(L, "val");
        lua_rawget(L, -2);
        if (lua_type(L, -1) != LUA_TNUMBER) {
            lua_pop(L, 1);
            break;
        }
        ev = lua_tointeger(L, -1);
        lua_pushliteral(L, "cat");
        lua_rawget(L, -3);
        if (lua_type(L, -1) != LUA_TUSERDATA || !lua_getmetatable(L, -1)) {
            lua_pop(L, 2);
            break;
        }
        rawgetp(L, LUA_REGISTRYINDEX, &detail::error_category_key);
        if (!lua_rawequal(L, -1, -2)) {
            lua_pop(L, 4);
            break;
        }
        cat = *reinterpret_cast<std::error_category**>(lua_touserdata(L, -3));
        ret = cat->message(ev);
        lua_pop(L, 4);
        break;
    }
    }

    return std::move(ret);
}

class lua_category_impl: public std::error_category
{
public:
    const char* name() const noexcept override;
    std::string message(int value) const noexcept override;
};

const char* lua_category_impl::name() const noexcept
{
    return "lua";
}

std::string lua_category_impl::message(int value) const noexcept
{
    switch (value) {
    case static_cast<int>(lua_errc::file):
        return "cannot open/read the file";
    case static_cast<int>(lua_errc::syntax):
        return "syntax error during pre-compilation";
    case static_cast<int>(lua_errc::run):
        return "runtime error";
    case static_cast<int>(lua_errc::err):
        return "error while running the error handler function";
    case static_cast<int>(lua_errc::mem):
        return "memory allocation error";
    default:
        return {};
    }
}

const std::error_category& lua_category()
{
    static lua_category_impl cat;
    return cat;
}

lua_exception::lua_exception(int ev)
    : std::system_error{ev, lua_category()}
{}

lua_exception::lua_exception(int ev, const std::string& what_arg)
    : std::system_error{ev, lua_category(), what_arg}
{}

lua_exception::lua_exception(int ev, const char* what_arg)
    : std::system_error{ev, lua_category(), what_arg}
{}

lua_exception::lua_exception(lua_errc ec)
    : std::system_error{ec}
{}

lua_exception::lua_exception(lua_errc ec, const std::string& what_arg)
    : std::system_error{ec, what_arg}
{}

lua_exception::lua_exception(lua_errc ec, const char* what_arg)
    : std::system_error{ec, what_arg}
{}

class category_impl: public std::error_category
{
public:
    const char* name() const noexcept override;
    std::string message(int value) const noexcept override;
};

const char* category_impl::name() const noexcept
{
    return "emilua";
}

std::string category_impl::message(int value) const noexcept
{
    switch (value) {
    case static_cast<int>(errc::invalid_module_name):
        return "Cannot have a module with this name";
    case static_cast<int>(errc::module_not_found):
        return "Module not found";
    case static_cast<int>(errc::root_cannot_import_parent):
        return "The root module doesn't have a parent and can't reference one";
    case static_cast<int>(errc::cyclic_import):
        return "The module you're trying to import has a dependency on the"
            " current module (and it is partially loaded already)";
    case static_cast<int>(errc::leaf_cannot_import_child):
        return "A leaf module cannot import child modules";
    case static_cast<int>(errc::failed_to_load_module):
        return "Error when opening/loading module file (maybe invalid lua"
            " syntax)";
    case static_cast<int>(errc::only_main_fiber_may_import):
        return "You can only import modules from the main fiber";
    case static_cast<int>(errc::bad_root_context):
        return "Bad root context";
    case static_cast<int>(errc::bad_index):
        return "Requested key wasn't found in the table/userdata";
    case static_cast<int>(errc::bad_coroutine):
        return "The fiber coroutine is reserved to the scheduler";
    case static_cast<int>(errc::suspension_already_allowed):
        return "Suspension already allowed";
    case static_cast<int>(errc::forbid_suspend_block):
        return "Operation not permitted within a forbid-suspend block";
    default:
        return {};
    }
}

const std::error_category& category()
{
    static category_impl cat;
    return cat;
}

exception::exception(int ev)
    : std::system_error{ev, category()}
{}

exception::exception(int ev, const std::string& what_arg)
    : std::system_error{ev, category(), what_arg}
{}

exception::exception(int ev, const char* what_arg)
    : std::system_error{ev, category(), what_arg}
{}

exception::exception(errc ec)
    : std::system_error{ec}
{}

exception::exception(errc ec, const std::string& what_arg)
    : std::system_error{ec, what_arg}
{}

exception::exception(errc ec, const char* what_arg)
    : std::system_error{ec, what_arg}
{}

lua_Integer detail::unsafe_suspension_disallowed_count(
    vm_context& vm_ctx, lua_State* L)
{
    auto current_fiber = vm_ctx.current_fiber();
    rawgetp(L, LUA_REGISTRYINDEX, &fiber_list_key);
    lua_pushthread(current_fiber);
    lua_xmove(current_fiber, L, 1);
    lua_rawget(L, -2);
    lua_rawgeti(L, -1, FiberDataIndex::SUSPENSION_DISALLOWED);
    auto count = lua_tointeger(L, -1);
    lua_pop(L, 3);
    return count;
}

} // namespace emilua
