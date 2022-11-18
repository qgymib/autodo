# fs_iterdir

## SYNOPSIS

```lua
next,table,nil auto.fs_iterdir(path)
```

## DESCRIPTION

List all entry in `path`. It is typical used in `for` syntax like:

```lua
for _, p in auto.fs_iterdir(path) do
    io.write(p)
end
```

The value `p` is a full path to entry in `path`. If `path` is relative, `p` is relative. If `path` is absolute, `p` is absolute.

## RETURN VALUE

A next function, a table, and nil. It is used in iterate over scene.

Checkout [__pairs](https://www.lua.org/manual/5.4/manual.html#pdf-pairs) for details.
