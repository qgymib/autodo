# coroutine

## SYNOPSIS

```lua
coroutine auto.coroutine(callback, ...)
```

The returned token have following method:

```lua
coroutine:await()
coroutine:suspend()
coroutine:resume()
```

## DESCRIPTION

Create a managed coroutine and put it into schedule queue. The coroutine will be scheduled as soon as possible.

A managed coroutine is a lua coruinte which is automatically scheduled by AutoDo. It have following features:
+ It is automatically scheduled by AutoDo.
+ Any uncatched error stop the whole Lua VM by default.


