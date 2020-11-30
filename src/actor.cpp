#include <emilua/actor.hpp>
#include <emilua/state.hpp>
#include <emilua/fiber.hpp>
#include <emilua/json.hpp>

#include <boost/hana/functional/overload.hpp>

namespace emilua {

extern unsigned char chan_op_bytecode[];
extern std::size_t chan_op_bytecode_size;

char inbox_key;
static char inbox_mt_key;
static char tx_chan_mt_key;
static char closed_tx_chan_mt_key;
static char chan_recv_key;
static char chan_send_key;

// TODO: fix it
static int dummy;

int deserializer_closure(lua_State* L)
{
    using array_key_type = int;

    struct object_range
    {
        using iterator =
            typename std::map<std::string, inbox_t::value_type>::iterator;

        object_range(std::map<std::string, inbox_t::value_type>& m)
            : it{m.begin()}
            , end{m.end()}
        {}

        bool empty() const
        {
            return it == end;
        }

        iterator it;
        iterator end;
    };

    struct array_range
    {
        array_range(std::vector<inbox_t::value_type>& array)
            : array{array}
        {
            assert(array.size() <= std::numeric_limits<array_key_type>::max());
        }

        bool empty() const
        {
            return idx == array.size();
        }

        std::size_t idx = 0;
        std::vector<inbox_t::value_type>& array;
    };

    auto& value = *reinterpret_cast<inbox_t::value_type*>(
        lua_touserdata(L, lua_upvalueindex(1)));

    std::vector<std::variant<object_range, array_range>> path;

    static constexpr auto push_address = [](lua_State* L, actor_address& a) {
        auto buf = reinterpret_cast<actor_address*>(
            lua_newuserdata(L, sizeof(actor_address))
        );
        rawgetp(L, LUA_REGISTRYINDEX, &tx_chan_mt_key);
        int res = lua_setmetatable(L, -2);
        assert(res); boost::ignore_unused(res);
        new (buf) actor_address{std::move(a)};
    };

    auto push_leaf_or_append_path_and_return_true_on_leaf = [&](
        inbox_t::value_type::variant_type& value
    ) {
        return std::visit(hana::overload(
            [L](bool b) { lua_pushboolean(L, b ? 1 : 0); return true; },
            [L](lua_Number n) { lua_pushnumber(L, n); return true;},
            [L](std::string_view v) { push(L, v); return true; },
            [L](actor_address& a) { push_address(L, a); return true; },
            [&](std::map<std::string, inbox_t::value_type>& m) {
                path.emplace_back(std::in_place_type<object_range>, m);
                return false;
            },
            [&](std::vector<inbox_t::value_type>& v) {
                path.emplace_back(std::in_place_type<array_range>, v);
                return false;
            }
        ), value);
    };
    if (/*is_leaf=*/push_leaf_or_append_path_and_return_true_on_leaf(value))
        return 1;

    // During `value` traversal, we always keep two values on top of the lua
    // stack (from top to bottom):
    //
    // * -1: Current work item.
    // * -2: The items tree stack.
    //
    // See `json::decode()` implementation for details. It's kinda the same
    // layout idea. There's an explanation comment block there already. Do
    // notice there are major differences between JSON and `value` traversal
    // (e.g. key-value pair available in one-shot vs pieces, untrusted vs
    // pre-sanitized data, cheapness of look-ahead, requirement to mark arrays
    // with special metatable, etc) so the traversal algorithm accordingly
    // deviates a lot (e.g. current work item is never nil, ...).
    lua_newtable(L);
    lua_newtable(L);
    lua_pushvalue(L, -1);
    lua_rawseti(L, -3, 1);

    for (;;) {
        assert(lua_type(L, -1) == LUA_TTABLE);
        inbox_t::value_type::variant_type *value = nullptr;
        std::visit(hana::overload(
            [&](object_range& o) {
                if (o.empty())
                    return;

                push(L, o.it->first);
                value = &o.it->second;
                ++o.it;
            },
            [&](array_range& a) {
                if (a.empty())
                    return;

                lua_pushinteger(L, a.idx + 1);
                value = &a.array[a.idx];
                ++a.idx;
            }
        ), path.back());
        if (!value) { // close event
            path.pop_back();
            if (path.size() == 0)
                break;

            lua_pop(L, 1);
            lua_pushnil(L);
            lua_rawseti(L, -2, static_cast<array_key_type>(path.size() + 1));
            lua_rawgeti(L, -1, static_cast<array_key_type>(path.size()));
            continue;
        }
        if (push_leaf_or_append_path_and_return_true_on_leaf(*value)) {
            lua_rawset(L, -3);
            continue;
        }
        lua_newtable(L);
        lua_insert(L, -2);
        lua_pushvalue(L, -2);
        lua_rawset(L, -4);
        lua_remove(L, -2);
        lua_pushvalue(L, -1);
        lua_rawseti(L, -3, static_cast<array_key_type>(path.size()));
    }

    assert(lua_objlen(L, -2) == 1);
    return 1;
}

static int chan_send(lua_State* L)
{
    if (lua_gettop(L) < 2) {
        push(L, std::errc::invalid_argument);
        return lua_error(L);
    }

    auto& vm_ctx = get_vm_context(L);
    auto handle = reinterpret_cast<actor_address*>(lua_touserdata(L, 1));
    if (!handle || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument);
        lua_pushliteral(L, "arg");
        lua_pushinteger(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &tx_chan_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument);
        lua_pushliteral(L, "arg");
        lua_pushinteger(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }

    EMILUA_CHECK_SUSPEND_ALLOWED(vm_ctx, L);

    auto dest_vm_ctx = handle->dest.lock();
    if (!dest_vm_ctx) {
        push(L, errc::channel_closed);
        return lua_error(L);
    }

    using array_key_type = int;
    constexpr auto array_key_max = std::numeric_limits<array_key_type>::max();

    enum {
        NODE_IDX = 1,
        ITER_IDX,
    };

    static constexpr std::uintptr_t is_object_bitmask = 0x1;
    static_assert(sizeof(std::uintptr_t) == sizeof(void*));
    // number of bits we can hijack from pointer for our own purposes {{{
    static_assert(alignof(inbox_t::value_object_type) > 1);
    static_assert(alignof(inbox_t::value_array_type) > 1);
    // }}}

    struct dom_reference
    {
        dom_reference(inbox_t::value_object_type& o)
            : ptr{reinterpret_cast<std::uintptr_t>(&o) | is_object_bitmask}
        {}

        dom_reference(inbox_t::value_array_type& a)
            : ptr{reinterpret_cast<std::uintptr_t>(&a)}
        {}

        inbox_t::value_object_type& as_object()
        {
            return *reinterpret_cast<inbox_t::value_object_type*>(
                ptr & ~is_object_bitmask);
        }

        inbox_t::value_array_type& as_array()
        {
            return *reinterpret_cast<inbox_t::value_array_type*>(ptr);
        }

        bool is_object() const
        {
            return ptr & is_object_bitmask;
        }

        bool is_array() const
        {
            return !is_object();
        }

        std::uintptr_t ptr;
    };

    inbox_t::sender_state sender{vm_ctx};

    switch (lua_type(L, 2)) {
    case LUA_TNIL:
    case LUA_TFUNCTION:
    case LUA_TTHREAD:
    case LUA_TLIGHTUSERDATA:
        push(L, std::errc::invalid_argument);
        return lua_error(L);
    case LUA_TNUMBER:
        sender.msg.emplace<lua_Number>(lua_tonumber(L, 2));
        break;
    case LUA_TBOOLEAN:
        sender.msg.emplace<bool>(lua_toboolean(L, 2));
        break;
    case LUA_TSTRING: {
        std::size_t size;
        const char* data = lua_tolstring(L, 2, &size);
        sender.msg.emplace<std::string>(data, size);
        break;
    }
    case LUA_TTABLE: {
        if (lua_getmetatable(L, 2)) {
            push(L, std::errc::invalid_argument);
            return lua_error(L);
        }

        std::vector<dom_reference> dom_stack;
        array_key_type current_array_idx;

        // During iteration, we keep 3 values as the Lua stack base (from bottom
        // to top):
        //
        // * A visited table to detect reference cycles.
        // * The iterators stack (our iteration tree). Each element is a Lua
        //   table (a work item).
        // * The current Lua table being iterated.
        //
        // If current work item is an object, we keep 1 extra value on the Lua
        // stack top:
        //
        // * Current key.
        lua_newtable(L);
        lua_pushvalue(L, 2);
        lua_pushboolean(L, 1);
        lua_rawset(L, -3);

        lua_newtable(L);
        lua_pushvalue(L, 2);

        {
            lua_createtable(L, /*narr=*/2, /*nrec=*/0);
            lua_pushvalue(L, -2);
            lua_rawseti(L, -2, NODE_IDX);
        }
        lua_rawseti(L, -3, 1);

        if (lua_objlen(L, -1) > 0) {
            dom_stack.emplace_back(
                sender.msg.emplace<inbox_t::value_array_type>());
            current_array_idx = 0;
        } else {
            dom_stack.emplace_back(
                sender.msg.emplace<inbox_t::value_object_type>());
            lua_pushnil(L);
        }

        while (dom_stack.size() > 0) {
            auto cur_node = dom_stack.back();
            inbox_t::value_type *cur_value = nullptr;
            inbox_t::value_object_type::iterator obj_it;
            if (cur_node.is_array()) {
                if (current_array_idx == array_key_max) {
                    push(L, json_errc::array_too_long);
                    return lua_error(L);
                }

                lua_rawgeti(L, -1, ++current_array_idx);
                switch (lua_type(L, -1)) {
                case LUA_TNIL:
                    lua_pop(L, 1);
                    break;
                default:
                    cur_value = &cur_node.as_array().emplace_back(
                        std::in_place_type<bool>, false);
                }
            } else {
                if (lua_next(L, -2) != 0) {
                    if (lua_type(L, -2) != LUA_TSTRING) {
                        lua_pop(L, 1);
                        continue;
                    }
                    obj_it = cur_node.as_object().emplace(
                        tostringview(L, -2), false).first;
                    cur_value = &obj_it->second;
                }
            }

            auto update_lua_ctx_on_level_popped = [&]() {
                // remove from visited
                lua_pushnil(L);
                lua_rawset(L, -4);

                // update iterators stack {{{
                lua_pushnil(L);
                lua_rawseti(L, -2,
                            static_cast<array_key_type>(dom_stack.size() + 1));
                // }}}

                // update cur table + iterator
                lua_rawgeti(L, -1,
                            static_cast<array_key_type>(dom_stack.size()));
                lua_rawgeti(L, -1, NODE_IDX);
                lua_rawgeti(L, -2, ITER_IDX);
                lua_remove(L, -3);
                if (dom_stack.back().is_array()) {
                    current_array_idx = lua_tointeger(L, -1);
                    lua_pop(L, 1);
                }
            };

            // event: close current node
            if (!cur_value) {
                dom_stack.pop_back();
                if (dom_stack.size() == 0)
                    break;

                update_lua_ctx_on_level_popped();
                continue;
            }

            auto ignore_cur_item = [&]() {
                lua_pop(L, 1);
                if (cur_node.is_array()) {
                    cur_node.as_array().pop_back();

                    dom_stack.pop_back();
                    if (dom_stack.size() == 0)
                        return;

                    update_lua_ctx_on_level_popped();
                } else {
                    cur_node.as_object().erase(obj_it);
                }
            };

            switch (lua_type(L, -1)) {
            case LUA_TNIL:
                assert(false);
            case LUA_TUSERDATA:
                if (lua_getmetatable(L, -1)) {
                    rawgetp(L, LUA_REGISTRYINDEX, &tx_chan_mt_key);
                    if (lua_rawequal(L, -1, -2)) {
                        const auto& msg = *reinterpret_cast<actor_address*>(
                            lua_touserdata(L, -3));
                        cur_value->emplace<actor_address>(msg);
                        lua_pop(L, 3);
                        break;
                    }
                    rawgetp(L, LUA_REGISTRYINDEX, &inbox_mt_key);
                    if (lua_rawequal(L, -1, -3)) {
                        cur_value->emplace<actor_address>(vm_ctx);
                        lua_pop(L, 4);
                        break;
                    }
                    // TODO: check whether has metamethod to transfer between
                    // states
                    lua_pop(L, 3);
                }
            case LUA_TFUNCTION:
            case LUA_TTHREAD:
            case LUA_TLIGHTUSERDATA:
                ignore_cur_item();
                break;
            case LUA_TNUMBER:
                cur_value->emplace<lua_Number>(lua_tonumber(L, -1));
                lua_pop(L, 1);
                break;
            case LUA_TBOOLEAN:
                cur_value->emplace<bool>(lua_toboolean(L, -1));
                lua_pop(L, 1);
                break;
            case LUA_TSTRING:
                cur_value->emplace<std::string>(tostringview(L, -1));
                lua_pop(L, 1);
                break;
            case LUA_TTABLE: {
                if (lua_getmetatable(L, -1)) {
                    lua_pop(L, 1);
                    ignore_cur_item();
                    break;
                }

                {
                    int visited_idx = cur_node.is_array() ? -4 : -5;
                    lua_pushvalue(L, -1);
                    lua_rawget(L, visited_idx - 1);
                    if (lua_type(L, -1) == LUA_TBOOLEAN) {
                        assert(lua_toboolean(L, -1) == 1);
                        push(L, json_errc::cycle_exists);
                        return lua_error(L);
                    }
                    assert(lua_type(L, -1) == LUA_TNIL);
                    lua_pop(L, 1);

                    lua_pushvalue(L, -1);
                    lua_pushboolean(L, 1);
                    lua_rawset(L, visited_idx - 2);
                }

                if (dom_stack.size() == array_key_max) {
                    push(L, json_errc::too_many_levels);
                    return lua_error(L);
                }

                // save current iterator
                {
                    int iterators_stack_idx = cur_node.is_array() ? -3 : -4;
                    lua_rawgeti(L, iterators_stack_idx,
                                static_cast<array_key_type>(dom_stack.size()));
                    if (cur_node.is_array())
                        lua_pushinteger(L, current_array_idx);
                    else
                        lua_pushvalue(L, -3);
                    lua_rawseti(L, -2, ITER_IDX);
                    lua_pop(L, 1);
                }

                // remove iter/key from Lua stack (only there on objects)
                if (cur_node.is_object())
                    lua_remove(L, -2);

                // remove from the Lua stack, the node that we were previously
                // iterating over
                lua_remove(L, -2);

                {
                    lua_createtable(L, /*narr=*/2, /*nrec=*/0);
                    lua_pushvalue(L, -2);
                    lua_rawseti(L, -2, NODE_IDX);
                }
                lua_rawseti(L, -3,
                            static_cast<array_key_type>(dom_stack.size() + 1));

                if (lua_objlen(L, -1) > 0) {
                    dom_stack.emplace_back(
                        cur_value->emplace<inbox_t::value_array_type>());
                    current_array_idx = 0;
                } else {
                    dom_stack.emplace_back(
                        cur_value->emplace<inbox_t::value_object_type>());
                    lua_pushnil(L);
                }
            }
            }
        }
        lua_pop(L, 3);
        break;
    }
    case LUA_TUSERDATA:
        if (!lua_getmetatable(L, 2)) {
            push(L, std::errc::invalid_argument);
            return lua_error(L);
        }
        if (lua_rawequal(L, -1, -2)) {
            const auto& msg = *reinterpret_cast<const actor_address*>(
                lua_touserdata(L, 2));
            sender.msg.emplace<actor_address>(msg);
            break;
        }
        rawgetp(L, LUA_REGISTRYINDEX, &inbox_mt_key);
        if (lua_rawequal(L, -1, -2)) {
            sender.msg.emplace<actor_address>(vm_ctx);
            break;
        }
        // TODO: check whether has metamethod to transfer between states
        push(L, std::errc::not_supported);
        return lua_error(L);
    }

    lua_pushvalue(L, 1);
    lua_pushlightuserdata(L, vm_ctx.current_fiber());
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto& vm_ctx = get_vm_context(L);
            auto handle = reinterpret_cast<const actor_address*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            auto current_fiber = reinterpret_cast<lua_State*>(
                lua_touserdata(L, lua_upvalueindex(2)));

            auto dest_vm_ctx = handle->dest.lock();
            if (!dest_vm_ctx)
                return 0;

            inbox_t::sender_state sender{vm_ctx, current_fiber};
            dest_vm_ctx->strand().post(
                [vm_ctx=dest_vm_ctx, sender=std::move(sender)]() {
                    // We rely on FIFO order here. If it were allowed for the
                    // interrupter to arrive before the `sender` delivery, this
                    // algorithm would fail by assuming the task already
                    // finished and there is nothing to interrupt.
                    auto it = std::find(
                        vm_ctx->inbox.incoming.begin(),
                        vm_ctx->inbox.incoming.end(),
                        sender);
                    if (it == vm_ctx->inbox.incoming.end())
                        return;
                    vm_ctx->inbox.incoming.erase(it);

                    sender.vm_ctx->strand().post(
                        [vm_ctx=sender.vm_ctx, fiber=sender.fiber]() {
                            vm_ctx->fiber_prologue(
                                fiber,
                                [&]() { push(fiber, errc::interrupted); });
                            int res = lua_resume(fiber, 1);
                            vm_ctx->fiber_epilogue(res);
                        },
                        std::allocator<void>{}
                    );
                },
                std::allocator<void>{}
            );
            return 0;
        },
        2
    );
    set_interrupter(L);

    sender.wake_on_destruct = true;
    dest_vm_ctx->strand().post(
        [vm_ctx=dest_vm_ctx, sender=std::move(sender)]() mutable {
            auto recv_fiber = vm_ctx->inbox.recv_fiber;
            if (!vm_ctx->inbox.open)
                return;

            if (!recv_fiber) {
                vm_ctx->inbox.incoming.emplace_back(std::move(sender));
                vm_ctx->inbox.incoming.back().wake_on_destruct = false;
                return;
            }

            vm_ctx->inbox.recv_fiber = nullptr;
            vm_ctx->inbox.work_guard.reset();

            vm_ctx->fiber_prologue(
                recv_fiber,
                [&]() {
                    lua_pushnil(recv_fiber);
                    lua_pushlightuserdata(recv_fiber, &sender.msg);
                    lua_pushcclosure(recv_fiber, deserializer_closure, 1);
                });
            int res = lua_resume(recv_fiber, 2);
            vm_ctx->fiber_epilogue(res);

            sender.wake_on_destruct = false;
            sender.vm_ctx->strand().post(
                [vm_ctx=sender.vm_ctx, fiber=sender.fiber]() {
                    vm_ctx->fiber_prologue(fiber);
                    lua_pushnil(fiber);
                    int res = lua_resume(fiber, 1);
                    vm_ctx->fiber_epilogue(res);
                },
                std::allocator<void>{}
            );
        },
        std::allocator<void>{}
    );

    return lua_yield(L, 0);
}

