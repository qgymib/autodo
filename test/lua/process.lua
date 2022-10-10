local token = auto.process({ path = "ls", stdio = { "enable_stdout" } })
io.write(token:cout())
