local println = require('println')
local mutex = require('mutex')
local cond = require('cond')

local m = mutex.new()
local c = cond.new()

local state = 0

function assert_eq(x, y)
    if x ~= y then
        error('critical check assert_eq() has failed [' .. tostring(x) ..
              ' != ' .. tostring(y) .. ']', 0)
    end
end

spawn(function()
    println('201')
    m:lock()
    println('202')
    assert_eq(state, 0)
    c:wait(m)
    assert_eq(state, 1)
    println('203')
    m:unlock()
end)

println('101')
this_fiber.yield()
this_fiber.yield()
this_fiber.yield()
println('102')
m:lock()
println('103')
state = 1
m:unlock()
this_fiber.yield()
this_fiber.yield()
this_fiber.yield()
println('104')
c:notify_one()
println('105')
