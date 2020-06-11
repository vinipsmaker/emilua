-- Multiple waiters

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
    state = 2
    m:unlock()
end)

spawn(function()
    println('301')
    m:lock()
    println('302')
    assert_eq(state, 0)
    c:wait(m)
    assert_eq(state, 2)
    println('303')
    m:unlock()
end)

this_fiber.yield()
println('101')
m:lock()
println('102')
state = 1
m:unlock()
println('103')
c:notify_all()
println('104')
