#ifndef _LOGGER_H_
#define _LOGGER_H_

// The logger makes copies of any data it needs from pointer parameters in the functions
// described in this file. aka "Don't worry about the memory lifecycle of pointer parameters".

// Define this to fully disable all loggin on compilation.
// #define DISABLE_LOGGER

#include "selector.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

typedef enum
{
	LOG_DEBUG = 0,
	LOG_INFO,
	LOG_OUTPUT,
	LOG_WARNING,
	LOG_ERROR,
	LOG_FATAL
} log_level_t;

#define MIN_LOG_LEVEL LOG_DEBUG
#define MAX_LOG_LEVEL LOG_FATAL

const char* logger_get_level_string(log_level_t level);

#ifdef DISABLE_LOGGER
#define logger_init(selector, logFile, logStream)
#define logger_finalize()
#define logger_set_level(level)
#define logger_is_enabled_for(level) 0
#define logf(level, format, ...)
#define log(level, s)
#else
/**
 * @brief Initializes the logging system. Not calling this function will result is the
 * server running with logging disabled.
 * @param selector The selector to use. This is required as logging is typically buffered,
 * and to make writes non-blocking writing can only occur when the file descriptor is
 * available.
 * @param file A file where logs are saved. Set to NULL to disable saving logs to a file,
 * or set to an empty string "" to use a default file name appended by the current date.
 * @param log_stream A stream where logs are saved. Typically set to stdout to print logs
 * to the console. Set to NULL to disable. WARNING: Printing to this stream is done with
 * fprintf which may be blocking, halting the server. This stream is not closed by the
 * logging system.
 */
int logger_init(fd_selector selector_param, const char* log_file, FILE* log_stream_param);

/**
 * @brief Closes the logging system, flushing any remaining logs, closing any opened
 * files and unregistering them from the selector.
 */
int logger_finalize();

void logger_set_level(log_level_t level);

int logger_is_enabled_for(log_level_t level);

void logger_pre_print();

void logger_get_buf_start_and_max_length(char** bufstart_var, size_t* maxlen_var);

int logger_post_print(int written, size_t maxlen);

#define logf(level, format, ...)                                                                      \
	if (logger_is_enabled_for(level)) {                                                               \
		logger_pre_print();                                                                           \
		time_t loginternal_time = time(NULL);                                                         \
		struct tm loginternal_tm = *localtime(&loginternal_time);                                     \
		size_t loginternal_maxlen;                                                                    \
		char* loginternal_bufstart;                                                                   \
		logger_get_buf_start_and_max_length(&loginternal_bufstart, &loginternal_maxlen);              \
		int loginternal_written = snprintf(loginternal_bufstart,                                      \
		                                   loginternal_maxlen,                                        \
		                                   "%04d-%02d-%02dT%02d:%02d:%02d%s\t" format "\n",           \
		                                   loginternal_tm.tm_year + 1900,                             \
		                                   loginternal_tm.tm_mon + 1,                                 \
		                                   loginternal_tm.tm_mday,                                    \
		                                   loginternal_tm.tm_hour,                                    \
		                                   loginternal_tm.tm_min,                                     \
		                                   loginternal_tm.tm_sec,                                     \
		                                   level == LOG_OUTPUT ? "" : logger_get_level_string(level), \
		                                   ##__VA_ARGS__);                                            \
		logger_post_print(loginternal_written, loginternal_maxlen);                                   \
	}

#define log(level, s) logf(level, "%s", s)

#endif
#endif