local token = auto.process({ file = "ls", stdio = { "enable_stdout" } })

while token:running() do
    io.write(token:cout())
end
