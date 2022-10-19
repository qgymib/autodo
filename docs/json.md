# json

## SYNOPSIS

```lua
json auto.json()
```

## DESCRIPTION

Create a json parser for encode and decode json string.

## RETURN VALUE

A token for encode and decode json string.

### json:encode

```lua
string json:encode(table)
```

Encode lua table into json string.

### json:decode

```lua
table json:decode(string)
```

Decode json string into lua table.
