# string_split

## SYNOPSIS

```lua
list auto.string_split(string, pattern)
```

## DESCRIPTION

Split `string` into string sequence by `pattern`.

The `pattern` is a regular expression, every position that match with `pattern` will be splited, the match part is not included.

## RETURN VALUE

A string sequence.
