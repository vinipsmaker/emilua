#include <emilua/plugin.hpp>

namespace emilua {

void plugin::init_appctx(app_context&) noexcept
{}

std::error_code plugin::init_ioctx_services(asio::io_context&) noexcept
{
    return {};
}

std::error_code plugin::init_lua_module(vm_context&, lua_State*)
{
    return errc::internal_module;
}

} // namespace emilua
