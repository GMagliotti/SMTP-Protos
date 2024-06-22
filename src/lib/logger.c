#include "logger.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define DEFAULT_LOG_FOLDER         "./log"
#define DEFAULT_LOG_FILE           (DEFAULT_LOG_FOLDER "/%02d-%02d-%04d.log")
#define DEFAULT_LOG_FILE_MAXSTRLEN 48

/** The minimum allowed length for the log writing buffer. */
#define LOG_MIN_BUFFER_SIZE 0x1000  // 4 KBs
/** The maximum allowed length for the log writing buffer. */
#define LOG_MAX_BUFFER_SIZE 0x400000  // 4 MBs
/** The amount of bytes to expand the log buffer by when expanding. */
#define LOG_BUFFER_SIZE_GRANULARITY 0x1000  // 4 KBs
/** The maximum length a single print into the log buffer SHOULD require. */
#define LOG_BUFFER_MAX_PRINT_LENGTH 0x200  // 512 bytes

#define LOG_FILE_PERMISSION_BITS   666
#define LOG_FOLDER_PERMISSION_BITS 666
#define LOG_FILE_OPEN_FLAGS        (O_WRONLY | O_APPEND | O_CREAT | O_NONBLOCK)

const char*
logger_get_level_string(TLogLevel level)
{
	switch (level) {
		case LOG_DEBUG:
			return " [DEBUG]";
		case LOG_INFO:
			return " [INFO]";
		case LOG_OUTPUT:
			return " [OUTPUT]";
		case LOG_WARNING:
			return " [WARNING]";
		case LOG_ERROR:
			return " [ERROR]";
		case LOG_FATAL:
			return " [FATAL]";
		default:
			return " [UNKNOWN]";
	}
}

#ifndef DISABLE_LOGGER

/** The buffer where logs are buffered. */
static char* buffer = NULL;
static size_t buffer_start = 0, buffer_length = 0, buffer_capacity = 0;

/** The file descriptor for writing logs to disk, or -1 if we're not doing that. */
static int log_file_fd = -1;
static fd_selector selector = NULL;
static TLogLevel log_level = MIN_LOG_LEVEL;

/** The stream for writing logs to, or NULL if we're not doing that. */
static FILE* log_stream = NULL;

/**
 * @brief Attempts to make at least len bytes available in the log buffer.
 */
static inline void
make_buffer_space(size_t len)
{
	// Make enough space in the buffer for the string
	if (buffer_length + buffer_start + len > buffer_capacity) {
		// If the buffer can be compacted to fit this string, do so. Otherwise,
		// we'll have to allocate more memory.
		if (buffer_capacity <= len) {
			memmove(buffer, buffer + buffer_start, buffer_length);
			buffer_start = 0;
		} else if (buffer_capacity < LOG_MAX_BUFFER_SIZE) {
			size_t new_buffer_capacity = buffer_length + len;
			new_buffer_capacity = (new_buffer_capacity + LOG_BUFFER_SIZE_GRANULARITY - 1) /
			                      LOG_BUFFER_SIZE_GRANULARITY * LOG_BUFFER_SIZE_GRANULARITY;
			if (new_buffer_capacity > LOG_MAX_BUFFER_SIZE)
				new_buffer_capacity = LOG_MAX_BUFFER_SIZE;

			// The buffer isn't large enough, let's try to expand it, or at
			// least compact it to make as much space available as possible.
			void* new_buffer = malloc(new_buffer_capacity);
			if (new_buffer == NULL) {
				memmove(buffer, buffer + buffer_start, buffer_length);
				buffer_start = 0;
			} else {
				memcpy(new_buffer, buffer + buffer_start, buffer_length);
				free(buffer);
				buffer = new_buffer;
				buffer_capacity = new_buffer_capacity;
				buffer_start = 0;
			}
		}
	}
}

/**
 * @brief Attempts to flush as much of the logging buffer into the logging file as it can.
 * This is performed with nonblocking writes. If any bytes are left for writing, we tell the
 * selector to notify us when writing is available and retry once that happens.
 */
static inline void
try_flush_buffer_to_file()
{
	// Try to write everything we have in the buffer. This is nonblocking, so any
	// (or all) remaining bytes will be saved in the buffer and retried later.
	ssize_t written = write(log_file_fd, buffer + buffer_start, buffer_length);
	if (written > 0) {
		buffer_length -= written;
		buffer_start = (buffer_length == 0 ? 0 : (buffer_start + written));
	}

	// If there are still remaining bytes to write, leave them in the buffer and retry
	// once the selector says the fd can be written.
	selector_set_interest(selector, log_file_fd, buffer_length > 0 ? OP_WRITE : OP_NOOP);
}

static void
fd_write_handler(selector_key* key)
{
	// TODO: TEMP FOR BUILD
	key = key;
	// END TEMP FOR BUILD
	try_flush_buffer_to_file();
}

