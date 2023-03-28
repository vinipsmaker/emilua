/* Copyright (c) 2020, 2021 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

#pragma once

#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context_strand.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/intrusive/list.hpp>
#include <boost/predef/os/linux.h>
#include <boost/predef/os/unix.h>
#include <boost/config.hpp>

#include <boost/hana/functional/overload.hpp>
#include <boost/hana/functional/compose.hpp>
#include <boost/hana/functional/always.hpp>
#include <boost/hana/core/is_a.hpp>
#include <boost/hana/for_each.hpp>
#include <boost/hana/none_of.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/pair.hpp>
#include <boost/hana/size.hpp>
#include <boost/hana/type.hpp>
#include <boost/hana/set.hpp>

#include <boost/outcome/basic_result.hpp>
#include <boost/outcome/policy/all_narrow.hpp>
#include <boost/outcome/policy/terminate.hpp>

#include <fmt/format.h>

#include <condition_variable>
#include <unordered_map>
#include <system_error>
#include <string_view>
#include <filesystem>
#include <optional>
#include <utility>
#include <variant>
#include <atomic>
#include <deque>
#include <mutex>
#include <map>
#include <set>

extern "C" {
#include <lauxlib.h>
#include <luajit.h>
#include <lualib.h>
#include <lua.h>
}

#include <emilua/config.h>

#if EMILUA_CONFIG_ENABLE_PLUGINS
#include <boost/shared_ptr.hpp>
#endif // EMILUA_CONFIG_ENABLE_PLUGINS

#define EMILUA_GPERF_BEGIN(ID)
#define EMILUA_GPERF_END(ID) {}
#define EMILUA_GPERF_PARAM(...)
#define EMILUA_GPERF_DEFAULT_VALUE(...)
#define EMILUA_GPERF_PAIR(ID, ...)
#define EMILUA_GPERF_DECLS_BEGIN(ID)
#define EMILUA_GPERF_DECLS_END(ID)
#define EMILUA_GPERF_NAMESPACE(ID)

#define EMILUA_IMPL_INITIAL_FIBER_DATA_CAPACITY 11
#define EMILUA_IMPL_INITIAL_MODULE_FIBER_DATA_CAPACITY 8

// EMILUA_IMPL_INITIAL_MODULE_FIBER_DATA_CAPACITY currently takes into
// consideration:
//
// * STACK
// * LEAF
// * CONTEXT
// * INTERRUPTION_DISABLED
// * JOINER
// * STATUS
// * SOURCE_PATH
// * LOCAL_STORAGE

#define EMILUA_CHECK_SUSPEND_ALLOWED(VM_CTX, L)             \
    if (!emilua::detail::unsafe_can_suspend((VM_CTX), (L))) \
        return lua_error((L));

#define EMILUA_CHECK_SUSPEND_ALLOWED_ASSUMING_INTERRUPTION_DISABLED(VM_CTX, L) \
    if (!emilua::detail::unsafe_can_suspend2((VM_CTX), (L)))                   \
        return lua_error((L));

namespace boost::hana {}
namespace boost::http {}
namespace boost::nowide {}

namespace emilua {

namespace gperf::detail {
template<class T>
auto value_or(const T* p, decltype(std::declval<T>().action) default_value)
    -> decltype(default_value)
{
    return p ? p->action : default_value;
}

template<class T>
auto make_optional(const T* p)
    -> std::optional<decltype(std::declval<T>().action)>
{
    if (p)
        return p->action;
    else
        return std::nullopt;
}
} // namespace gperf::detail

using namespace std::literals::string_view_literals;
namespace outcome = BOOST_OUTCOME_V2_NAMESPACE;
namespace asio = boost::asio;
namespace hana = boost::hana;
namespace http = boost::http;
namespace nowide = boost::nowide;

extern bool stdout_has_color;
extern char raw_unpack_key;
extern char raw_xpcall_key;
extern char raw_pcall_key;
extern char raw_error_key;
extern char raw_type_key;
extern char raw_pairs_key;
extern char raw_ipairs_key;
extern char raw_next_key;
extern char raw_setmetatable_key;
extern char raw_getmetatable_key;
extern char fiber_list_key;

#if BOOST_OS_LINUX
extern void* clone_stack_address;
#endif // BOOST_OS_LINUX

enum FiberDataIndex: lua_Integer
{
    JOINER = 1,
    STATUS,
    SOURCE_PATH,
    SUSPENSION_DISALLOWED,
    LOCAL_STORAGE,
    STACKTRACE,

    // data used by the interruption system {{{
    INTERRUPTION_DISABLED,
    INTERRUPTED,
    INTERRUPTER,
    ASIO_CANCELLATION_SIGNAL,
    DEFAULT_EMIT_SIGNAL_INTERRUPTER,
    USER_HANDLE, //< "augmented joiner"
    // }}}

    // data only available for modules:
    STACK,
    LEAF,
    CONTEXT
};

template<class T, class EC = std::error_code>
using result = outcome::basic_result<
    T, EC,
#ifdef NDEBUG
    outcome::policy::all_narrow
#else
    outcome::policy::terminate
#endif // defined(NDEBUG)
>;

#if EMILUA_CONFIG_THREAD_SUPPORT_LEVEL == 2
using strand_type = asio::io_context::strand;
#elif EMILUA_CONFIG_THREAD_SUPPORT_LEVEL == 1 || \
    EMILUA_CONFIG_THREAD_SUPPORT_LEVEL == 0
using strand_type = asio::io_context::executor_type;
#else
# error Invalid thread support level
#endif

template<class T>
struct log_domain;

struct default_log_domain;

template<>
struct log_domain<default_log_domain>
{
    static std::string_view name;
    static int log_level;
};

struct TransparentStringComp
{
    using is_transparent = void;

    bool operator()(const std::string& lhs, const std::string& rhs) const
    {
        return lhs < rhs;
    }

    bool operator()(const std::string_view& lhs, const std::string& rhs) const
    {
        return lhs < rhs;
    }

    bool operator()(const std::string& lhs, const std::string_view& rhs) const
    {
        return lhs < rhs;
    }

    bool operator()(const char* lhs, const std::string& rhs) const
    {
        return lhs < rhs;
    }

    bool operator()(const std::string& lhs, const char* rhs) const
    {
        return lhs < rhs;
    }
};

struct TransparentStringHash : private std::hash<std::string_view>
{
    using is_transparent = void;

    template<class T>
    std::size_t operator()(const T& s) const
    {
        return static_cast<const std::hash<std::string_view>&>(*this)(s);
    }
};

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

#if EMILUA_CONFIG_ENABLE_PLUGINS
class BOOST_SYMBOL_VISIBLE plugin;
#endif // EMILUA_CONFIG_ENABLE_PLUGINS

class vm_context;

struct rdf_error_category : public std::error_category
{
    const char* name() const noexcept override;
    std::string message(int value) const noexcept override;
    std::error_condition default_error_condition(int code) const noexcept
        override;

    std::string name_;
    std::map<
        int,
        std::map<
            // default constructed for non-localized version
            /*locale=*/std::string,
            /*message=*/std::string
        >
    > messages;
    std::unordered_map<
        std::string, int, TransparentStringHash, std::equal_to<>
    > aliases;
    std::map<
        /*internal_code=*/int,
        /*code_from_generic_category=*/int
    > generic_errors;
};

