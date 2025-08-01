# LCC Compiler Module

# Run static analysis on LCC source code
lint:
	find src/ -name "*.c" | xargs -I {} cppcheck --enable=warning,style --std=c99 --error-exitcode=1 \
		--suppress=unusedFunction --suppress=missingIncludeSystem \
		--suppress=variableScope --suppress=unreadVariable \
		{}

# Build LCC compiler with Miniforth backend
build:
	make

# Run LCC test suite
test: build
	#!/usr/bin/env bash
	cd tst
	for test in *.c; do
		name=$(basename "$test" .c)
		echo "Testing $name.c -> $name.mf"
		../build/rcc -target=miniforth < "$test" > "$name.mf" 2>&1 || echo "  Failed: $name.c"
	done
	echo "Checking for differences from committed versions..."
	if git diff --name-only *.mf 2>/dev/null | grep -q .; then
		echo "  Changed files:"
		git diff --name-only *.mf | sed 's/^/    /'
	else
		echo "  No differences found"
	fi

# Clean LCC build artifacts
clean:
	-make clean
