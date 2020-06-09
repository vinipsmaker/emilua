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
char error_code_mt_key;
char error_category_mt_key;
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

            suppress_tail_errors = true;
        }

        lua_close(L_);

        if (!suppress_tail_errors && deadlock_errors.size() != 0) {
            std::string_view red{"\033[31;1m"};
            std::string_view dim{"\033[2m"};
            std::string_view reset_red{"\033[22;39m"};
            std::string_view reset_dim{"\033[22m"};
            if (!stdout_has_color)
                red = dim = reset_red = reset_dim = {};

            constexpr auto spec{FMT_STRING(
                "{}Possible deadlock(s) detected during VM {} shutdown{}:\n"
                "{}{}{}"
            )};
            std::string errors;
            for (auto& e: deadlock_errors) {
                errors.push_back('\t');
                errors += e;
                errors.push_back('\n');
            }
            fmt::print(
                stderr,
                spec,
                red, static_cast<void*>(L_), reset_red,
                dim, errors, reset_dim);
        }
    }

    valid_ = false;
    L_ = nullptr;
}

void vm_context::fiber_prologue_trivial(lua_State* new_current_fiber)
{
    if (!valid_)
        throw dead_vm_error{};

    assert(lua_status(new_current_fiber) == 0 ||
           lua_status(new_current_fiber) == LUA_YIELD);
    current_fiber_ = new_current_fiber;
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
            /*code branches=*/hana::maximum(hana::tuple_c<int, 2, 1, 1>);
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
                bool is_main = L() == current_fiber_;
                luaL_traceback(current_fiber_, current_fiber_, nullptr, 1);
                lua_pushvalue(current_fiber_, -5);
                auto err_obj = inspect_errobj(current_fiber_);
                static_assert(std::is_same_v<decltype(err_obj)::error_type,
                                             std::bad_alloc>,
                              "");
                if (!err_obj) {
                    lua_errmem = true;
                    close();
                    return;
                }
                if (auto e = std::get_if<std::error_code>(&err_obj.value()) ;
                    !e || *e != errc::interrupted || is_main) {
                    print_panic(current_fiber_, is_main,
                                errobj_to_string(err_obj),
                                tostringview(current_fiber_, -2));
                }
                if (is_main) {
                    // TODO: call uncaught-hook
                    suppress_tail_errors = true;
                    close();
                    return;
                }
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

            lua_rawgeti(current_fiber_, -2, FiberDataIndex::USER_HANDLE);
            auto join_handle = reinterpret_cast<fiber_handle*>(
                lua_touserdata(current_fiber_, -1));

            lua_pop(current_fiber_, 4);

            int nret = (resume_result == 0) ? lua_gettop(current_fiber_) : 1;
            if (!lua_checkstack(joiner, nret + 1)) {
                lua_errmem = true;
                close();
                return;
            }
            lua_pushboolean(joiner, resume_result == 0);
            lua_xmove(current_fiber_, joiner, nret);

            bool interruption_caught = false;
            if (resume_result == LUA_ERRRUN) {
                auto err_obj = inspect_errobj(joiner);
                static_assert(std::is_same_v<decltype(err_obj)::error_type,
                                             std::bad_alloc>,
                              "");
                if (!err_obj) {
                    lua_errmem = true;
                    close();
                    return;
                }
                if (auto e = std::get_if<std::error_code>(&err_obj.value()) ;
                    e && *e == errc::interrupted) {
                    lua_pop(joiner, 2);
                    lua_pushboolean(joiner, 1);
                    nret = 0;
                    interruption_caught = true;
                }
            }
            if (join_handle) {
                join_handle->interruption_caught = interruption_caught;
            } else {
                // do nothing; this branch executes only for modules' fibers
                // and modules' fibers never expose a join handle to the user
            }

            fiber_prologue(joiner);
            reclaim_reserved_zone();
            int res = lua_resume(joiner, nret + 1);
            // I'm assuming the compiler will eliminate this tail recursive call
            // or else we may experience stack overflow on really really long
            // join()-chains. Still better than the round-trip of post
            // semantics.
            return fiber_epilogue(res);
        } else {
            // Handle still alive
            lua_pushinteger(
                current_fiber_,
                (resume_result == 0)
                ? FiberStatus::FINISHED_SUCCESSFULLY
                : FiberStatus::FINISHED_WITH_ERROR);
            lua_rawseti(current_fiber_, -3, FiberDataIndex::STATUS);
            lua_pop(current_fiber_, 3);
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

void vm_context::notify_deadlock(std::string msg)
{
    deadlock_errors.emplace_back(std::move(msg));
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
        rawgetp(L, LUA_REGISTRYINDEX, &detail::error_category_mt_key);
        lua_setmetatable(L, -2);
    }
    lua_rawset(L, -3);

    rawgetp(L, LUA_REGISTRYINDEX, &detail::error_code_mt_key);
    lua_setmetatable(L, -2);
    return outcome::success();
}

