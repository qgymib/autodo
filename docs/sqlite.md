# sqlite

## SYNOPSIS

```lua
sqlite auto.sqlite(options)
```

## DESCRIPTION

Create a sqlite instance.

The 1st parameter is options for create sqlite instance:

+ "filename": Database filename. If the filename is ":memory:", then a private, temporary in-memory database is created for the connection. This in-memory database will vanish when the database connection is closed. If the filename is an empty string, then a private, temporary on-disk database will be created. This private database will be automatically deleted as soon as the database connection is closed.

## RETURN VALUE

A token for futher processing.

### sqlite:close

```lua
sqlite:close()
```

Close database connection.

You are not necessary to call this function, as the connection will be closed when sqlite object is released by Lua VM GC.

### sqlite:exec

```lua
sqlite:exec(sql)
```

Execute SQL statements. Checkout [SQL As Understood By SQLite](https://www.sqlite.org/lang.html).

If sql execute failes, it raise an error with error information on top of stack.

### sqlite:from_csv

```lua
sqlite:from_csv(table_name, data)
```

Import CSV data into SQL table.

### sqlite:from_csv_file

```lua
sqlite:from_csv_file(table_name, file_path)
```

Import CSV file into SQL table.

### sqlite:to_csv

```lua
string sqlite:to_csv(table_name)
```

Export SQL table into CSV string.

### sqlite:to_csv_file

```lua
sqlite:to_csv_file(table_name, file_path, mode)
```

Export SQL table into CSV file.

The `mode` is a optional string for specific how to open `file_path`:

+ "a": (Default) Open for appending (writing at end of file). The file is created if it does not exist.
+ "w": Truncate file to zero length or create text file for writing.
