#include <new>

#include <emilua/detail/core.hpp>

namespace asio = boost::asio;

namespace emilua {

namespace detail {
char fiber_list_key;
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
    if (valid_)
        lua_close(L_);

    valid_ = false;
    L_ = nullptr;
}

void vm_context::fiber_prologue(lua_State* new_current_fiber)
{
    assert(valid_);
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
        // TODO:
        // join algorithm
        rawgetp(current_fiber_, LUA_REGISTRYINDEX, &detail::fiber_list_key);

        lua_pushthread(current_fiber_);
        lua_rawget(current_fiber_, -2);
        lua_rawgeti(current_fiber_, -1, detail::FiberDataIndex::CONTEXT);
        if (lua_type(current_fiber_, -1) == LUA_TNIL) {
            lua_pop(current_fiber_, 2);
            lua_pushthread(current_fiber_);
            lua_pushnil(current_fiber_);
            lua_rawset(current_fiber_, -3);
        } else {
            lua_pop(current_fiber_, 2);
        }

        lua_pop(current_fiber_, 1);
        break;
    case LUA_ERRRUN: //< a runtime error
        // TODO:
        // errored, call handler as in detail::on_fiber_resumed()
        // join algorithm
        throw std::runtime_error{"UNIMPLEMENTED"};
        break;
    case LUA_ERRMEM: //< memory allocation error
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
        rawgetp(L, LUA_REGISTRYINDEX, &detail::error_category_key);
        lua_setmetatable(L, -2);
    }
    lua_rawset(L, -3);

    rawgetp(L, LUA_REGISTRYINDEX, &detail::error_code_key);
    lua_setmetatable(L, -2);
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
        return "Cannot have a module with this name.";
    case static_cast<int>(errc::module_not_found):
        return "Module not found.";
    case static_cast<int>(errc::root_cannot_import_parent):
        return "The root module doesn't have a parent and can't reference"
            " one.";
    case static_cast<int>(errc::cyclic_import):
        return "The module you're trying to import has a dependency on the"
            " current module (and it is partially loaded already).";
    case static_cast<int>(errc::leaf_cannot_import_child):
        return "A leaf module cannot import child modules.";
    case static_cast<int>(errc::failed_to_load_module):
        return "Error when opening/loading module file (maybe invalid lua"
            " syntax).";
    case static_cast<int>(errc::only_main_fiber_may_import):
        return "You can only import modules from the main fiber.";
    case static_cast<int>(errc::bad_root_context):
        return "Bad root context.";
    case static_cast<int>(errc::bad_index):
        return "Requested key wasn't found in the table/userdata.";
    default:
        return "";
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

} // namespace emilua