class app_context
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
    app_context() = default;
    app_context(const app_context&) = delete;

    template<class Dom>
    void init_log_domain()
    {
        init_log_domain(log_domain<Dom>::name, log_domain<Dom>::log_level);
    }

    template<class Domain>
    void log(int priority, fmt::string_view format_str, fmt::format_args args)
    {
        if (priority > log_domain<Domain>::log_level)
            return;

        log(priority, log_domain<Domain>::name, format_str, std::move(args));
    }

#if EMILUA_CONFIG_ENABLE_LINUX_NAMESPACES
    static int linux_namespaces_service_main(int sockfd);
#endif // EMILUA_CONFIG_ENABLE_LINUX_NAMESPACES

    std::vector<std::string_view> app_args;
    std::unordered_map<std::string_view, std::string_view> app_env;
    int exit_code = 0;

    std::atomic<std::weak_ptr<vm_context>> master_vm;

    std::vector<std::filesystem::path> emilua_path;

    std::unordered_map<std::filesystem::path, std::string, path_hash>
        modules_cache_registry;
    std::unordered_map<
        std::filesystem::path, std::unique_ptr<rdf_error_category>, path_hash
    > rdf_ec_cache_registry;
#if EMILUA_CONFIG_ENABLE_PLUGINS
    std::unordered_map<std::string, boost::shared_ptr<plugin>>
        native_modules_cache_registry;
    std::set<std::string, TransparentStringComp> visited_native_modules;
