local json = require('json')

local function my_print(x)
    print(type(x), x)
end

my_print(json.decode('1'))
my_print(json.decode('1.5'))
my_print(json.decode('false'))
my_print(json.decode('true'))
my_print(json.decode('null'))
my_print(json.decode('"hello world"'))
