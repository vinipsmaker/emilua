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

function connect_bootstrap(error, type, next, pcall, e_not_found, e_invalid)
    return function(sock, resolve_results, condition)
        local t_condition = type(condition)
        if
            type(resolve_results) ~= 'table' or
            (t_condition ~= 'nil' and t_condition ~= 'function')
        then
            error(e_invalid)
        end

        local last_error
        for _, v in next, resolve_results do
            if condition and not condition(last_error, v.address, v.port) then
                goto continue
            end

            if sock.is_open then sock:close() end
            local ok, e = pcall(function() sock:connect(v.address, v.port) end)
            if ok then
                return v.address, v.port
            end
            last_error = e

            ::continue::
        end
        if last_error == nil then
            last_error = e_not_found
        end
        error(last_error)
    end
end

connect_bytecode = string.dump(connect_bootstrap, true)
f = io.open(OUTPUT, 'wb')
f:write(connect_bytecode)
f:close()
connect_cdef = strip_xxd_hdr(io.popen('xxd -i ' .. OUTPUT))

function write_bootstrap(stream, buffer)
   local ret = #buffer
   while #buffer > 0 do
       local nwritten = stream:write_some(buffer)
       buffer = buffer:slice(1 + nwritten)
   end
   return ret
end

write_bytecode = string.dump(write_bootstrap, true)
f = io.open(OUTPUT, 'wb')
f:write(write_bytecode)
f:close()
write_cdef = strip_xxd_hdr(io.popen('xxd -i ' .. OUTPUT))

function read_bootstrap(stream, buffer)
    local ret = #buffer
    while #buffer > 0 do
        local nread = stream:read_some(buffer)
        buffer = buffer:slice(1 + nread)
    end
    return ret
end

read_bytecode = string.dump(read_bootstrap, true)
f = io.open(OUTPUT, 'wb')
f:write(read_bytecode)
f:close()
read_cdef = strip_xxd_hdr(io.popen('xxd -i ' .. OUTPUT))

function write_at_bootstrap(io_obj, offset, buffer)
   local ret = #buffer
   while #buffer > 0 do
       local nwritten = io_obj:write_some_at(offset, buffer)
       offset = offset + nwritten
       buffer = buffer:slice(1 + nwritten)
   end
   return ret
end

write_at_bytecode = string.dump(write_at_bootstrap, true)
f = io.open(OUTPUT, 'wb')
f:write(write_at_bytecode)
f:close()
write_at_cdef = strip_xxd_hdr(io.popen('xxd -i ' .. OUTPUT))

function read_at_bootstrap(io_obj, offset, buffer)
    local ret = #buffer
    while #buffer > 0 do
        local nread = io_obj:read_some_at(offset, buffer)
        offset = offset + nread
        buffer = buffer:slice(1 + nread)
    end
    return ret
end

read_at_bytecode = string.dump(read_at_bootstrap, true)
f = io.open(OUTPUT, 'wb')
f:write(read_at_bytecode)
f:close()
read_at_cdef = strip_xxd_hdr(io.popen('xxd -i ' .. OUTPUT))

