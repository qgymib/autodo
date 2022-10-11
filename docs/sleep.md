# sleep

## SYNOPSIS

```lua
auto.sleep(number timeout)
```

## DESCRIPTION

Pause current coroutine for `timeout` milliseconds.

## RETURN VALUE

The `auto.sleep()` function return nothing.

## NOTES

The paraemter `timeout` cannot be a negative number. The `timeout` is cast to `uint64_t`, so a negative value is actually a very large positive value.
