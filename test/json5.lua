local println = require('println')
local json = require('json')

println(tostring(json.decode('{}')))
json.decode('{} true')
