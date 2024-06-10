#########
# FLAGS #
#########
# -Wall 			: Enable all warnings
# -Wextra 			: Extra warnings
# -Werror 			: Treat all warnings as errors
# -g				: Add debugging symbols
# -fsanitize=address 		: Address sanitizer (libasan)
# -std=c11			: Use C11
# -D_POSIX_C_SOURCE=200112L 	: Posix version

# CFLAGS:= -std=c11 -pedantic -pedantic-errors -g -Wall  -Wextra -D_POSIX_C_SOURCE=200112L -fsanitize=address #-Werror
# SMTPD_CLI:= smtpd
# SMTPD_OBJS:= args.o netutils.o parser.o stm.o selector.o parser_utils.o buffer.o main.o

# .PHONY: all clean

# all: $(SMTPD_CLI)

# $(SMTPD_CLI): $(SMTPD_OBJS)
# 	$(CC) $(CFLAGS) $(SMTPD_OBJS) -o $(SMTPD_CLI)

# clean:
# 	- rm -rf $(SMTPD_CLI) $(SMTPD_OBJS)


# CFLAGS:= -std=c11 -pedantic -pedantic-errors -g -Wall  -Wextra -D_POSIX_C_SOURCE=200112L -fsanitize=address #-Werror
# SMTPD_CLI:= smtpd
# SMTPD_OBJS:= build/args.o build/netutils.o build/parser.o build/stm.o build/selector.o build/parser_utils.o build/buffer.o build/main.o

# .PHONY: all clean

# all: $(SMTPD_CLI)

# $(SMTPD_CLI): $(SMTPD_OBJS)
# 	$(CC) $(CFLAGS) $(SMTPD_OBJS) -o $(SMTPD_CLI)

# clean:
# 	- rm -rf $(SMTPD_CLI) $(SMTPD_OBJS)

# build/%.o: %.c
# 	mkdir -p build
# 	$(CC) -c $(CFLAGS) $< -o $@

CFLAGS:= -std=c11 -pedantic -pedantic-errors -g -Wall  -Wextra -D_POSIX_C_SOURCE=200112L -Werror -fsanitize=address -Ilib/headers -Itest/headers 
SMTPD_CLI:= smtpd
LIB_OBJS:= build/args.o build/netutils.o build/parser.o build/stm.o build/selector.o build/parser_utils.o build/buffer.o build/smtp.o build/request.o
MAIN_OBJ:= build/main.o
TEST_OBJS:= build/hello_test.o build/netutils_test.o build/parser_test.o build/parser_utils_test.o build/request.o
 
.PHONY: all clean test

all: $(SMTPD_CLI)

$(SMTPD_CLI): $(LIB_OBJS) $(MAIN_OBJ)
	$(CC) $(CFLAGS) $(LIB_OBJS) $(MAIN_OBJ) -o $(SMTPD_CLI)

test: $(TEST_OBJS)
	$(CC) $(CFLAGS) $(LIB_OBJS) $(TEST_OBJS) -o test_runner

clean:
	- rm -rf $(SMTPD_CLI) build/*.o test_runner

build/%.o: lib/%.c
	mkdir -p build
	$(CC) -c $(CFLAGS) $< -o $@

build/main.o: main.c
	mkdir -p build
	$(CC) -c $(CFLAGS) $< -o $@

build/%_test.o: test/%_test.c
	mkdir -p build
	$(CC) -c $(CFLAGS) $< -o $@