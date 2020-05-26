local println = require('println')
local sleep_for = require('sleep_for')

local numbers = {8, 42, 38, 111, 2, 39, 1}

for _, n in pairs(numbers) do
    spawn(function()
        sleep_for(n * 10)
        println(n)
    end)
end