function get_line_bootstrap(type, pcall, error, byte_span_new, regex_search,
                            re_search_flags, regex_split, regex_patsplit, lws,
                            EEOF, EMSGSIZE)
    return function(self)
        local ready_wnd = self.buffer_:slice(
            1, self.buffer_used - self.record_size)
        if self.record_size > 0 then
            ready_wnd:copy(self.buffer_:slice(1 + self.record_size))
            self.buffer_used = self.buffer_used - self.record_size
            self.record_size = 0
        end

        local record_separator = self.record_separator
        local record_separator_type = type(record_separator)
        local trim_record = self.trim_record
        local field_separator = self.field_separator
        local field_separator_type = type(field_separator)
        local field_pattern = self.field_pattern
        local field_pattern_type = type(field_pattern)
        local max_record_size = self.max_record_size
        local stream = self.stream
        local read_some = stream.read_some
        while true do
            local line
            if record_separator_type == 'string' then
                local idx = ready_wnd:find(record_separator)
                if idx then
                    line = ready_wnd:slice(1, idx - 1)
                    self.record_terminator = record_separator
                    self.record_size = idx - 1 + #record_separator
                end
            else
                local m = regex_search(record_separator, ready_wnd,
                                       re_search_flags)
                if not m.empty then
                    line = ready_wnd:slice(1, m[0].start - 1)
                    self.record_terminator = ready_wnd:slice(
                        m[0].start, m[0].end_)
                    self.record_size = m[0].end_
                end
            end
            if line then
                if trim_record then
                    local start = line:find_first_not_of(lws)
                    if start then
                        local end_ = line:find_last_not_of(lws)
                        line = line:slice(start, end_)
                    else
                        line = byte_span_new(0)
                    end
                end
                self.record_number = self.record_number + 1
                if field_separator then
                    if field_separator_type == 'string' then
                        local ret = {}
                        if #line == 0 then
                            return ret
                        end
                        local nf = 1
                        local idx = line:find(field_separator)
                        while idx do
                            ret[nf] = line:slice(1, idx - 1)
                            nf = nf + 1
                            -- TODO: use several indexes to avoid reslicing so
                            -- much
                            line = line:slice(idx + #field_separator)
                            idx = line:find(field_separator)
                        end
                        ret[nf] = line
                        return ret
                    else
                        return regex_split(field_separator, line)
                    end
                elseif field_pattern then
                    return regex_patsplit(field_pattern, line)
                else
                    return line
                end
            end
            if #self.buffer_ == self.buffer_used then
                local new_size = #self.buffer_ * 2
                if new_size > max_record_size then
                    new_size = max_record_size
                end
                if #self.buffer_ >= new_size then
                    error(EMSGSIZE, 0)
                end
                local new_buffer = byte_span_new(new_size)
                new_buffer:copy(self.buffer_)
                self.buffer_ = new_buffer
            end
            local ok, nread = pcall(read_some, stream,
                                    self.buffer_:slice(1 + self.buffer_used))
            if not ok then
                if nread ~= EEOF or #ready_wnd == 0 then
                    error(nread, 0)
                end
                if trim_record then
                    local start = ready_wnd:find_first_not_of(lws)
                    if start then
                        local end_ = ready_wnd:find_last_not_of(lws)
                        ready_wnd = ready_wnd:slice(start, end_)
                    else
                        ready_wnd = byte_span_new(0)
                    end
                end
                if record_separator_type == 'string' then
                    self.record_terminator = ''
                else
                    self.record_terminator = byte_span_new(0)
                end
                self.record_size = #ready_wnd
                self.record_number = self.record_number + 1
                if field_separator then
                    if field_separator_type == 'string' then
                        local ret = {}
                        if #ready_wnd == 0 then
                            return ret
                        end
                        local nf = 1
                        local idx = ready_wnd:find(field_separator)
                        while idx do
                            ret[nf] = ready_wnd:slice(1, idx - 1)
                            nf = nf + 1
                            -- TODO: use several indexes to avoid reslicing so
                            -- much
                            ready_wnd = ready_wnd:slice(idx + #field_separator)
                            idx = ready_wnd:find(field_separator)
                        end
                        ret[nf] = ready_wnd
                        return ret
                    else
                        return regex_split(field_separator, ready_wnd)
                    end
                elseif field_pattern then
                    return regex_patsplit(field_pattern, ready_wnd)
                else
                    return ready_wnd
                end
            end
            self.buffer_used = self.buffer_used + nread
            ready_wnd = self.buffer_:slice(1, self.buffer_used)
        end
    end
end

get_line_bytecode = string.dump(get_line_bootstrap, true)
f = io.open(OUTPUT, 'wb')
f:write(get_line_bytecode)
f:close()
get_line_cdef = strip_xxd_hdr(io.popen('xxd -i ' .. OUTPUT))

function scanner_buffer_bootstrap(self)
    -- TODO: Golang doesn't use the buffer's head. Golang uses a moving window
    -- over the buffer. Golang's strategy allows for less memory copies. Emilua
    -- should do the same later down the road.
    return self.buffer_:slice(1, self.buffer_used)
end

scanner_buffer_bytecode = string.dump(scanner_buffer_bootstrap, true)
f = io.open(OUTPUT, 'wb')
f:write(scanner_buffer_bytecode)
f:close()
scanner_buffer_cdef = strip_xxd_hdr(io.popen('xxd -i ' .. OUTPUT))

function scanner_set_buffer_bootstrap(self, buffer)
    self.buffer_ = buffer
    self.buffer_used = 0
    self.record_size = 0
    self.record_terminator = nil
end

scanner_set_buffer_bytecode = string.dump(scanner_set_buffer_bootstrap, true)
f = io.open(OUTPUT, 'wb')
f:write(scanner_set_buffer_bytecode)
f:close()
scanner_set_buffer_cdef = strip_xxd_hdr(io.popen('xxd -i ' .. OUTPUT))

function scanner_remove_line_bootstrap(self)
    local ready_wnd = self.buffer_:slice(1, self.buffer_used - self.record_size)
    ready_wnd:copy(self.buffer_:slice(1 + self.record_size))
    self.buffer_used = self.buffer_used - self.record_size
    self.record_size = 0
    self.record_terminator = nil
end

scanner_remove_line_bytecode = string.dump(
    scanner_remove_line_bootstrap, true)
f = io.open(OUTPUT, 'wb')
f:write(scanner_remove_line_bytecode)
f:close()
scanner_remove_line_cdef = strip_xxd_hdr(io.popen('xxd -i ' .. OUTPUT))

function scanner_buffered_line_bootstrap(self)
    return self.buffer_:slice(1, self.record_size - #self.record_terminator)
end

scanner_buffered_line_bytecode = string.dump(
    scanner_buffered_line_bootstrap, true)
f = io.open(OUTPUT, 'wb')
f:write(scanner_buffered_line_bytecode)
f:close()
scanner_buffered_line_cdef = strip_xxd_hdr(io.popen('xxd -i ' .. OUTPUT))

function scanner_new_bootstrap(mt, setmetatable, byte_span_new)
    -- default sizes borrowed from Golang's bufio.Scanner
    local INITIAL_BUFFER_SIZE = 4096
    local MAX_RECORD_SIZE = 64 * 1024

    return function(ret)
        if ret == nil then
            ret = {}
        end

        if not ret.record_separator then
            -- good default for network protocols
            ret.record_separator = '\r\n'
        end

        ret.buffer_ = byte_span_new(INITIAL_BUFFER_SIZE)
        ret.buffer_used = 0
        ret.record_size = 0
        ret.record_number = 0

        if not ret.max_record_size then
            ret.max_record_size = MAX_RECORD_SIZE
        end

        setmetatable(ret, mt)
        return ret
    end
end

scanner_new_bytecode = string.dump(scanner_new_bootstrap, true)
f = io.open(OUTPUT, 'wb')
f:write(scanner_new_bytecode)
f:close()
scanner_new_cdef = strip_xxd_hdr(io.popen('xxd -i ' .. OUTPUT))

function scanner_with_awk_defaults_bootstrap(
    mt, setmetatable, byte_span_new, regex_new
)
    -- default sizes borrowed from Golang's bufio.Scanner
    local INITIAL_BUFFER_SIZE = 4096
    local MAX_RECORD_SIZE = 64 * 1024

    local FS = regex_new{
        pattern = '[ \f\n\r\t\v]+',
        grammar = 'extended',
        nosubs = true,
        optimize = true
    }

    return function(stream)
        local ret = {
            trim_record = true,
            record_separator = '\n',
            field_separator = FS,

            buffer_ = byte_span_new(INITIAL_BUFFER_SIZE),
            buffer_used = 0,
            record_size = 0,
            record_number = 0,
            max_record_size = MAX_RECORD_SIZE,

            stream = stream
        }

        setmetatable(ret, mt)
        return ret
    end
end

scanner_with_awk_defaults_bytecode = string.dump(
    scanner_with_awk_defaults_bootstrap, true)
f = io.open(OUTPUT, 'wb')
f:write(scanner_with_awk_defaults_bytecode)
f:close()
scanner_with_awk_defaults_cdef = strip_xxd_hdr(io.popen('xxd -i ' .. OUTPUT))

f = io.open(OUTPUT, 'wb')

f:write([[
#include <cstddef>

namespace emilua {
unsigned char stream_connect_bytecode[] = {
]])

f:write(connect_cdef)

f:write('};')
f:write(string.format('std::size_t stream_connect_bytecode_size = %i;', #connect_bytecode))

f:write([[
unsigned char write_bytecode[] = {
]])

f:write(write_cdef)

f:write('};')
f:write(string.format('std::size_t write_bytecode_size = %i;', #write_bytecode))

f:write([[
unsigned char read_bytecode[] = {
]])

f:write(read_cdef)

f:write('};')
f:write(string.format('std::size_t read_bytecode_size = %i;', #read_bytecode))

f:write([[
unsigned char write_at_bytecode[] = {
]])

f:write(write_at_cdef)

f:write('};')
f:write(string.format('std::size_t write_at_bytecode_size = %i;',
                      #write_at_bytecode))

f:write([[
unsigned char read_at_bytecode[] = {
]])

f:write(read_at_cdef)

f:write('};')
f:write(string.format('std::size_t read_at_bytecode_size = %i;',
                      #read_at_bytecode))

f:write([[
unsigned char get_line_bytecode[] = {
]])

f:write(get_line_cdef)

f:write('};')
f:write(string.format('std::size_t get_line_bytecode_size = %i;',
                      #get_line_bytecode))

f:write([[
unsigned char scanner_buffer_bytecode[] = {
]])

f:write(scanner_buffer_cdef)

f:write('};')
f:write(string.format('std::size_t scanner_buffer_bytecode_size = %i;',
                      #scanner_buffer_bytecode))

f:write([[
unsigned char scanner_set_buffer_bytecode[] = {
]])

f:write(scanner_set_buffer_cdef)

f:write('};')
f:write(string.format('std::size_t scanner_set_buffer_bytecode_size = %i;',
                      #scanner_set_buffer_bytecode))

f:write([[
unsigned char scanner_remove_line_bytecode[] = {
]])

f:write(scanner_remove_line_cdef)

f:write('};')
f:write(string.format('std::size_t scanner_remove_line_bytecode_size = %i;',
                      #scanner_remove_line_bytecode))

f:write([[
unsigned char scanner_buffered_line_bytecode[] = {
]])

f:write(scanner_buffered_line_cdef)

f:write('};')
f:write(string.format('std::size_t scanner_buffered_line_bytecode_size = %i;',
                      #scanner_buffered_line_bytecode))

f:write([[
unsigned char scanner_new_bytecode[] = {
]])

f:write(scanner_new_cdef)

f:write('};')
f:write(string.format('std::size_t scanner_new_bytecode_size = %i;',
                      #scanner_new_bytecode))

f:write([[
unsigned char scanner_with_awk_defaults_bytecode[] = {
]])

f:write(scanner_with_awk_defaults_cdef)

f:write('};')
f:write(string.format(
    'std::size_t scanner_with_awk_defaults_bytecode_size = %i;',
    #scanner_with_awk_defaults_bytecode))

f:write('} // namespace emilua')
