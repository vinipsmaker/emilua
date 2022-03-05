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

function var_args__retval1_to_error__fwd_retval2__bootstrap(error, native)
    return function(...)
        local e, v = native(...)
        if e then
            error(e, 0)
        end
        return v
    end
end

var_args__retval1_to_error__fwd_retval2__bytecode = string.dump(
    var_args__retval1_to_error__fwd_retval2__bootstrap, true)
f = io.open(OUTPUT, 'wb')
f:write(var_args__retval1_to_error__fwd_retval2__bytecode)
f:close()
var_args__retval1_to_error__fwd_retval2_cdef =
    strip_xxd_hdr(io.popen('xxd -i ' .. OUTPUT))

function var_args__retval1_to_error__bootstrap(error, native)
    return function(...)
        local e = native(...)
        if e then
            error(e, 0)
        end
    end
end

var_args__retval1_to_error__bytecode = string.dump(
    var_args__retval1_to_error__bootstrap, true)
f = io.open(OUTPUT, 'wb')
f:write(var_args__retval1_to_error__bytecode)
f:close()
var_args__retval1_to_error__cdef = strip_xxd_hdr(io.popen('xxd -i ' .. OUTPUT))

function var_args__retval1_to_error__fwd_retval234__bootstrap(error, native)
    return function(...)
        local e, v1, v2, v3 = native(...)
        if e then
            error(e, 0)
        end
        return v1, v2, v3
    end
end

var_args__retval1_to_error__fwd_retval234__bytecode = string.dump(
    var_args__retval1_to_error__fwd_retval234__bootstrap, true)
f = io.open(OUTPUT, 'wb')
f:write(var_args__retval1_to_error__fwd_retval234__bytecode)
f:close()
var_args__retval1_to_error__fwd_retval234__cdef =
    strip_xxd_hdr(io.popen('xxd -i ' .. OUTPUT))

function var_args__retval1_to_error__fwd_retval23__bootstrap(error, native)
    return function(...)
        local e, v1, v2 = native(...)
        if e then
            error(e, 0)
        end
        return v1, v2
    end
end

var_args__retval1_to_error__fwd_retval23__bytecode = string.dump(
    var_args__retval1_to_error__fwd_retval23__bootstrap, true)
f = io.open(OUTPUT, 'wb')
f:write(var_args__retval1_to_error__fwd_retval23__bytecode)
f:close()
var_args__retval1_to_error__fwd_retval23__cdef =
    strip_xxd_hdr(io.popen('xxd -i ' .. OUTPUT))

f = io.open(OUTPUT, 'wb')

f:write([[
#include <cstddef>

namespace emilua {
unsigned char var_args__retval1_to_error__fwd_retval2__bytecode[] = {
]])

f:write(var_args__retval1_to_error__fwd_retval2_cdef)

f:write('};')
f:write(string.format(
    'std::size_t var_args__retval1_to_error__fwd_retval2__bytecode_size = %i;',
    #var_args__retval1_to_error__fwd_retval2__bytecode))

f:write([[
unsigned char var_args__retval1_to_error__bytecode[] = {
]])

f:write(var_args__retval1_to_error__cdef)

f:write('};')
f:write(string.format(
    'std::size_t var_args__retval1_to_error__bytecode_size = %i;',
    #var_args__retval1_to_error__bytecode))

f:write([[
unsigned char var_args__retval1_to_error__fwd_retval234__bytecode[] = {
]])

f:write(var_args__retval1_to_error__fwd_retval234__cdef)

f:write('};')
f:write(string.format(
    'std::size_t ' ..
    'var_args__retval1_to_error__fwd_retval234__bytecode_size = %i;',
    #var_args__retval1_to_error__fwd_retval234__bytecode))

f:write([[
unsigned char var_args__retval1_to_error__fwd_retval23__bytecode[] = {
]])

f:write(var_args__retval1_to_error__fwd_retval23__cdef)

f:write('};')
f:write(string.format(
    'std::size_t var_args__retval1_to_error__fwd_retval23__bytecode_size = %i;',
    #var_args__retval1_to_error__fwd_retval23__bytecode))

f:write('} // namespace emilua')
