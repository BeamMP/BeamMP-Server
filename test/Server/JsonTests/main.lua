local function assert_eq(x, y, explain)
	if x ~= y then
		print("assertion '"..explain.."' failed:\n\tgot:\t", x, "\n\texpected:", y)
	end
end

---@param o1 any|table First object to compare
---@param o2 any|table Second object to compare
---@param ignore_mt boolean True to ignore metatables (a recursive function to tests tables inside tables)
function equals(o1, o2, ignore_mt)
    if o1 == o2 then return true end
    local o1Type = type(o1)
    local o2Type = type(o2)
    if o1Type ~= o2Type then return false end
    if o1Type ~= 'table' then return false end

    if not ignore_mt then
        local mt1 = getmetatable(o1)
        if mt1 and mt1.__eq then
            --compare using built in method
            return o1 == o2
        end
    end

    local keySet = {}

    for key1, value1 in pairs(o1) do
        local value2 = o2[key1]
        if value2 == nil or equals(value1, value2, ignore_mt) == false then
            return false
        end
        keySet[key1] = true
    end

    for key2, _ in pairs(o2) do
        if not keySet[key2] then return false end
    end
    return true
end


local function assert_table_eq(x, y, explain)
	if not equals(x, y, true) then
		print("assertion '"..explain.."' failed:\n\tgot:\t", x, "\n\texpected:", y)
	end
end

assert_eq(Util.JsonEncode({1, 2, 3, 4, 5}), "[1,2,3,4,5]", "table to array")
assert_eq(Util.JsonEncode({"a", 1, 2, 3, 4, 5}), '["a",1,2,3,4,5]', "table to array")
assert_eq(Util.JsonEncode({"a", 1, 2.0, 3, 4, 5}), '["a",1,2.0,3,4,5]', "table to array")
assert_eq(Util.JsonEncode({hello="world", john={doe = 1, jane = 2.5, mike = {2, 3, 4}}, dave={}}), '{"dave":{},"hello":"world","john":{"doe":1,"jane":2.5,"mike":[2,3,4]}}', "table to obj")
assert_eq(Util.JsonEncode({a = nil}), "{}", "null obj member")
assert_eq(Util.JsonEncode({1, nil, 3}), "[1,3]", "null array member")
assert_eq(Util.JsonEncode({}), "{}", "empty array/table")
assert_eq(Util.JsonEncode({1234}), "[1234]", "int")
assert_eq(Util.JsonEncode({1234.0}), "[1234.0]", "double")

assert_table_eq(Util.JsonDecode("[1,2,3,4,5]"), {1, 2, 3, 4, 5}, "decode table to array")
assert_table_eq(Util.JsonDecode('["a",1,2,3,4,5]'), {"a", 1, 2, 3, 4, 5}, "decode table to array")
assert_table_eq(Util.JsonDecode('["a",1,2.0,3,4,5]'), {"a", 1, 2.0, 3, 4, 5}, "decode table to array")
assert_table_eq(Util.JsonDecode('{"dave":{},"hello":"world","john":{"doe":1,"jane":2.5,"mike":[2,3,4]}}'), {hello="world", john={doe = 1, jane = 2.5, mike = {2, 3, 4}}, dave={}}, "decode table to obj")
assert_table_eq(Util.JsonDecode("{}"), {a = nil}, "decode null obj member")
assert_table_eq(Util.JsonDecode("[1,3]"), {1, 3}, "decode null array member")
assert_table_eq(Util.JsonDecode("{}"), {}, "decode empty array/table")
assert_table_eq(Util.JsonDecode("[1234]"), {1234}, "decode int")
assert_table_eq(Util.JsonDecode("[1234.0]"), {1234.0}, "decode double")
