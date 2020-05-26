local println = require('println')

spawn(function()
    println('second')
    this_fiber.yield()
    println('fourth')
end):detach()

println('first')
this_fiber.yield()
println('third')
