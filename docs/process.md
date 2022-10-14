# process

## SYNOPSIS

```lua
process auto.process(table config)
```

## DESCRIPTION

Create a process.

```
{
    "file": "file",
    "cwd": "working directory",
    "args": ["file", "--command", "--list"],
    "envs": ["environment", "list"],
    "stdio": ["enable_stdin", "enable_stdout", "enable_stderr"],
}
```

## RETURN VALUE

A token for interactive with process.

### process:kill

```lua
process:kill(int signum)
```

Kill process with `signum` signal. If the process is already exited, then noting happen.

### process:cin

```lua
int process:cin(string data)
```

Send `data` to process's stdin.

Return the number of bytes written. If the return value not match the size of data, it means something bad happen.

### process:cout

```lua
string process:cout()
```

Get output from process's stdout.

Return the content of stdout. If nothing returned, either process exited or stdout is not opened.

### process:cerr

```lua
string process:cerr()
```

Like `process:cout()`, but get the content of stderr.

### process:running

```lua
bool process:running()
```

Return whether the process is running.
