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

function coroutine_create_bootstrap(create, create_root_scope, root_scope,
                                    set_current_traceback,
                                    terminate_vm_with_cleanup_error,
                                    mark_yield_as_non_native, xpcall, pcall,
                                    error, unpack)
    return function(f)
        return create(function(...)
            create_root_scope()
            local args = {...}
            local ret = {xpcall(
                function()
                    local ret = {f(unpack(args))}
                    mark_yield_as_non_native()
                    return unpack(ret)
                end,
                function(e)
                    set_current_traceback()
                    return e
                end
            )}
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
            if ret[1] == false then
                error(ret[2])
            end
            return unpack(ret, 2)
        end)
    end
end

coroutine_create_bytecode = string.dump(coroutine_create_bootstrap, true)
f = io.open(OUTPUT, 'wb')
f:write(coroutine_create_bytecode)
f:close()
coroutine_create_cdef = strip_xxd_hdr(io.popen('xxd -i ' .. OUTPUT))

function coroutine_resume_bootstrap(resume, yield, is_yield_native,
                                    mark_yield_as_native, is_busy, set_busy,
                                    clear_busy, eperm, check_not_interrupted,
                                    error, unpack)
    return function(co, ...)
        check_not_interrupted()

        if is_busy(co) then
            error(eperm)
        end

        local args = {...}
        while true do
            local ret = {resume(co, unpack(args))}
            if ret[1] == false then
                mark_yield_as_native()
                return unpack(ret)
            end
            if is_yield_native() then
                set_busy(co)
                args = {yield(unpack(ret, 2))}
                clear_busy(co)
            else
                mark_yield_as_native()
                return unpack(ret)
            end
        end
    end
end

coroutine_resume_bytecode = string.dump(coroutine_resume_bootstrap, true)
f = io.open(OUTPUT, 'wb')
f:write(coroutine_resume_bytecode)
f:close()
coroutine_resume_cdef = strip_xxd_hdr(io.popen('xxd -i ' .. OUTPUT))

function coroutine_wrap_bootstrap(new_create, new_resume, error, unpack)
    return function(f)
        local co = new_create(f)
        return function(...)
            local ret = {new_resume(co, ...)}
            if ret[1] == false then
                error(ret[2])
            end
            return unpack(ret, 2)
        end
    end
end

coroutine_wrap_bytecode = string.dump(coroutine_wrap_bootstrap, true)
f = io.open(OUTPUT, 'wb')
f:write(coroutine_wrap_bytecode)
f:close()
coroutine_wrap_cdef = strip_xxd_hdr(io.popen('xxd -i ' .. OUTPUT))

function pcall_bootstrap(pcall, scope_push, scope_pop,
                         terminate_vm_with_cleanup_error, restore_interruption,
                         check_not_interrupted, unpack)
    return function(f, ...)
        check_not_interrupted()
        scope_push()
        local ret = {pcall(f, ...)}
        do
            local cleanup_handlers = scope_pop()
            local i = #cleanup_handlers
            while i > 0 do
                local ok = pcall(cleanup_handlers[i])
                i = i - 1
                if not ok then
                    terminate_vm_with_cleanup_error()
                end
            end
            -- scope_pop() already calls disable_interruption()
            restore_interruption()
        end
        return unpack(ret)
    end
end

pcall_bytecode = string.dump(pcall_bootstrap, true)
f = io.open(OUTPUT, 'wb')
f:write(pcall_bytecode)
f:close()
pcall_cdef = strip_xxd_hdr(io.popen('xxd -i ' .. OUTPUT))

function xpcall_bootstrap(xpcall, pcall, scope_push, scope_pop,
                          terminate_vm_with_cleanup_error, restore_interruption,
                          check_not_interrupted, unpack)
    return function(f, err)
        check_not_interrupted()
        scope_push()
        local ret = {xpcall(f, err)}
        do
            local cleanup_handlers = scope_pop()
            local i = #cleanup_handlers
            while i > 0 do
                local ok = pcall(cleanup_handlers[i])
                i = i - 1
                if not ok then
                    terminate_vm_with_cleanup_error()
                end
            end
            -- scope_pop() already calls disable_interruption()
            restore_interruption()
        end
        return unpack(ret)
    end
end

xpcall_bytecode = string.dump(xpcall_bootstrap, true)
f = io.open(OUTPUT, 'wb')
f:write(xpcall_bytecode)
f:close()
xpcall_cdef = strip_xxd_hdr(io.popen('xxd -i ' .. OUTPUT))

f = io.open(OUTPUT, 'wb')

f:write([[
#include <cstddef>

namespace emilua {
unsigned char coroutine_create_bytecode[] = {
]])

f:write(coroutine_create_cdef)

f:write('};')
f:write(string.format('std::size_t coroutine_create_bytecode_size = %i;',
                      #coroutine_create_bytecode))

f:write([[
unsigned char coroutine_resume_bytecode[] = {
]])

f:write(coroutine_resume_cdef)

f:write('};')
f:write(string.format('std::size_t coroutine_resume_bytecode_size = %i;',
                      #coroutine_resume_bytecode))

f:write([[
unsigned char coroutine_wrap_bytecode[] = {
]])

f:write(coroutine_wrap_cdef)

f:write('};')
f:write(string.format('std::size_t coroutine_wrap_bytecode_size = %i;',
                      #coroutine_wrap_bytecode))

f:write([[
unsigned char pcall_bytecode[] = {
]])

f:write(pcall_cdef)

f:write('};')
f:write(string.format('std::size_t pcall_bytecode_size = %i;', #pcall_bytecode))

f:write([[
unsigned char xpcall_bytecode[] = {
]])

f:write(xpcall_cdef)

f:write('};')
f:write(string.format('std::size_t xpcall_bytecode_size = %i;',
                      #xpcall_bytecode))

f:write('} // namespace emilua')
