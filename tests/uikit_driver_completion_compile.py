"""Compile the UIKit adapter against a current and an intentionally old Core header."""
from pathlib import Path
import argparse
import os
import subprocess
import tempfile


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--build",type=Path,required=True)
    parser.add_argument("--core-includes",required=True)
    args=parser.parse_args()
    root=Path(__file__).resolve().parents[1]
    includes=[Path(value) for value in args.core_includes.split("|") if value]
    header=next((path/"iiSDKCore/Runtime/Application.h" for path in includes
                 if (path/"iiSDKCore/Runtime/Application.h").is_file()),None)
    if header is None:
        raise RuntimeError("Core application header was not supplied")
    env=os.environ.copy()
    for key in list(env):
        if key in {"CPATH","CPLUS_INCLUDE_PATH","C_INCLUDE_PATH","LIBRARY_PATH",
                   "CMAKE_PREFIX_PATH","CMAKE_INCLUDE_PATH","CMAKE_LIBRARY_PATH"} or key.startswith(
                       ("QT_","QML","LVRS_","DYLD_","LD_")):
            env.pop(key,None)
    sdk=subprocess.check_output(["xcrun","--sdk","iphoneos","--show-sdk-path"],text=True,env=env).strip()
    clang=subprocess.check_output(["xcrun","--sdk","iphoneos","--find","clang++"],text=True,env=env).strip()
    with tempfile.TemporaryDirectory(prefix=".uikit-driver-completion-",dir=root/"resources") as resource, \
         tempfile.TemporaryDirectory(prefix="uikit-driver-completion-",dir=args.build) as output:
        overlay=Path(resource)/"iiSDKCore/Runtime/Application.h"
        overlay.parent.mkdir(parents=True)
        overlay.write_text(f'#include "{header.as_posix()}"\n#undef IISDKCORE_APPLICATION_DRIVER_STOPPED_API\n')
        command=[clang,"-std=c++23","-Wall","-Wextra","-Wpedantic","-Werror",
                 "-target","arm64-apple-ios18.0","-isysroot",sdk,
                 "-I",str(args.build/"include")]
        for path in includes:
            command.extend(("-I",str(path)))
        source=str(root/"src/Core/Application.cpp")
        obj=Path(output)/"Application.o"
        subprocess.run([*command,"-c",source,"-o",str(obj)],text=True,env=env,check=True)
        symbols=subprocess.check_output(["xcrun","nm","-u",str(obj)],text=True,env=env)
        if "driver_stopped" not in symbols:
            raise RuntimeError("UIKit adapter did not reference the Core completion API")
        old=subprocess.run([*command[:1],"-I",resource,*command[1:],"-fsyntax-only",source],
                           text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,env=env)
        if old.returncode==0 or "UIKit application lifecycle requires iiSDKCore driver completion support" not in old.stdout:
            raise RuntimeError("A Core header without driver completion was not rejected: "+old.stdout)
        host=Path(output)/"HostIOS.o"
        subprocess.run([*command,"-fobjc-arc","-c",str(root/"src/Platform/HostIOS.mm"),"-o",str(host)],
                       text=True,env=env,check=True)
        host_symbols=subprocess.check_output(["xcrun","nm",str(host)],text=True,env=env)
        for symbol in ("UIKitWindowGeometry", "UIKitSceneRouter"):
            if symbol not in host_symbols:
                raise RuntimeError("UIKit host did not compile " + symbol)
    print("UIKit adapter and host compiled for iPhoneOS; old Core rejected; no app executed")


if __name__=="__main__":
    main()
