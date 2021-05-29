OUTPUT = arg[#arg]

function strip_xxd_hdr(file)
    local lines = {}
    for line in file:lines() do
        lines[#lines + 1] = line
    end
    table.remove(lines, 1)
    lines[#lines] = nil
    lines[#lines] = nil
    return table.concat(lines)
end

function start_fn_bootstrap(root_scope, set_current_traceback,
                            terminate_vm_with_cleanup_error, xpcall, pcall,
                            error, start_fn)
    return function()
        local ok, e = xpcall(
            start_fn,
            function(e)
                set_current_traceback()
                return e
            end
        )
        if ok == false then
            error(e, 0)
        end
        do
            local cleanup_handlers = root_scope()
            local i = #cleanup_handlers
            while i > 0 do
                local ok = pcall(cleanup_handlers[i])
                i = i - 1
                if not ok then
                    terminate_vm_with_cleanup_error()
                end
            end
        end
    end
end

start_fn_bytecode = string.dump(start_fn_bootstrap, true)
f = io.open(OUTPUT, 'wb')
f:write(start_fn_bytecode)
f:close()
start_fn_cdef = strip_xxd_hdr(io.popen('xxd -i ' .. OUTPUT))

function module_start_fn_bootstrap(root_scope, terminate_vm_with_cleanup_error,
                                   pcall, error, mark_module_as_loaded,
                                   start_fn)
    return function()
        local ok, e = pcall(start_fn)
        do
            local cleanup_handlers = root_scope()
            local i = #cleanup_handlers
            while i > 0 do
                local ok = pcall(cleanup_handlers[i])
                i = i - 1
                if not ok then
                    terminate_vm_with_cleanup_error()
                end
            end
        end
        if ok == false then
            error(e, 0)
        end
        return mark_module_as_loaded()
    end
end

module_start_fn_bytecode = string.dump(module_start_fn_bootstrap, true)
f = io.open(OUTPUT, 'wb')
f:write(module_start_fn_bytecode)
f:close()
module_start_fn_cdef = strip_xxd_hdr(io.popen('xxd -i ' .. OUTPUT))

function require_bootstrap(error, native)
    return function(module)
        local ok, val = native(module)
        if ok then
            return val
        else
            error(val, 0)
        end
    end
end

require_bytecode = string.dump(require_bootstrap, true)
f = io.open(OUTPUT, 'wb')
f:write(require_bytecode)
f:close()
require_cdef = strip_xxd_hdr(io.popen('xxd -i ' .. OUTPUT))

f = io.open(OUTPUT, 'wb')

f:write([[
#include <cstddef>

namespace emilua {
unsigned char start_fn_bytecode[] = {
]])

f:write(start_fn_cdef)

f:write('};')
f:write(string.format('std::size_t start_fn_bytecode_size = %i;',
                      #start_fn_bytecode))

f:write('unsigned char module_start_fn_bytecode[] = {')
f:write(module_start_fn_cdef)

f:write('};')
f:write(string.format('std::size_t module_start_fn_bytecode_size = %i;',
                      #module_start_fn_bytecode))

f:write('unsigned char require_bytecode[] = {')
f:write(require_cdef)

f:write('};')
f:write(string.format('std::size_t require_bytecode_size = %i;',
                      #require_bytecode))

f:write('} // namespace emilua')
