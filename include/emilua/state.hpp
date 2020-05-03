#include <emilua/core.hpp>

#include <boost/asio/io_context.hpp>

namespace emilua {

enum ContextType: lua_Integer
{
    regular_context,
    main,
    test,
    worker,
    error_category,
};

std::shared_ptr<vm_context> make_vm(boost::asio::io_context& ioctx,
                                    int& exit_code,
                                    std::filesystem::path entry_point,
                                    ContextType lua_context);

} // namespace emilua

