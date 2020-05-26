local println = require('println')

println(tostring(this_fiber['local'].test))
this_fiber['local'].test = 'foo'
println(tostring(this_fiber['local'].test))

coroutine.resume(coroutine.create(function()
    println(tostring(this_fiber['local'].test))
end))

spawn(function()
    println(tostring(this_fiber['local'].test))
    this_fiber['local'].test = 'bar'
    println(tostring(this_fiber['local'].test))
    this_fiber.yield()
    println(tostring(this_fiber['local'].test))
    println('end of secondary fiber')
end)
this_fiber.yield()

println(tostring(this_fiber['local'].test))
println('end of main fiber')
