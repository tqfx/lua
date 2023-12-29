s = { { 1, 2 }, { 3, 4 }, ["'1"] = { 5, 6 }, ["\"1"] = { 7, 8 }, ["_1"] = { 9, 10 } }
s1 = { { 1, 2 }, { 3, 4 }, ["'1"] = { 5, 6 }, ["\"1"] = { 7, 8 }, ["_1"] = { 9, 10 } }
t = setmetatable({ func = function(self) return self end }, { __index = s })
t1 = setmetatable({ func = function(self) return self end }, s1)
s1.__newindex = function(self, key, val)
    s1[key] = val
end
s1.__index = function(self, key)
    if key == "__index" then
        return s1
    end
    return s1[key]
end
