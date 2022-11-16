# json

## SYNOPSIS

```lua
json auto.json()
```

## DESCRIPTION

Create a json parser for encode and decode json string.

## RETURN VALUE

A token for encode and decode json string.

### json:compare

```lua
boolean json:compare(json1, json2, case_sensitive)
```

Compare two json string. Return `true` if they are equal, otherwise return false.

The third parameter `case_sensitive` is optional, by defualt it is set to `true`.

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