#endif // EMILUA_CONFIG_ENABLE_PLUGINS
    std::mutex modules_cache_registry_mtx;

    std::size_t extra_threads_count = 0;
    std::mutex extra_threads_count_mtx;
    std::condition_variable extra_threads_count_empty_cond;

#if EMILUA_CONFIG_ENABLE_LINUX_NAMESPACES
    int linux_namespaces_service_sockfd = -1;
#endif // EMILUA_CONFIG_ENABLE_LINUX_NAMESPACES

private:
    void init_log_domain(std::string_view name, int& log_level);
    void log(int priority, std::string_view domain, fmt::string_view format_str,
             fmt::format_args args);
};

class properties_service : public asio::execution_context::service
{
public:
    using key_type = properties_service;

    properties_service(asio::execution_context& ctx, int concurrency_hint);
    explicit properties_service(asio::execution_context& ctx);

    void shutdown() override;

    const int concurrency_hint;

    static asio::io_context::id id;
};

void set_interrupter(lua_State* L, vm_context& vm_ctx);
asio::cancellation_slot
set_default_interrupter(lua_State* L, vm_context& vm_ctx);

struct actor_address
{
    actor_address(vm_context& vm_ctx);
    ~actor_address();

    actor_address(actor_address&&) = default;
    actor_address(const actor_address&);

    actor_address& operator=(actor_address&& o);

    std::weak_ptr<vm_context> dest;
    asio::executor_work_guard<asio::io_context::executor_type> work_guard;
};

struct inbox_t
{
#if BOOST_OS_UNIX
    struct file_descriptor_box
    {
        file_descriptor_box()
            : value{-1}
        {}

        file_descriptor_box(int value)
            : value{value}
        {}

        file_descriptor_box(const file_descriptor_box&) = delete;
        file_descriptor_box& operator=(const file_descriptor_box&) = delete;

        ~file_descriptor_box()
        {
            if (value == -1)
                return;

            close(value);
        }

        int value;
    };
#endif // BOOST_OS_UNIX

#if EMILUA_CONFIG_ENABLE_LINUX_NAMESPACES
    struct linux_container_address
    {
        linux_container_address(std::shared_ptr<file_descriptor_box> inbox)
            : inbox{std::move(inbox)}
        {}

        std::shared_ptr<file_descriptor_box> inbox;
    };
#endif // EMILUA_CONFIG_ENABLE_LINUX_NAMESPACES

    struct value_type: std::variant<
        bool, lua_Number, std::string,
#if BOOST_OS_UNIX
        std::shared_ptr<file_descriptor_box>,
#endif // BOOST_OS_UNIX
#if EMILUA_CONFIG_ENABLE_LINUX_NAMESPACES
        linux_container_address,
#endif // EMILUA_CONFIG_ENABLE_LINUX_NAMESPACES
        std::map<std::string, value_type>,
        std::vector<value_type>,
        actor_address
    >
    {
        using variant_type = variant;

        using variant::variant;

        value_type(value_type&&) = default;
        value_type(const value_type&) = default;

        value_type& operator=(value_type&&) = default;
    };

