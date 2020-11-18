local json = require('json')

print(json.null)

print('--')

print(json.is_array())
print(json.is_array(nil))
print(json.is_array(false))
print(json.is_array(1234))
print(json.is_array(''))
print(json.is_array({}))

print('--')

local x = {}
print(json.is_array(x))
print(x)
print(json.into_array(x))
print(json.is_array(x))
print(json.into_array(x))
print(json.is_array(x))
print(json.is_array(json.into_array()))

print('--')

local ok, err = pcall(function() json.into_array('') end)
print(ok, err)

setmetatable(x, {})
ok, err = pcall(function() json.into_array(x) end)
print(ok, err)
