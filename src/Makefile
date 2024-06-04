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

CFLAGS:= -std=c11 -pedantic -pedantic-errors -g -Wall  -Wextra -D_POSIX_C_SOURCE=200112L -fsanitize=address #-Werror
SMTPD_CLI:= smtpd
SMTPD_OBJS:= args.o netutils.o parser.o stm.o selector.o parser_utils.o buffer.o main.o

.PHONY: all clean

all: $(SMTPD_CLI)

$(SMTPD_CLI): $(SMTPD_OBJS)
	$(CC) $(CFLAGS) $(SMTPD_OBJS) -o $(SMTPD_CLI)

clean:
	- rm -rf $(SMTPD_CLI) $(SMTPD_OBJS)