static int tx_chan_close(lua_State* L)
{
    auto handle = reinterpret_cast<actor_address*>(lua_touserdata(L, 1));
    if (!handle || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument);
        lua_pushliteral(L, "arg");
        lua_pushinteger(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &tx_chan_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument);
        lua_pushliteral(L, "arg");
        lua_pushinteger(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }

    rawgetp(L, LUA_REGISTRYINDEX, &closed_tx_chan_mt_key);
    int res = lua_setmetatable(L, 1);
    assert(res); boost::ignore_unused(res);
    handle->~actor_address();
    return 0;
}

static int chan_recv(lua_State* L)
{
    auto& vm_ctx = get_vm_context(L);
    if (!lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument);
        lua_pushliteral(L, "arg");
        lua_pushinteger(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &inbox_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument);
        lua_pushliteral(L, "arg");
        lua_pushinteger(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }

    EMILUA_CHECK_SUSPEND_ALLOWED(vm_ctx, L);

    if (!vm_ctx.inbox.open) {
        push(L, errc::channel_closed);
        return lua_error(L);
    }

    if (vm_ctx.inbox.recv_fiber != nullptr) {
        push(L, std::errc::device_or_resource_busy);
        return lua_error(L);
    }

    if (vm_ctx.inbox.incoming.size() != 0) {
        lua_pushnil(L);

        auto sender = std::move(vm_ctx.inbox.incoming.front());
        vm_ctx.inbox.incoming.pop_front();

        sender.vm_ctx->strand().post(
            [vm_ctx=sender.vm_ctx, fiber=sender.fiber]() {
                vm_ctx->fiber_prologue(fiber);
                int res = lua_resume(fiber, 0);
                vm_ctx->fiber_epilogue(res);
            },
            std::allocator<void>{}
        );

        lua_pushlightuserdata(L, &sender.msg);
        lua_pushcclosure(L, deserializer_closure, 1);
        lua_call(L, 0, 1);

        return 2;
    }

    // runtime errors are checked after logical errors
    if (vm_ctx.inbox.nsenders.load() == 0) {
        push(L, errc::no_senders);
        return lua_error(L);
    }

    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto& vm_ctx = get_vm_context(L);
            auto recv_fiber = vm_ctx.inbox.recv_fiber;

            vm_ctx.inbox.recv_fiber = nullptr;
            vm_ctx.inbox.work_guard.reset();

            vm_ctx.strand().post(
                [vm_ctx=vm_ctx.shared_from_this(), recv_fiber]() {
                    vm_ctx->fiber_prologue(
                        recv_fiber,
                        [&]() { push(recv_fiber, errc::interrupted); });
                    int res = lua_resume(recv_fiber, 1);
                    vm_ctx->fiber_epilogue(res);
                },
                std::allocator<void>{}
            );

            return 0;
        },
        0
    );
    set_interrupter(L);

    vm_ctx.inbox.recv_fiber = vm_ctx.current_fiber();
    vm_ctx.inbox.work_guard = vm_ctx.shared_from_this();
    return lua_yield(L, 0);
}

