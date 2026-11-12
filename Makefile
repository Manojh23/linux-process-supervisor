CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Werror -pedantic
batch-runner: batch_runner.c
	$(CC) $(CFLAGS) $< -o $@
.PHONY: test clean
test: batch-runner
	python3 test_runner.py
clean:
	rm -f batch-runner
