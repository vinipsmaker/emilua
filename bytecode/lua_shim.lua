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

function coroutine_create_bootstrap(create, mark_yield_as_non_native, unpack)
    return function(f)
        return create(function(...)
            local ret = {f(...)}
            mark_yield_as_non_native()
            return unpack(ret)
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
                                    clear_busy, eperm, error, unpack)
    return function(co, ...)
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

f:write('} // namespace emilua')
