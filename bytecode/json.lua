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

function json_encode_bootstrap(writer_new, get_tojson, json_is_array, json_null,
                               type, error, EINVAL, ECYCLE, ENOTSUP, pairs,
                               ipairs)
    -- That's how Y-combinators are done in Lua
    local function json_encode_recur(value, state, writer, visited, indent)
        local t = type(value)
        if t == 'nil' or t == 'function' or t == 'thread' then
            error(EINVAL, 0)
        elseif t == 'boolean' or t == 'number' or t == 'string' or
            value == json_null then
            writer:value(value)
            return
        end

        do
            local tojson = get_tojson(value)
            if tojson then
                if visited[value] then error(ECYCLE, 0) end
                visited[value] = true
                tojson(value, state)
                visited[value] = nil
                return
            elseif t == 'userdata' then
                error(EINVAL, 0)
            end
        end

        -- `assert(t == 'table')`
        if visited[value] then error(ECYCLE, 0) end
        visited[value] = true
        if json_is_array(value) or #value > 0 then
            writer:begin_array()
            for _, v in ipairs(value) do
                -- not actually permissive to encode arrays (except for numbers
                -- that might become null when inf or nan is given as input)
                json_encode_recur(v, state, writer, visited, indent)
            end
            writer:end_array()
        else
            writer:begin_object()
            for k, v in pairs(value) do
                local t = type(v)
                if type(k) ~= 'string' or t == 'function' or t == 'thread' then
                    goto continue
                elseif t == 'boolean' or t == 'number' or t == 'string' or
                    v == json_null then
                    writer:value(k)
                    writer:value(v)
                    goto continue
                elseif t == 'userdata' then
                    local tojson = get_tojson(v)
                    if tojson then
                        if visited[v] then error(ECYCLE, 0) end
                        visited[v] = true
                        writer:value(k)
                        tojson(v, state)
                        visited[v] = nil
                    end
                    goto continue
                end

                writer:value(k)
                json_encode_recur(v, state, writer, visited, indent)

                ::continue::
            end
            writer:end_object()
        end
        visited[value] = nil
    end

    return function(value, opts)
        local state = opts and opts.state
        local writer
        local visited
        local indent

        if state == nil then
            writer = writer_new()
            visited = {}
            indent = opts and opts.indent
            state = {
                writer = writer,
                visited = visited,
                indent = indent
            }
        else
            writer = state.writer
            visited = state.visited
            indent = state.indent
        end

        if indent then error(ENOTSUP, 0) end

        json_encode_recur(value, state, writer, visited, indent)
        if opts and opts.state then
            return
        end
        return writer:generate()
    end
end

json_encode_bytecode = string.dump(json_encode_bootstrap, true)
f = io.open(OUTPUT, 'wb')
f:write(json_encode_bytecode)
f:close()
json_encode_cdef = strip_xxd_hdr(io.popen('xxd -i ' .. OUTPUT))

f = io.open(OUTPUT, 'wb')

f:write([[
#include <cstddef>

namespace emilua {
unsigned char json_encode_bytecode[] = {
]])

f:write(json_encode_cdef)

f:write('};')
f:write(string.format('std::size_t json_encode_bytecode_size = %i;',
                      #json_encode_bytecode))

f:write('} // namespace emilua')
