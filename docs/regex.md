# regex

## SYNOPSIS

```lua
regex auto.regex(string pattern)
```

## DESCRIPTION

Create a regex pattern for futher processing.

## RETURN VALUE

A regex pattern for futher processing.

### regex:match

```lua
list regex:match(string str, int offset)
```

Match `str` with pattern. If match success, the return value is a list of captured groups. If match failed, the return value is nil.

The optional parameter `offset` shows the start position to match, which by default is 0.

The retuend list have following layout:

```
{
    { pos, length, content },
    { pos, length, content },
    ...
}
```