    using value_object_type = std::map<std::string, value_type>;
    using value_array_type = std::vector<value_type>;

    struct sender_state
    {
        sender_state(vm_context& vm_ctx);
        sender_state(vm_context& vm_ctx, lua_State* fiber);
        sender_state(std::nullopt_t);
        sender_state(sender_state&& o);

        ~sender_state();

        sender_state& operator=(sender_state&& o);

        sender_state(const sender_state&) = delete;
        sender_state& operator=(const sender_state&) = delete;

        // check whether is same sender
        bool operator==(const sender_state& o)
        {
            // `msg` is ignored
            return vm_ctx == o.vm_ctx && fiber == o.fiber;
        }

        std::shared_ptr<vm_context> vm_ctx;
        asio::executor_work_guard<asio::io_context::executor_type> work_guard;
        lua_State* fiber;
        value_type msg;
        bool wake_on_destruct = false;
    };

    lua_State* recv_fiber = nullptr;
    std::deque<sender_state> incoming;
    bool open = true;
    bool imported = false;
    std::atomic_size_t nsenders = 0;
    std::shared_ptr<vm_context> work_guard;
};

// This class represents a node to be destroyed when the VM finishes
// prematurely. It can be used to register cleanup code (the `cancel()` method).
class pending_operation
    : public boost::intrusive::list_base_hook<
        boost::intrusive::link_mode<
#ifdef NDEBUG
            boost::intrusive::normal_link
#else // defined(NDEBUG)
            boost::intrusive::safe_link
#endif // defined(NDEBUG)
        >
    >
{
public:
    pending_operation(bool shared_ownership)
        : shared_ownership(shared_ownership)
    {}

    virtual ~pending_operation() noexcept = default;

    virtual void cancel() noexcept = 0;

    // If `shared_ownership`, then the runtime won't `delete` the node after it
    // is removed from the list of pending operations. It's useful if you don't
    // want to allocate `pending_operation` on the heap (and other scenarios).
    bool shared_ownership;
};

class vm_context: public std::enable_shared_from_this<vm_context>
{
public:
    struct options
    {
        // If your code hasn't set an interrupter in the first place, then
        // you're safe to use this option.
        static constexpr struct skip_clear_interrupter_t {}
            skip_clear_interrupter{};

        static constexpr struct arguments_t {} arguments{};

        // Convert from `asio::error::operation_aborted` to `errc::interrupted`
        // iff `FiberDataIndex::INTERRUPTED` has been set for the fiber about to
        // be resumed.
        //
        // If the implementation for your IO operation has the following
        // workflow:
        //
        // 1. Set interrupter to cancel the underlying IO operation
        //    (i.e. handler will be called with
        //    `ec=asio::error::operation_aborted`).
        // 2. Initiates the async operation.
        //
        // Then this option will take care of the usual boilerplate to awake the
        // fiber with the proper reason (i.e. fiber interrupted). Check
        // implementation for details.
        static constexpr
        struct auto_detect_interrupt_t {} auto_detect_interrupt{};

        // Convert from `asio::error::operation_aborted` to `errc::interrupted`.
        //
        // If the implementation for your IO operation has the following
        // workflow:
        //
        // 1. Set interrupter to cancel the underlying IO operation
        //    (i.e. handler will be called with
        //    `ec=asio::error::operation_aborted`).
        // 2. Initiates the async operation.
        //
        // Then this option will take care of the usual boilerplate to awake the
        // fiber with the proper reason (i.e. fiber interrupted). Check
        // implementation for details.
        //
        // WARNING: You can only use this option when
        // `asio::error::operation_aborted` undoubtedly signifies fiber
        // interrupted (e.g. when the IO object outlives the Lua VM and there is
        // no way for user-written Lua code to call io_object.cancel()).
        static constexpr
        struct fast_auto_detect_interrupt_t {} fast_auto_detect_interrupt{};
    };

