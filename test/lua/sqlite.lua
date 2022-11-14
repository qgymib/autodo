local db = auto.sqlite()
assert(db ~= nil)

local csv_file_path = os.getenv("PROJECT_SOURCE_DIR") .. "/third_party/CsvParser/examples/example_file_with_header.csv"

db:from_csv_file("a_test_table", csv_file_path)
io.write(db:to_csv("a_test_table") .. "\n")