static int inbox_close(lua_State* L)
{
    auto& vm_ctx = get_vm_context(L);
    if (!lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument);
        lua_pushliteral(L, "arg");
        lua_pushinteger(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &inbox_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument);
        lua_pushliteral(L, "arg");
        lua_pushinteger(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }

    if (!vm_ctx.inbox.open)
        return 0;

    if (vm_ctx.inbox.recv_fiber != nullptr) {
        auto recv_fiber = vm_ctx.inbox.recv_fiber;

        vm_ctx.inbox.recv_fiber = nullptr;
        vm_ctx.inbox.work_guard.reset();

        vm_ctx.strand().post([vm_ctx=vm_ctx.shared_from_this(), recv_fiber]() {
            vm_ctx->fiber_prologue(
                recv_fiber,
                [&]() { push(recv_fiber, errc::channel_closed); });
            int res = lua_resume(recv_fiber, 1);
            vm_ctx->fiber_epilogue(res);
        }, std::allocator<void>{});
    }

    vm_ctx.inbox.open = false;
    for (auto& m: vm_ctx.inbox.incoming) {
        m.wake_on_destruct = true;
    }
    vm_ctx.inbox.incoming.clear();
    return 0;
}

static int inbox_gc(lua_State* L)
{
    auto& vm_ctx = get_vm_context(L);
    if (!vm_ctx.inbox.open)
        return 0;

    // This is only needed if the VM crashes (i.e. nobody woke `recv_fiber` up),
    // but even so there are other layers that will execute this same
    // snippet. Anyway, defensive programming here. This extra safety belt
    // doesn't hurt. {{{
    vm_ctx.inbox.recv_fiber = nullptr;
    vm_ctx.inbox.work_guard.reset();
    // }}}

    vm_ctx.inbox.open = false;
    for (auto& m: vm_ctx.inbox.incoming) {
        m.wake_on_destruct = true;
    }
    vm_ctx.inbox.incoming.clear();
    return 0;
}

