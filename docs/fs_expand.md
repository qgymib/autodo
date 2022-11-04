# fs_expand

## SYNOPSIS

```lua
string auto.fs_expand(string template)
```

## DESCRIPTION

Get expanded path for a given path tempalte.

+ `$AUTO_SCRIPT_FILE`: Full path to run script
+ `$AUTO_SCRIPT_PATH`: Path to script without file name
+ `$AUTO_SCRIPT_NAME`: Script name without path
+ `$AUTO_CWD`: Current working directory
+ `$AUTO_EXE_PATH`: The autodo executable path

## RETURN VALUE

The expanded path.