    vm_context(app_context& appctx, strand_type strand);
    ~vm_context();

    vm_context(const vm_context&) = delete;
    vm_context(vm_context&&) = delete;

    vm_context& operator=(const vm_context&) = delete;
    vm_context& operator=(vm_context&&) = delete;

    const strand_type& strand()
    {
        return strand_;
    }

    remap_post_to_defer<strand_type> strand_using_defer()
    {
        return remap_post_to_defer<strand_type>{strand_};
    }

    asio::executor_work_guard<asio::io_context::executor_type> work_guard()
    {
        return asio::executor_work_guard<asio::io_context::executor_type>{
            strand_.context().get_executor()};
    }

    lua_State* L()
    {
        return L_;
    }

    // A thread that is useful to execute async handlers. This thread has
    // suspension disallowed eternally. It's not much different from a C
    // sighandler in the sense that not every function call is legal. C++ code
    // might use this thread to avoid lua_resume() and call lua_pcall()
    // directly.
    lua_State* async_event_thread()
    {
        current_fiber_ = async_event_thread_;
        return async_event_thread_;
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

    template<class HanaSet = std::decay_t<decltype(hana::make_set())>>
    void fiber_resume(lua_State* new_current_fiber, HanaSet&& options = {});

    void notify_errmem();
    void notify_exit_request();

    void notify_deadlock(std::string msg);
    void notify_cleanup_error(lua_State* coro);

    bool is_master() const noexcept
    {
        return this == appctx.master_vm.load().lock().get();
    }

    void async_event_thread(lua_State* new_async_event_thread)
    {
        assert(async_event_thread_ == nullptr);
        async_event_thread_ = new_async_event_thread;
    }

    lua_State* async_event_thread_
#ifndef NDEBUG
        = nullptr
#endif // NDEBUG
        ;

    // Use it to detect cycles when loading modules from external packages.
    std::set<std::string, TransparentStringComp> visited_external_packages;

    inbox_t inbox;

    boost::intrusive::list<
        pending_operation,
        boost::intrusive::constant_time_size<false>
    > pending_operations;

    app_context& appctx;

    // can be empty
    std::weak_ptr<asio::io_context> ioctxref;

private:
    void fiber_epilogue(int resume_result);

    strand_type strand_;
    bool valid_;
    bool lua_errmem;
    bool exit_request;
    bool suppress_tail_errors = false;
    lua_State* L_;
    lua_State* current_fiber_;
    std::vector<std::string> deadlock_errors;
    void* failed_cleanup_handler_coro = nullptr;
};

vm_context& get_vm_context(lua_State* L);

inline void setmetatable(lua_State* L, int index)
{
    int res = lua_setmetatable(L, index);
    assert(res); boost::ignore_unused(res);
}

void push(lua_State* L, const std::error_code& ec);

namespace detail {

template<class T>
void push(lua_State* L, std::string_view key, T v)
{
    static constexpr auto arg_pusher = hana::overload(
        [](lua_State* L, int v) { lua_pushinteger(L, v); },
        [](lua_State* L, lua_Integer v) { lua_pushinteger(L, v); },
        [](lua_State* L, lua_Number v) { lua_pushnumber(L, v); },
        [](lua_State* L, bool v) { lua_pushboolean(L, v ? 1 : 0); },
        [](lua_State* L, const char* v) {lua_pushstring(L, v); },
        [](lua_State* L, std::string_view v) {
            lua_pushlstring(L, v.data(), v.size());
        }
    );

    lua_pushlstring(L, key.data(), key.size());
    arg_pusher(L, v);
    lua_rawset(L, -3);
}

template<class T, class... Args>
void push(lua_State* L, std::string_view key, T v, Args&&... args)
{
    push(L, key, v);
    push(L, std::forward<Args>(args)...);
}

} // namespace detail

template<class... Args>
void push(lua_State* L, const std::error_code& ec, Args&&... args)
{
    static_assert(sizeof...(args) % 2 == 0);
    push(L, ec);
    detail::push(L, std::forward<Args>(args)...);
}

template<class... Args>
inline void push(lua_State* L, std::errc ec, Args&&... args)
{
    return push(L, make_error_code(ec), std::forward<Args>(args)...);
}

// gets value from top of the stack
std::variant<std::string_view, std::error_code> inspect_errobj(lua_State* L);

std::string errobj_to_string(std::variant<std::string_view, std::error_code> o);

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
    auto p = path.u8string();
    lua_pushlstring(L, reinterpret_cast<char*>(p.data()), p.size());
}