static int spawn_vm(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TSTRING);
    auto& vm_ctx = get_vm_context(L);
    auto module = tostringview(L, 1);
    if (module == ".") {
        rawgetp(L, LUA_REGISTRYINDEX, &fiber_list_key);
        lua_pushthread(vm_ctx.current_fiber());
        lua_xmove(vm_ctx.current_fiber(), L, 1);
        lua_rawget(L, -2);
        lua_rawgeti(L, -1, FiberDataIndex::SOURCE_PATH);
        module = tostringview(L, -1);
    }

    try {
        auto new_vm_ctx = emilua::make_vm(vm_ctx.strand().context(), dummy,
                                          module, emilua::ContextType::worker);

        auto buf = reinterpret_cast<actor_address*>(
            lua_newuserdata(L, sizeof(actor_address))
        );
        rawgetp(L, LUA_REGISTRYINDEX, &tx_chan_mt_key);
        int res = lua_setmetatable(L, -2);
        assert(res); boost::ignore_unused(res);
        new (buf) actor_address{*new_vm_ctx};

        new_vm_ctx->strand().post([new_vm_ctx]() {
            new_vm_ctx->fiber_resume_trivial(new_vm_ctx->L());
        }, std::allocator<void>{});

        return 1;
    } catch (const std::bad_alloc&) {
        push(L, std::errc::not_enough_memory);
        return lua_error(L);
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }
}

