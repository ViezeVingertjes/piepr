# PlatformIO pre-build script for the native (PC) tests: AddressSanitizer +
# UndefinedBehaviorSanitizer, for compiling and linking (single definition).
Import("env")  # noqa: F821  (provided by PlatformIO)

SANITIZERS = ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"]
env.Append(CCFLAGS=SANITIZERS, LINKFLAGS=SANITIZERS)  # noqa: F821
