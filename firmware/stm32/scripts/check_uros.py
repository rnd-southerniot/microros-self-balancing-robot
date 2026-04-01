"""
PlatformIO pre-build script:
  1. Force hard-float FPU flags (linker flags too, not just compiler)
  2. Auto-detect libmicroros.a and enable UROS_ENABLED
"""
import os
Import("env")

# Force hard-float on STM32F429 (has FPv4-SP) for both compile and link
fpu_flags = ["-mfloat-abi=hard", "-mfpu=fpv4-sp-d16"]
env.Append(CCFLAGS=fpu_flags)
env.Append(LINKFLAGS=fpu_flags)

lib_path = os.path.join(env.subst("$PROJECT_DIR"), "lib", "microros", "libmicroros.a")

if os.path.isfile(lib_path):
    env.Append(CPPDEFINES=["UROS_ENABLED"])
    env.Append(LIBPATH=[os.path.dirname(lib_path)])
    env.Append(LIBS=["microros"])
    env.Append(CPPPATH=[os.path.join(os.path.dirname(lib_path), "include")])
    print("*** microROS: ENABLED (libmicroros.a found) ***")
else:
    print("*** microROS: DISABLED (libmicroros.a not found) ***")
