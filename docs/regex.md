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
bool,list regex:match(string str)
```

Match `str` with pattern. The first return value is whether match success. If match success, the second value is a list of captured groups. If match failed, the second value is nil.
