CC ?= gcc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror
INCLUDES = -Iinclude
SOURCES = src/switch.c
.PHONY: all test run clean sanitize
all: bin/switch-demo bin/switch-tests
bin:
	mkdir -p bin
bin/switch-demo: src/main.c $(SOURCES) include/switch.h | bin
	$(CC) $(CFLAGS) $(INCLUDES) src/main.c $(SOURCES) -o $@
bin/switch-tests: tests/test_switch.c $(SOURCES) include/switch.h | bin
	$(CC) $(CFLAGS) $(INCLUDES) tests/test_switch.c $(SOURCES) -o $@
test: bin/switch-tests
	./bin/switch-tests
run: bin/switch-demo
	./bin/switch-demo
sanitize:
	$(CC) -std=c11 -g -Wall -Wextra -Wpedantic -fsanitize=address,undefined -fno-omit-frame-pointer $(INCLUDES) tests/test_switch.c $(SOURCES) -o bin/switch-tests-sanitized
	./bin/switch-tests-sanitized
clean:
	rm -rf bin