template<class F>
inline
auto push(lua_State* L, F&& pusher) -> decltype(std::forward<F>(pusher)(L))
{
    return std::forward<F>(pusher)(L);
}

inline std::string_view tostringview(lua_State* L, int index = -1)
{
    std::size_t len;
    const char* buf = lua_tolstring(L, index, &len);
    return std::string_view{buf, len};
}

inline void rawgetp(lua_State* L, int pseudoindex, const void* p)
{
    lua_pushlightuserdata(L, const_cast<void*>(p));
    lua_rawget(L, pseudoindex);
}

template<class T>
inline void finalize(lua_State* L, int index = 1)
{
    auto obj = static_cast<T*>(lua_touserdata(L, index));
    assert(obj);
    obj->~T();
}

template<class T>
inline int finalizer(lua_State* L)
{
    finalize<T>(L);
    return 0;
}

int throw_enosys(lua_State* L);

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
    only_main_fiber_may_import,
    bad_root_context,
    bad_index,
    bad_coroutine,
    suspension_already_allowed,
    interruption_already_allowed,
    forbid_suspend_block,
    interrupted,
    unmatched_scope_cleanup,
    channel_closed,
    no_senders,
    internal_module,
    raise_error,
    bad_rdf,
    bad_rdf_module,
    bad_rdf_error_category,
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

namespace detail {
bool unsafe_can_suspend(vm_context& vm_ctx, lua_State* L);
bool unsafe_can_suspend2(vm_context& vm_ctx, lua_State* L);
} // namespace detail

} // namespace emilua

template<>
struct std::is_error_code_enum<emilua::lua_errc>: std::true_type {};

template<>
struct std::is_error_code_enum<emilua::errc>: std::true_type {};

