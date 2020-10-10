#include <emilua/actor.hpp>
#include <emilua/state.hpp>
#include <emilua/fiber.hpp>

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
    auto& value = *reinterpret_cast<inbox_t::value_type*>(
        lua_touserdata(L, lua_upvalueindex(1)));

    return std::visit(hana::overload(
        [L](bool b) {
            lua_pushboolean(L, b ? 1 : 0);
            return 1;
        },
        [L](double d) {
            lua_pushnumber(L, d);
            return 1;
        },
        [L](const std::string& s) {
            lua_pushlstring(L, s.data(), s.size());
            return 1;
        },
        [L](std::map<std::string, inbox_t::value_type>& m) {
            // TODO
            push(L, std::errc::not_supported);
            return lua_error(L);
        },
        [L](std::vector<inbox_t::value_type>& v) {
            // TODO
            push(L, std::errc::not_supported);
            return lua_error(L);
        },
        [L](actor_address& a) {
            auto buf = reinterpret_cast<actor_address*>(
                lua_newuserdata(L, sizeof(actor_address))
            );
            rawgetp(L, LUA_REGISTRYINDEX, &tx_chan_mt_key);
            int res = lua_setmetatable(L, -2);
            assert(res); boost::ignore_unused(res);
            new (buf) actor_address{std::move(a)};
            return 1;
        }
    ), static_cast<inbox_t::value_type::variant_type&>(value));
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

    inbox_t::sender_state sender{vm_ctx};

    switch (lua_type(L, 2)) {
    case LUA_TNIL:
    case LUA_TFUNCTION:
    case LUA_TTHREAD:
    case LUA_TLIGHTUSERDATA:
        push(L, std::errc::invalid_argument);
        return lua_error(L);
    case LUA_TNUMBER:
        sender.msg.emplace<double>(lua_tonumber(L, 2));
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
    case LUA_TTABLE:
        // TODO: serialize
        push(L, std::errc::not_supported);
        return lua_error(L);
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
                            vm_ctx->fiber_prologue(fiber);
                            push(fiber, errc::interrupted);
                            vm_ctx->reclaim_reserved_zone();
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

            vm_ctx->fiber_prologue(recv_fiber);
            lua_pushnil(recv_fiber);
            lua_pushlightuserdata(recv_fiber, &sender.msg);
            lua_pushcclosure(recv_fiber, deserializer_closure, 1);
            vm_ctx->reclaim_reserved_zone();
            int res = lua_resume(recv_fiber, 2);
            vm_ctx->fiber_epilogue(res);

            sender.wake_on_destruct = false;
            sender.vm_ctx->strand().post(
                [vm_ctx=sender.vm_ctx, fiber=sender.fiber]() {
                    vm_ctx->fiber_prologue(fiber);
                    lua_pushnil(fiber);
                    vm_ctx->reclaim_reserved_zone();
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
                vm_ctx->reclaim_reserved_zone();
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
                    vm_ctx->fiber_prologue(recv_fiber);
                    push(recv_fiber, errc::interrupted);
                    vm_ctx->reclaim_reserved_zone();
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
            vm_ctx->fiber_prologue(recv_fiber);
            push(recv_fiber, errc::channel_closed);
            vm_ctx->reclaim_reserved_zone();
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
