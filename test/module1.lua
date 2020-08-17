-- only_main_fiber_may_import

spawn(function()
    require('cond')
end)
