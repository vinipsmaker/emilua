if _CONTEXT == 'main' then
    spawn_vm('.', { inherit_ctx = false, concurrency_hint = 1 })
else
    assert(_CONTEXT == 'worker')
    spawn_ctx_threads(0)
end