result<std::variant<std::string_view, std::error_code>, std::bad_alloc>
inspect_errobj(lua_State* L)
{
    if (!lua_checkstack(L, 4))
        return std::bad_alloc{};

    std::variant<std::string_view, std::error_code> ret = std::string_view{};

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
        rawgetp(L, LUA_REGISTRYINDEX, &detail::error_category_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            lua_pop(L, 4);
            break;
        }
        cat = *reinterpret_cast<std::error_category**>(lua_touserdata(L, -3));
        ret = std::error_code{ev, *cat};
        lua_pop(L, 4);
        break;
    }
    }

    return ret;
}

std::string errobj_to_string(
    result<std::variant<std::string_view, std::error_code>, std::bad_alloc> o)
{
    if (!o)
        return {};

    return std::visit(
        [](auto&& arg) -> std::string {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::string_view>)
                return std::string{arg};
            else if constexpr (std::is_same_v<T, std::error_code>)
                return arg.category().message(arg.value());
        },
        o.value()
    );
}

void set_interrupter(lua_State* L)
{
    bool interruption_disabled;

    rawgetp(L, LUA_REGISTRYINDEX, &fiber_list_key);
    lua_pushthread(L);
    lua_rawget(L, -2);
    lua_rawgeti(L, -1, FiberDataIndex::INTERRUPTION_DISABLED);
    switch (lua_type(L, -1)) {
    case LUA_TBOOLEAN:
        interruption_disabled = lua_toboolean(L, -1);
        break;
    case LUA_TNUMBER:
        interruption_disabled = lua_tointeger(L, -1) > 0;
        break;
    default: assert(false);
    }
    if (interruption_disabled) {
        lua_pop(L, 4);
        return;
    }

    lua_pushvalue(L, -4);
    lua_rawseti(L, -3, FiberDataIndex::INTERRUPTER);
    lua_pop(L, 4);
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
    case static_cast<int>(errc::interruption_already_allowed):
        return "Interrupt-ability already allowed";
    case static_cast<int>(errc::forbid_suspend_block):
        return "Operation not permitted within a forbid-suspend block";
    case static_cast<int>(errc::interrupted):
        return "Fiber interrupted";
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

bool detail::unsafe_can_suspend(vm_context& vm_ctx, lua_State* L)
{
    auto current_fiber = vm_ctx.current_fiber();
    rawgetp(L, LUA_REGISTRYINDEX, &fiber_list_key);
    lua_pushthread(current_fiber);
    lua_xmove(current_fiber, L, 1);
    lua_rawget(L, -2);
    lua_rawgeti(L, -1, FiberDataIndex::SUSPENSION_DISALLOWED);
    if (lua_tointeger(L, -1) != 0) {
        push(L, emilua::errc::forbid_suspend_block).value();
        return false;
    }
    lua_rawgeti(L, -2, FiberDataIndex::INTERRUPTION_DISABLED);
    bool interruption_disabled;
    switch (lua_type(L, -1)) {
    case LUA_TBOOLEAN:
        interruption_disabled = lua_toboolean(L, -1);
        break;
    case LUA_TNUMBER:
        interruption_disabled = lua_tointeger(L, -1) > 0;
        break;
    default: assert(false);
    }
    if (interruption_disabled) {
        lua_pop(L, 4);
        return true;
    }
    lua_rawgeti(L, -3, FiberDataIndex::INTERRUPTED);
    if (lua_toboolean(L, -1) == 1) {
        push(L, emilua::errc::interrupted).value();
        return false;
    }
    lua_pop(L, 5);
    return true;
}

bool detail::unsafe_can_suspend2(vm_context& vm_ctx, lua_State* L)
{
    auto current_fiber = vm_ctx.current_fiber();
    rawgetp(L, LUA_REGISTRYINDEX, &fiber_list_key);
    lua_pushthread(current_fiber);
    lua_xmove(current_fiber, L, 1);
    lua_rawget(L, -2);
    lua_rawgeti(L, -1, FiberDataIndex::SUSPENSION_DISALLOWED);
    if (lua_tointeger(L, -1) != 0) {
        push(L, emilua::errc::forbid_suspend_block).value();
        return false;
    }
    lua_pop(L, 3);
    return true;
}

} // namespace emilua
