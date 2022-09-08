
sum = 0

auto.coroutine(function()
    for i=1,10 do
        sum = sum + 1
        coroutine.yield(1)
    end
end)

auto.coroutine(function()
    for i=1,20 do
        sum = sum + 1
        coroutine.yield(1)
    end
end)

auto.sleep(1000)
assert(sum == 30)

