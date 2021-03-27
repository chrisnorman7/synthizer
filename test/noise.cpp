#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <algorithm>
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
	syz_Handle context = 0, generator = 0, source = 0;
	int ecode = 0, ending = 0;

	CHECKED(syz_configureLoggingBackend(SYZ_LOGGING_BACKEND_STDERR, nullptr));
	syz_setLogLevel(SYZ_LOG_LEVEL_DEBUG);
	CHECKED(syz_initialize());

	CHECKED(syz_createContext(&context));
	CHECKED(syz_createDirectSource(&source, context));
	CHECKED(syz_createNoiseGenerator(&generator, context, 2));
	CHECKED(syz_sourceAddGenerator(source, generator));

	for (unsigned int i = 0; i < SYZ_NOISE_TYPE_COUNT; i++) {
		CHECKED(syz_setI(generator, SYZ_P_NOISE_TYPE, i));
		std::this_thread::sleep_for(std::chrono::seconds(5));
	}

end:
	ending = 1;
	CHECKED(syz_handleDecRef(source));
	CHECKED(syz_handleDecRef(generator));
	CHECKED(syz_handleDecRef(context));
end2:
	CHECKED(syz_shutdown());
	return ecode;
}