static int tx_chan_mt_eq(lua_State* L)
{
    auto is_same = [](const std::weak_ptr<vm_context>& a,
                      const std::weak_ptr<vm_context>& b) {
        return !a.owner_before(b) && !b.owner_before(a);
    };

    const auto& a = *reinterpret_cast<actor_address*>(lua_touserdata(L, 1));
    const auto& b = *reinterpret_cast<actor_address*>(lua_touserdata(L, 2));
    lua_pushboolean(L, is_same(a.dest, b.dest) ? 1 : 0);
    return 1;
}

static int tx_chan_mt_index(lua_State* L)
{
    auto key = tostringview(L, 2);
    if (key == "send") {
        rawgetp(L, LUA_REGISTRYINDEX, &chan_send_key);
        return 1;
    } else if (key == "close") {
        lua_pushcfunction(L, tx_chan_close);
        return 1;
    } else {
        push(L, errc::bad_index);
        lua_pushliteral(L, "index");
        lua_pushvalue(L, 2);
        lua_rawset(L, -3);
        return lua_error(L);
    }
}

static int closed_tx_chan_mt_index(lua_State* L)
{
    auto key = tostringview(L, 2);
    if (key == "send") {
        lua_pushcfunction(
            L,
            [](lua_State* L) -> int {
                push(L, errc::channel_closed);
                return lua_error(L);
            }
        );
        return 1;
    } else if (key == "close") {
        lua_pushcfunction(L, [](lua_State*) -> int { return 0; });
        return 1;
    } else {
        push(L, errc::bad_index);
        lua_pushliteral(L, "index");
        lua_pushvalue(L, 2);
        lua_rawset(L, -3);
        return lua_error(L);
    }
}