namespace emilua {

template<class HanaSet>
void vm_context::fiber_resume(lua_State* new_current_fiber, HanaSet&& options)
{
    static constexpr auto is_skip_clear_interrupter = hana::compose(
        hana::equal.to(hana::type_c<options::skip_clear_interrupter_t>),
        hana::typeid_);
    static constexpr auto is_auto_detect_interrupt = hana::compose(
        hana::equal.to(hana::type_c<options::auto_detect_interrupt_t>),
        hana::typeid_);
    static constexpr auto is_fast_auto_detect_interrupt = hana::compose(
        hana::equal.to(hana::type_c<options::fast_auto_detect_interrupt_t>),
        hana::typeid_);
    static constexpr auto is_arguments = [](auto&& x) {
        return hana::if_(
            hana::is_a<hana::pair_tag>(x),
            [](auto&& x) {
                return hana::typeid_(hana::first(x)) ==
                    hana::type_c<options::arguments_t>;
            },
            hana::always(hana::false_c)
        )(x);
    };

    static constexpr decltype(hana::any_of(options, is_auto_detect_interrupt))
        has_auto_detect_interrupt;
    static constexpr decltype(
        hana::any_of(options, is_fast_auto_detect_interrupt)
    ) has_fast_auto_detect_interrupt;

    assert(strand_.running_in_this_thread());
    if (!valid_)
        return;

    assert(lua_status(new_current_fiber) == 0 ||
           lua_status(new_current_fiber) == LUA_YIELD);
    current_fiber_ = new_current_fiber;

    int narg = 0;

    try {
        bool ok = true;
        hana::find_if(options, is_arguments) | [&](auto&& x) {
            auto push2 = hana::overload(
                [&](const boost::system::error_code& ec) {
                    std::error_code std_ec = ec;
                    if (has_auto_detect_interrupt) {
                        // `auto_detect_interrupt` means the user has set an
                        // interrupter that will cancel the underlying IO
                        // operation. Therefore the completion handler must have
                        // been called with `ec=asio::error::operation_aborted`
                        // (otherwise we want to defer interruption handling
                        // until the next interruption/suspendion point anyway).
                        if (ec == asio::error::operation_aborted) {
                            // A call to `fib:interrupt()` will set
                            // `FiberDataIndex::INTERRUPTED` for `fib`
                            // automatically.
                            rawgetp(new_current_fiber, LUA_REGISTRYINDEX,
                                    &fiber_list_key);
                            lua_pushthread(new_current_fiber);
                            lua_rawget(new_current_fiber, -2);
                            lua_rawgeti(new_current_fiber, -1,
                                        FiberDataIndex::INTERRUPTED);
                            bool interrupted =
                                lua_toboolean(new_current_fiber, -1);
                            lua_pop(new_current_fiber, 3);
                            if (interrupted)
                                std_ec = errc::interrupted;
                        }
                    } else if (has_fast_auto_detect_interrupt) {
                        // `fast_auto_detect_interrupt` means there is no other
                        // way but fiber interruption to have
                        // `ec=asio::error::operation_aborted`.
                        if (ec == asio::error::operation_aborted)
                            std_ec = errc::interrupted;
                    }
                    push(new_current_fiber, std_ec);
                },
                [&](std::nullopt_t) { lua_pushnil(new_current_fiber); },
                [&](bool b) { lua_pushboolean(new_current_fiber, b ? 1 : 0); },
                [&](auto n) -> std::enable_if_t<std::is_integral_v<decltype(n)>>
                { lua_pushinteger(new_current_fiber, n); },
                [&](lua_Number n) { lua_pushnumber(new_current_fiber, n); },
                [&](const auto& x) -> std::enable_if_t<
                    !std::is_integral_v<std::decay_t<decltype(x)>> &&
                    !std::is_floating_point_v<std::decay_t<decltype(x)>>
                > { push(new_current_fiber, x); }
            );

            narg += hana::size(hana::second(x));
            if (!lua_checkstack(new_current_fiber, narg + LUA_MINSTACK)) {
                ok = false;
                return hana::nothing;
            }
            hana::for_each(hana::second(x), push2);
            return hana::nothing;
        };
        if (!ok) {
            notify_errmem();
            close();
            return;
        }
    } catch (...) {
        // on Lua errors, current exception should be empty
        if (std::current_exception()) {
            lua_settop(new_current_fiber, 0);
            throw;
        }

        notify_errmem();
        close();
        return;
    }

    if (hana::none_of(options, is_skip_clear_interrupter)) {
        // There is no need for a try-catch block here. Only throwing function
        // in set_interrupter() is lua_rawseti(). lua_rawseti() shouldn't throw
        // on LUA_ERRMEM for `nil` assignment.
        lua_pushnil(new_current_fiber);
        set_interrupter(new_current_fiber, *this);
    }

    int res = lua_resume(new_current_fiber, narg);
    fiber_epilogue(res);
}

inline actor_address::actor_address(vm_context& vm_ctx)
    : dest{vm_ctx.weak_from_this()}
    , work_guard{vm_ctx.work_guard()}
{
    ++vm_ctx.inbox.nsenders;
}

inline actor_address::actor_address(const actor_address& o)
    : dest{o.dest}
    , work_guard{o.work_guard}
{
    auto vm_ctx = dest.lock();
    if (!vm_ctx)
        return;

    ++vm_ctx->inbox.nsenders;
}

inline actor_address& actor_address::operator=(actor_address&& o)
{
    dest = std::move(o.dest);
    work_guard.~executor_work_guard();
    new (&work_guard) asio::executor_work_guard<
        asio::io_context::executor_type>{std::move(o.work_guard)};
    return *this;
}

inline actor_address::~actor_address()
{
    auto vm_ctx = dest.lock();
    if (!vm_ctx)
        return;

    if (--vm_ctx->inbox.nsenders != 0)
        return;

    vm_ctx->strand().post([vm_ctx]() {
        if (vm_ctx->inbox.nsenders.load() != 0) {
            // another fiber from the actor already created a new sender
            return;
        }

        auto recv_fiber = vm_ctx->inbox.recv_fiber;
        if (recv_fiber == nullptr)
            return;

        vm_ctx->inbox.recv_fiber = nullptr;
        vm_ctx->inbox.work_guard.reset();

        auto opt_args = vm_context::options::arguments;
        vm_ctx->fiber_resume(
            recv_fiber,
            hana::make_set(
                hana::make_pair(opt_args, hana::make_tuple(errc::no_senders))));
    }, std::allocator<void>{});
}

inline inbox_t::sender_state::sender_state(vm_context& vm_ctx)
    : vm_ctx(vm_ctx.shared_from_this())
    , work_guard(vm_ctx.work_guard())
    , fiber(vm_ctx.current_fiber())
    , msg{std::in_place_type<bool>, false}
{}

inline inbox_t::sender_state::sender_state(vm_context& vm_ctx, lua_State* fiber)
    : vm_ctx(vm_ctx.shared_from_this())
    , work_guard(vm_ctx.work_guard())
    , fiber(fiber)
    , msg{std::in_place_type<bool>, false}
{}

inline inbox_t::sender_state::sender_state(std::nullopt_t)
    : work_guard{[]() {
        asio::io_context ioctx;
        asio::executor_work_guard<asio::io_context::executor_type> work_guard{
            ioctx.get_executor()};
        work_guard.reset();
        return work_guard;
    }()}
    , fiber{nullptr}
    , msg{std::in_place_type<bool>, false}
    , wake_on_destruct{false}
{}

inline inbox_t::sender_state::sender_state(sender_state&& o)
    : vm_ctx(std::move(o.vm_ctx))
    , work_guard(std::move(o.work_guard))
    , fiber(o.fiber)
    , msg(std::move(o.msg))
    , wake_on_destruct(o.wake_on_destruct)
{
    o.wake_on_destruct = false;
}

inline inbox_t::sender_state::~sender_state()
{
    if (!wake_on_destruct)
        return;

    vm_ctx->strand().post([vm_ctx=vm_ctx, fiber=fiber]() {
        auto opt_args = vm_context::options::arguments;
        vm_ctx->fiber_resume(
            fiber,
            hana::make_set(
                hana::make_pair(
                    opt_args, hana::make_tuple(errc::channel_closed))));
    }, std::allocator<void>{});
}

inline
inbox_t::sender_state&
inbox_t::sender_state::operator=(inbox_t::sender_state&& o)
{
    if (wake_on_destruct) {
        vm_ctx->strand().post([vm_ctx=vm_ctx, fiber=fiber]() {
            auto opt_args = vm_context::options::arguments;
            vm_ctx->fiber_resume(
                fiber,
                hana::make_set(
                    hana::make_pair(
                        opt_args, hana::make_tuple(errc::channel_closed))));
        }, std::allocator<void>{});
    }

    vm_ctx = std::move(o.vm_ctx);
    work_guard.~executor_work_guard();
    new (&work_guard) asio::executor_work_guard<
        asio::io_context::executor_type>{std::move(o.work_guard)};
    fiber = o.fiber;
    msg = std::move(o.msg);
    wake_on_destruct = o.wake_on_destruct;

    o.wake_on_destruct = false;
    return *this;
}

} // namespace emilua
