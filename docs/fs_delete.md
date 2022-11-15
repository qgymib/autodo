# fs_delete

## SYNOPSIS

```lua
boolean auto.fs_delete(path, recursion)
```

## DESCRIPTION

Delete file or directory.

The optional parameter `recursion` is valid if `path` is a directory. If `recursion` is true, delete all contents in directory. If `recursion` is false, and directory is not empty, the delete operation will fail.

## RETURN VALUE

Boolean. `true` if success, `false` if there is error in operation.