static int inbox_mt_index(lua_State* L)
{
    auto key = tostringview(L, 2);
    if (key == "recv") {
        rawgetp(L, LUA_REGISTRYINDEX, &chan_recv_key);
        return 1;
    } else if (key == "close") {
        lua_pushcfunction(L, inbox_close);
        return 1;
    } else {
        push(L, errc::bad_index);
        lua_pushliteral(L, "index");
        lua_pushvalue(L, 2);
        lua_rawset(L, -3);
        return lua_error(L);
    }
}

void init_actor_module(lua_State* L)
{
    lua_pushliteral(L, "spawn_vm");
    lua_pushcfunction(L, spawn_vm);
    lua_rawset(L, LUA_GLOBALSINDEX);

    lua_pushlightuserdata(L, &tx_chan_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/5);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, finalizer<actor_address>);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "tx-channel");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__newindex");
        lua_pushcfunction(
            L,
            [](lua_State* L) -> int {
                push(L, std::errc::operation_not_permitted);
                return lua_error(L);
            });
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, tx_chan_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__eq");
        lua_pushcfunction(L, tx_chan_mt_eq);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &closed_tx_chan_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/4);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "tx-channel");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__newindex");
        lua_pushcfunction(
            L,
            [](lua_State* L) -> int {
                push(L, std::errc::operation_not_permitted);
                return lua_error(L);
            });
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, closed_tx_chan_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__eq");
        lua_pushcfunction(
            L,
            [](lua_State* L) -> int {
                push(L, std::errc::not_supported);
                return lua_error(L);
            });
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &inbox_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/4);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, inbox_gc);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "rx-channel");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__newindex");
        lua_pushcfunction(
            L,
            [](lua_State* L) -> int {
                push(L, std::errc::operation_not_permitted);
                return lua_error(L);
            });
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, inbox_mt_index);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    {
        lua_pushlightuserdata(L, &chan_send_key);
        int res = luaL_loadbuffer(
            L, reinterpret_cast<char*>(chan_op_bytecode), chan_op_bytecode_size,
            nullptr);
        assert(res == 0); boost::ignore_unused(res);
        lua_pushcfunction(L, lua_error);
        lua_pushcfunction(L, chan_send);
        lua_pushliteral(L, "type");
        lua_rawget(L, LUA_GLOBALSINDEX);
        lua_call(L, 3, 1);
        lua_rawset(L, LUA_REGISTRYINDEX);
    }
    {
        lua_pushlightuserdata(L, &chan_recv_key);
        int res = luaL_loadbuffer(
            L, reinterpret_cast<char*>(chan_op_bytecode), chan_op_bytecode_size,
            nullptr);
        assert(res == 0); boost::ignore_unused(res);
        lua_pushcfunction(L, lua_error);
        lua_pushcfunction(L, chan_recv);
        lua_pushliteral(L, "type");
        lua_rawget(L, LUA_GLOBALSINDEX);
        lua_call(L, 3, 1);
        lua_rawset(L, LUA_REGISTRYINDEX);
    }

    {
        lua_pushlightuserdata(L, &inbox_key);
        lua_newuserdata(L, sizeof(char));
        rawgetp(L, LUA_REGISTRYINDEX, &inbox_mt_key);
        int res = lua_setmetatable(L, -2);
        assert(res); boost::ignore_unused(res);
        lua_rawset(L, LUA_REGISTRYINDEX);
    }
}

} // namespace emilua
