/**
 * Demonstrate reading a buffer rfom in-memory encoded audio assets.
 * */
#include "synthizer.h"
#include "synthizer_constants.h"

#include <stdio.h>
#include <stdlib.h>

#define CHECKED(x) do { \
int ret = x; \
	if (ret) { \
		printf(#x ": Synthizer error code %i message %s\n", ret, syz_getLastErrorMessage());\
		ecode = 1; \
		goto end; \
	} \
} while(0)

int main(int argc, char *argv[]) {
	syz_Handle context = 0, generator = 0, source = 0, buffer = 0;
	/* Used by the CHECKED macro. */
	int ecode = 0;
	char *data = NULL;
	unsigned long long data_len = 0;
	FILE *file = NULL;

	if (argc != 2) {
		printf("Usage: buffer_from_memory <path>\n");
		return 1;
	}

	CHECKED(syz_configureLoggingBackend(SYZ_LOGGING_BACKEND_STDERR, NULL));
	syz_setLogLevel(SYZ_LOG_LEVEL_DEBUG);
	CHECKED(syz_initialize());

	CHECKED(syz_createContext(&context));
	CHECKED(syz_createBufferGenerator(&generator, context));
	CHECKED(syz_createDirectSource(&source, context));
	CHECKED(syz_sourceAddGenerator(source, generator));

	/**
	 * Read a file from disk, completely.
	 * */
	file = fopen(argv[1], "rb");
	if (file == NULL) {
		printf("Unable to open file\n");
		ecode = 1;
		goto end;
	}

	if (fseek(file, 0, SEEK_END) != 0 ||
		(data_len = ftell(file)) < 0 ||
		fseek(file, 0, SEEK_SET) != 0) {
		printf("Unable to compute file length\n");
		ecode = 1;
		goto end;
	}

	if (data_len == 0) {
		printf("No data in file\n");
		ecode = 1;
		goto end;
	}

	data = malloc(data_len);
	if (data == NULL) {
		printf("Unable to allocate data buffer\n");
		ecode = 1;
		goto end;
	}

	if (fread(data, 1, data_len, file) != data_len) {
		printf("Partial fread\n");
		ecode = 1;
		goto end;
	}

	CHECKED(syz_createBufferFromEncodedData(&buffer, data_len, data));
	CHECKED(syz_setO(generator, SYZ_P_BUFFER, buffer));

	printf("Press any key to quit...\n");
	getchar();

end:
	syz_handleDecRef(context);
	syz_handleDecRef(buffer);
	syz_handleDecRef(generator);
	syz_handleDecRef(buffer);
	free(data);
	if (file != NULL) fclose(file);
	return ecode;
}