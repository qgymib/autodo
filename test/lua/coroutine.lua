io.write("hello world\n")

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

assert(c1:await() == 20)
assert(c2:await() == 40)
io.write("finish\n")
