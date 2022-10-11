local token = auto.process({ path = "ls", stdio = { "enable_stdout" } })

while token:running() do
    io.write(token:cout())
end
