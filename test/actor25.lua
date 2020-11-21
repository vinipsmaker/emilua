if _CONTEXT ~= 'main' then
    return
end

local ch = spawn_vm('.')

local m = {}
m.a = {}
m.a.a = m.a
ch:send(m)
