if _CONTEXT == 'main' then
    spawn_vm('.', { inherit_context = false, concurrency_hint = 1 })
else
    assert(_CONTEXT == 'worker')
    spawn_context_threads(0)
end
