local println = require('println')
local json = require('json')

println(tostring(json.decode('30')))
json.decode('30000000000000000000000000000000000000000000000')
