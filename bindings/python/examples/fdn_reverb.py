"""Sets up a reverb.

Usage is fdn_reverb.py myfile.

The best way to get something out of this example is to read the manual for reverb properties, and tweak them in this file. Setting it up isn't the hard part, designing it is.
"""
import synthizer

import time
import random
import sys
import math

synthizer.configure_logging_backend(synthizer.LoggingBackend.STDERR)
synthizer.set_log_level(synthizer.LogLevel.DEBUG)

with synthizer.initialized():
    # Normal source setup from a CLI arg.
    ctx = synthizer.Context()
    gen = synthizer.BufferGenerator(ctx)
    buffer = synthizer.Buffer.from_stream_params("file", sys.argv[1])
    gen.buffer = buffer
    src = synthizer.Source3D(ctx)
    src.add_generator(gen)

    # create and connect the effect with a default gain of 1.0.
    reverb = synthizer.GlobalFdnReverb(ctx)
    ctx.config_route(src, reverb)
    time.sleep(5.0)
