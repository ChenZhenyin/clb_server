#include "log.h"

void setLogLevel(int logLevel) {

	level = logLevel;
}

void log(int logLevel, const char* fileName, int lineNum, const char* format, ...) {

	if (logLevel > level) {
		return;
	}

	time_t tmp = time(NULL);
	struct tm* curTime = localtime(&tmp);
	if (!curTime) {
		return;
	}

	char logBuffer[LOG_BUFFER_SIZE];
	memset(logBuffer, '\0', LOG_BUFFER_SIZE);
	strftime(logBuffer, LOG_BUFFER_SIZE - 1, "[%x %X]", curTime);
	printf("%s", logBuffer);
	printf("%s:%04d ", fileName, lineNum);
	printf("%s", logLevels[logLevel - LOG_EMERG]);

	va_list argList;
	va_start(argList, format);
	memset(logBuffer, '\0', LOG_BUFFER_SIZE);
	vsnprintf(logBuffer, LOG_BUFFER_SIZE - 1, format, argList);
	printf("%s\n", logBuffer);
	fflush(stdout);
	va_end(argList);
}
