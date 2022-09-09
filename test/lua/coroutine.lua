io.write("hello world\n")

c1 = auto.coroutine(function()
    local sum = 0
    for i=1,10 do
        sum = sum + 1
        coroutine.yield(1)
    end
    return sum
end)

c2 = auto.coroutine(function()
    local sum = 0
    for i=1,20 do
        sum = sum + 1
        coroutine.yield(1)
    end
    return sum
end)

assert(c1:await() == 10)
assert(c2:await() == 20)
io.write("finish\n")
