#ifndef __LOG_H__
#define __LOG_H__

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <syslog.h>
#include <cstdarg>

static int level = LOG_INFO;
static int LOG_BUFFER_SIZE = 2048;
static const char* logLevels[] = {
	"Emerge!",
	"Alert!",
	"Critical!",
	"Error!",
	"Warn!",
	"Notice:",
	"Info:",
	"Debug:",
	"\0"
};

void setLogLevel(int logLevel = LOG_INFO);
void log(int logLevel, const char* fileName, int lineNum, const char* format, ...);

#endif
