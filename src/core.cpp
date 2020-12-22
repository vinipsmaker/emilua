/* Copyright (c) 2020 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

#include <cstdio>
#include <new>

#include <fmt/format.h>

#include <boost/hana/maximum.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/plus.hpp>

#include <emilua/detail/core.hpp>
#include <emilua/fiber.hpp>
#include <emilua/actor.hpp>

namespace emilua {

bool stdout_has_color;
char raw_unpack_key;
char raw_xpcall_key;
char raw_pcall_key;

namespace detail {
char context_key;
char error_code_mt_key;
char error_category_mt_key;
} // namespace detail

vm_context::vm_context(std::shared_ptr<emilua::app_context> appctx,
                       asio::io_context::strand strand)
    : app_context(std::move(appctx))
    , strand_(std::move(strand))
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

        if (!suppress_tail_errors && failed_cleanup_handler_coro) {
            std::string_view red{"\033[31;1m"};
            std::string_view reset_red{"\033[22;39m"};
            if (!stdout_has_color)
                red = reset_red = {};

            constexpr auto spec{FMT_STRING(
                "{}VM {:p} forcibly closed due to error raised"
                " on cleanup handler from coroutine {:p}{}\n"
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
                red, static_cast<void*>(L_), failed_cleanup_handler_coro,
                reset_red);
            suppress_tail_errors = true;
        }

        if (!suppress_tail_errors && deadlock_errors.size() != 0) {
            std::string_view red{"\033[31;1m"};
            std::string_view dim{"\033[2m"};
            std::string_view reset_red{"\033[22;39m"};
            std::string_view reset_dim{"\033[22m"};
            if (!stdout_has_color)
                red = dim = reset_red = reset_dim = {};

            constexpr auto spec{FMT_STRING(
                "{}Possible deadlock(s) detected during VM {:p} shutdown{}:\n"
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
    inbox.recv_fiber = nullptr;
    inbox.open = false;
    inbox.work_guard.reset();

    for (auto& m: inbox.incoming) {
        m.wake_on_destruct = true;
    }
    inbox.incoming.clear();

    pending_operations.clear_and_dispose([](pending_operation* op) {
        op->cancel();

        if (!op->shared_ownership) {
            // `vm_context` destructor calls `close()`, but only if `close()`
            // hasn't been called before. This means that if extra operations
            // were appended, they won't be deleted here (unless `close()` is
            // again called explicitly somewhere else).
            //
            // However there is no leak. We don't need a different
            // behaviour. The pattern under which pending operations fit is fine
            // for this implementation. Pending operations will always hold a
            // strong reference to `vm_context` anyway, so they can't rely on
            // `vm_context` destructor anyway, and Boost.Intrusive's auto-unlink
            // properties already make everything work kinda of automatically.
            delete op;
        }
    });
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
    if (lua_errmem || failed_cleanup_handler_coro) {
        close();
        return;
    }
    if (!lua_checkstack(current_fiber_, LUA_MINSTACK)) {
        lua_errmem = true;
        close();
        return;
    }
    switch (resume_result) {
    case LUA_YIELD:
        // Nothing to do. Fiber is awakened by the completion handler.
        break;
    case 0: //< finished without errors
    case LUA_ERRRUN: { //< a runtime error
#ifndef NDEBUG
        if (resume_result == LUA_ERRRUN &&
            lua_type(current_fiber_, -1) == LUA_TSTRING) {
            assert(tostringview(current_fiber_, -1) !=
                   "cannot resume non-suspended coroutine");
        }
#endif // NDEBUG

        rawgetp(current_fiber_, LUA_REGISTRYINDEX, &fiber_list_key);

        lua_pushthread(current_fiber_);
        lua_rawget(current_fiber_, -2);
        lua_rawgeti(current_fiber_, -1, FiberDataIndex::JOINER);
        int joiner_type = lua_type(current_fiber_, -1);
        if (joiner_type == LUA_TBOOLEAN) {
            // Detached
            assert(lua_toboolean(current_fiber_, -1) == 0);
            bool is_main = L() == current_fiber_;

            if (is_main) {
                lua_pushlightuserdata(current_fiber_, &inbox_key);
                lua_pushnil(current_fiber_);
                lua_rawset(current_fiber_, LUA_REGISTRYINDEX);

                if (!inbox.imported) {
                    inbox.open = false;
                    for (auto& m: inbox.incoming) {
                        m.wake_on_destruct = true;
                    }
                    inbox.incoming.clear();
                }
            }

            if (resume_result == LUA_ERRRUN) {
                lua_rawgeti(current_fiber_, -2, FiberDataIndex::STACKTRACE);
                lua_pushvalue(current_fiber_, -5);
                auto err_obj = inspect_errobj(current_fiber_);
                if (auto e = std::get_if<std::error_code>(&err_obj) ;
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
            if (!lua_checkstack(joiner, nret + 1 + LUA_MINSTACK)) {
                lua_errmem = true;
                close();
                return;
            }
            lua_pushboolean(joiner, resume_result == 0);
            lua_xmove(current_fiber_, joiner, nret);

            bool interruption_caught = false;
            if (resume_result == LUA_ERRRUN) {
                auto err_obj = inspect_errobj(joiner);
                if (auto e = std::get_if<std::error_code>(&err_obj) ;
                    e && *e == errc::interrupted) {
                    lua_pop(joiner, 2);
                    lua_pushboolean(joiner, 1);
                    nret = 0;
                    interruption_caught = true;
                }
            }
            if (join_handle) {
                join_handle->fiber = nullptr;
                join_handle->interruption_caught = interruption_caught;
            } else {
                // do nothing; this branch executes only for modules' fibers
                // and modules' fibers never expose a join handle to the user
            }

            fiber_prologue(joiner);
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

void vm_context::notify_deadlock(std::string msg)
{
    deadlock_errors.emplace_back(std::move(msg));
}

void vm_context::notify_cleanup_error(lua_State* coro)
{
    failed_cleanup_handler_coro = coro;
}

vm_context& get_vm_context(lua_State* L)
{
    rawgetp(L, LUA_REGISTRYINDEX, &detail::context_key);
    auto ret = reinterpret_cast<vm_context*>(lua_touserdata(L, -1));
    assert(ret);
    lua_pop(L, 1);
    return *ret;
}

void push(lua_State* L, const std::error_code& ec)
{
    if (!ec) {
        lua_pushnil(L);
        return;
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
}

std::variant<std::string_view, std::error_code> inspect_errobj(lua_State* L)
{
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

std::string errobj_to_string(std::variant<std::string_view, std::error_code> o)
{
    return std::visit(
        [](auto&& arg) -> std::string {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::string_view>)
                return std::string{arg};
            else if constexpr (std::is_same_v<T, std::error_code>)
                return arg.category().message(arg.value());
        },
        o
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
    return "emilua.core";
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
    case static_cast<int>(errc::unmatched_scope_cleanup):
        return "scope_cleanup_pop() called w/o a matching scope_cleanup_push()";
    case static_cast<int>(errc::channel_closed):
        return "Channel closed";
    case static_cast<int>(errc::no_senders):
        return "Broadcast the address before attempting to receive on it";
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
        push(L, emilua::errc::forbid_suspend_block);
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
        push(L, emilua::errc::interrupted);
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
        push(L, emilua::errc::forbid_suspend_block);
        return false;
    }
    lua_pop(L, 3);
    return true;
}

} // namespace emilua
