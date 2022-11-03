# coroutine

## SYNOPSIS

```lua
coroutine auto.coroutine(callback, ...)
```

## DESCRIPTION

Create a managed coroutine and put it into schedule queue. The coroutine will be scheduled as soon as possible.

A managed coroutine is a lua coruinte which is automatically scheduled by AutoDo. It have following features:
+ It is automatically scheduled by AutoDo.
+ Any uncatched error stop the whole Lua VM by default.

## RETURN VALUE

### coroutine:await

```lua
bool,... coroutine:await()
```

Wait for coroutine finish.

The first return value is whether execute success. If true, the remain values is the returned value from coroutine. If false, it means the coroutine either error occur or closed by user. If it is closed by user, the second return value is nil. If error occur, the second return value is error object.

### coroutine:suspend

```lua
coroutine:suspend()
```

Suspend coroutine.

### coroutine:resume

```lua
coroutine:resume()
```

Resume coroutine.
