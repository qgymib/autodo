c1 = auto.coroutine(function(sum)
    for i=1,10 do
        sum = sum + 1
        coroutine.yield(1)
    end
    return sum
end, 10)

c2 = auto.coroutine(function(sum)
    for i=1,20 do
        sum = sum + 1
        coroutine.yield(1)
    end
    return sum
end, 20)

local b1, ret1 = c1:await()
local b2, ret2 = c2:await()

assert(b1 == true)
assert(b2 == true)

assert(ret1 == 20)
assert(ret2 == 40)
