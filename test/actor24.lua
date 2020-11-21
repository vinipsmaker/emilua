if _CONTEXT ~= 'main' then
    return
end

local ch = spawn_vm('.')

local m = {}
m.m = m
ch:send(m)