static void
fd_close_handler(selector_key* key)
{
	// TODO: TEMP FOR BUILD
	key = key;
	// END TEMP FOR BUILD
	// We will attempt to flush the remaining bytes to the log file and then close it.

	if (buffer_length != 0) {
		// Set the log file to blocking, then try to write the remaining bytes. If any of
		// this fails, just ignore the failure.
		int flags = fcntl(log_file_fd, F_GETFD, 0);
		fcntl(log_file_fd, F_SETFL, flags & (~O_NONBLOCK));
		ssize_t written = write(log_file_fd, buffer, buffer_length);
		if (written > 0) {
			buffer_length -= written;
			buffer_start = (buffer_length == 0 ? 0 : (buffer_start + written));
		}
	}

	close(log_file_fd);
	log_file_fd = -1;
}

static fd_handler fd_handler_instance = { .handle_read = NULL,
	                                      .handle_write = fd_write_handler,
	                                      .handle_close = fd_close_handler,
	                                      .handle_block = NULL };

/**
 * @brief Attempts to open a file for logging. Returns the fd, or -1 if failed.
 */
static int
try_open_log_file(const char* log_file, struct tm tm)
{
	if (log_file == NULL)
		return -1;

	char logfilebuf[DEFAULT_LOG_FILE_MAXSTRLEN + 1];

	// If log_file is "", then we instead of the default log file name.
	if (log_file[0] == '\0') {
		snprintf(
		    logfilebuf, DEFAULT_LOG_FILE_MAXSTRLEN, DEFAULT_LOG_FILE, tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900);
		log_file = logfilebuf;

		// If the default log folder isn't created, create it.
		mkdir(DEFAULT_LOG_FOLDER, LOG_FOLDER_PERMISSION_BITS);
	}

	// Warning: If a custom log_file is specified and it is within uncreated folders, this open will fail.
	int fd = open(log_file, LOG_FILE_OPEN_FLAGS, LOG_FILE_PERMISSION_BITS);
	if (fd < 0) {
		fprintf(stderr,
		        "WARNING: Failed to open logging file at %s. The server will still run, but with logging disabled.\n",
		        log_file);
		return -1;
	}

	return fd;
}

int
logger_init(fd_selector selector_param, const char* log_file, FILE* log_stream_param)
{
	// Get the local time (to log when the server started)
	time_t time_now = time(NULL);
	struct tm tm = *localtime(&time_now);

	selector = selector_param;
	log_file_fd = selector_param == NULL ? -1 : try_open_log_file(log_file, tm);
	log_stream = log_stream_param;
	log_level = MIN_LOG_LEVEL;

	// If we opened a file for writing logs, register it in the selector.
	if (log_file_fd >= 0)
		selector_register(selector, log_file_fd, &fd_handler_instance, OP_NOOP, NULL);

	// If we have any form of logging enabled, allocate a buffer for logging.
	if (log_file_fd >= 0 || log_stream != NULL) {
		buffer = malloc(LOG_MIN_BUFFER_SIZE);
		buffer_capacity = LOG_MIN_BUFFER_SIZE;
		buffer_length = 0;
		buffer_start = 0;
		if (buffer == NULL) {
			close(log_file_fd);
			log_file_fd = -1;
			fprintf(stderr, "WARNING: Failed to malloc a buffer for logging. You don't have 4KBs?\n");
			return -1;
		}
	}

	return 0;
}

int
logger_finalize()
{
	// If a logging file is opened, flush buffers, unregister it, and close it.
	if (log_file_fd >= 0) {
		selector_unregister_fd(selector,
		                       log_file_fd);  // This will also call the TFdHandler's close, and close the file.
		selector = NULL;
	}

	// If we allocated a buffer, free it.
	if (buffer != NULL) {
		free(buffer);
		buffer = NULL;
		buffer_capacity = 0;
		buffer_length = 0;
		buffer_start = 0;
	}

	// The logger does not handle closing the stream. We set it to NULL and forget.
	log_stream = NULL;
	return 0;
}

void
logger_set_level(TLogLevel level)
{
	log_level = level;
}

int
logger_is_enabled_for(TLogLevel level)
{
	return level >= log_level && (log_file_fd > 0 || log_stream != NULL);
}

void
logger_pre_print()
{
	make_buffer_space(LOG_BUFFER_MAX_PRINT_LENGTH);
}

void
logger_get_buf_start_and_max_length(char** bufstart_var, size_t* maxlen_var)
{
	*maxlen_var = buffer_capacity - buffer_length - buffer_start;
	*bufstart_var = buffer + buffer_start + buffer_length;
}

/**
 * @brief Called by the logf macro to perform error checking and log flushing.
 */
int
logger_post_print(int written, size_t maxlen)
{
	if (written < 0) {
		fprintf(stderr, "Error: snprintf(): %s\n", strerror(errno));
		return -1;
	}

	if ((size_t)written >= maxlen) {
		fprintf(stderr, "Error: %lu bytes of logs possibly lost. Slow disk?\n", written - maxlen + 1);
		written = maxlen - 1;
	}

	if (log_stream != NULL) {
		fprintf(log_stream, "%s", buffer + buffer_start + buffer_length);
	}

	// If there's no output file, then we printed the results to the stream but don't
	// update buffer_length because we're not saving anything in the buffer.
	if (log_file_fd >= 0) {
		buffer_length += written;
		try_flush_buffer_to_file();
	}
	return 0;
}

#endif  // #ifndef DISABLE_LOGGER