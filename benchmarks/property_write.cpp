#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <algorithm>
#include <chrono>
#include <memory>
#include <thread>

#include <cstddef>
#include <cmath>

#include "synthizer.h"
#include "synthizer_constants.h"

#define PI 3.1415926535
#define CHECKED(x) do { \
auto ret = x; \
	if (ret) { \
		printf(#x ": Synthizer error code %i message %s\n", ret, syz_getLastErrorMessage());\
		ecode = 1; \
		if (ending == 0) goto end; \
		else goto end2; \
	} \
} while(0)

int main(int argc, char *argv[]) {
	syz_Handle context = 0, source = 0;
	int ecode = 0, ending = 0;
	unsigned int iterations = 100000;
	std::chrono::high_resolution_clock::time_point t_start, t_end;
	std::chrono::high_resolution_clock clock;


	CHECKED(syz_configureLoggingBackend(SYZ_LOGGING_BACKEND_STDERR, nullptr));
	syz_setLogLevel(SYZ_LOG_LEVEL_DEBUG);
	CHECKED(syz_initialize());

	CHECKED(syz_createContext(&context));
	CHECKED(syz_createSource3D(&source, context));

	t_start = clock.now();
	for(unsigned int i = 0; i < iterations; i++) {
		CHECKED(syz_setD(source, SYZ_P_GAIN, 1.0));
	}
	t_end = clock.now();

	{
		auto dur = t_end - t_start;
		auto tmp = std::chrono::duration<double>(dur);
		double secs = tmp.count();
		printf("Took %f seconds total\n", secs);
		printf("%f per write\n", secs / iterations);
		printf("Estimated %f per second\n", iterations / secs);
	}

end:
	ending = 1;
	CHECKED(syz_handleFree(source));
	CHECKED(syz_handleFree(context));
end2:
	CHECKED(syz_shutdown());
	return ecode;
}
