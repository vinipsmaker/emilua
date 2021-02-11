#include <emilua/plugin.hpp>

namespace emilua {

void plugin::init_appctx(app_context&) noexcept
{}

void plugin::init_ioctx_services(asio::io_context&) noexcept
{}

} // namespace emilua
