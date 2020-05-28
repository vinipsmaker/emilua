local println = require('println')

println(tostring(this_fiber.local_.test))
this_fiber.local_.test = 'foo'
println(tostring(this_fiber.local_.test))

coroutine.wrap(function()
    println(tostring(this_fiber.local_.test))
end)()

spawn(function()
    println(tostring(this_fiber.local_.test))
    this_fiber.local_.test = 'bar'
    println(tostring(this_fiber.local_.test))
    this_fiber.yield()
    println(tostring(this_fiber.local_.test))
    println('end of secondary fiber')
end)
this_fiber.yield()

println(tostring(this_fiber.local_.test))
println('end of main fiber')
