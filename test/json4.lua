local println = require('println')
local json = require('json')

println(json.decode('123'))
json.decode('123 true')
