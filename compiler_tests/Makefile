# Compiler tests
IFX=ifx
SOURCE_FILE=compiler_tests.f90
OUTPUT_FILE=compiler_tests
COMPILER_FLAGS=
COMPILER_LDFLAGS=

.PHONY: all clean

all:
	$(IFX) $(SOURCE_FILE) $(COMPILER_FLAGS) -o $(OUTPUT_FILE) $(COMPILER_LDFLAGS)

clean:
	@rm -f $(OUTPUT_FILE)
