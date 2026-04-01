"""
PlatformIO pre-build script: auto-detect libmicroros.a and enable UROS_ENABLED.
"""
import os
Import("env")

lib_path = os.path.join(env.subst("$PROJECT_DIR"), "lib", "microros", "libmicroros.a")

if os.path.isfile(lib_path):
    env.Append(CPPDEFINES=["UROS_ENABLED"])
    env.Append(LIBPATH=[os.path.dirname(lib_path)])
    env.Append(LIBS=["microros"])
    env.Append(CPPPATH=[os.path.join(os.path.dirname(lib_path), "include")])
    print("*** microROS: ENABLED (libmicroros.a found) ***")
else:
    print("*** microROS: DISABLED (libmicroros.a not found) ***")
