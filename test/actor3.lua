if _CONTEXT ~= 'main' then
    return
end

local ch = spawn_vm('.')
ch:send('foobar')
