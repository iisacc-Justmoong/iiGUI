#!/usr/bin/env python3
"""Build a Qt-free, DEX-free LVUI NativeActivity APK and optionally open it."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import time
import zipfile

from verify_desktop import build_environment, checked_build_root

PACKAGE='com.iisacc.lvui.helloandroid'
TRIPLES={'arm64-v8a':'aarch64-linux-android','x86_64':'x86_64-linux-android'}
SYSTEM_LIBRARIES={'libandroid.so','libc.so','libdl.so','liblog.so','libm.so','libvulkan.so','libz.so','libjnigraphics.so'}


def devices_from_adb(text):
    rows=[]
    for line in text.splitlines():
        words=line.split()
        if len(words)>1 and words[1] in ('device','offline','unauthorized'):
            rows.append({'serial':words[0],'state':words[1],
                         'physical':not words[0].startswith('emulator-'),
                         'details':' '.join(words[2:])})
    return rows


def select_device(rows,serial,kind='physical'):
    if kind not in ('physical','emulator'):raise ValueError('Unsupported Android device kind')
    if kind=='emulator' and not serial:raise ValueError('Emulator execution requires an explicit --serial')
    choices=[r['serial'] for r in rows if r['state']=='device' and r['physical']==(kind=='physical') and (not serial or r['serial']==serial)]
    if len(choices)!=1:raise ValueError('Exactly one authorized '+kind+' device is required; specify its --serial')
    return choices[0]


def record_execution(report,kind,pid,screenshot):
    if kind not in ('physical','emulator'):raise ValueError('Unsupported Android device kind')
    report.update(launched=True,physicalDeviceTested=kind=='physical',emulatorDeviceTested=kind=='emulator',
                  pid=pid,screenshot=str(screenshot),
                  status='native_device_verified' if kind=='physical' else 'native_emulator_verified')


def device_log_start(text):
    value=text.strip()
    if not re.fullmatch(r'\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3}',value):
        raise ValueError('Android log start timestamp is invalid')
    return value


def make_manifest(source,output,ndk,compiler,abi,api):
    if abi not in TRIPLES or api<29:raise ValueError('This minimal native label host requires a supported 64-bit ABI and Android API 29+')
    data=json.loads((source/'examples/HelloWorld/iiSDKBuilder.json').read_text(encoding='utf-8'))
    data['app']={'name':'HelloAndroid','source':str(source/'examples/HelloAndroid'),'depends':['LVUI','iiGUI'],
                 'cache':{'BUILD_TESTING':'OFF'},'targets':['HelloAndroid']}
    data['sdkRoot']=str(source.parent);data['buildRoot']=str(output)
    for p in data['sdks']:p['source']=str(source.parent/p['name'])
    triple=TRIPLES[abi]
    prebuilt=ndk/'toolchains/llvm/prebuilt'
    host='darwin-x86_64' if sys.platform=='darwin' else 'windows-x86_64' if sys.platform=='win32' else 'linux-x86_64'
    sysroot=prebuilt/host/'sysroot'
    cache={'CMAKE_DISABLE_FIND_PACKAGE_Qt5':'ON','CMAKE_DISABLE_FIND_PACKAGE_Qt6':'ON',
           'CMAKE_DISABLE_FIND_PACKAGE_iiFileProviderNative':'ON','CMAKE_DISABLE_FIND_PACKAGE_iiFileProviderCoreIO':'ON',
           'CMAKE_POSITION_INDEPENDENT_CODE':'ON','CMAKE_EXPORT_COMPILE_COMMANDS':'ON',
           'CMAKE_FIND_ROOT_PATH_MODE_PACKAGE':'BOTH','ANDROID_STL':'c++_shared',
           'CMAKE_ANDROID_STL_TYPE':'c++_shared','LVUI_HOST_COMPILER':str(compiler),
           'Vulkan_INCLUDE_DIR':str(sysroot/'usr/include'),
           'Vulkan_LIBRARY':str(sysroot/'usr/lib'/triple/str(api)/'libvulkan.so')}
    data['kits']=[{'id':'android','platform':'android','androidAbi':abi,'androidApi':api,
                   'toolchain':str(ndk/'build/cmake/android.toolchain.cmake'),'cache':cache}]
    return data


def needed_libraries(text):
    match=re.search(r'NeededLibraries\s*\[([^]]*)\]',text,re.S)
    if not match:raise ValueError('ELF dependency table is missing')
    return [line.strip() for line in match[1].splitlines() if line.strip()]


def native_closure(roots,needed,find):
    pending=list(roots);result={}
    while pending:
        path=Path(pending.pop())
        if path.name in result:
            if path.resolve()!=result[path.name].resolve():raise ValueError('Conflicting native library name: '+path.name)
            continue
        result[path.name]=path
        for name in needed(path):
            if name in SYSTEM_LIBRARIES:continue
            dependency=find(name)
            if not dependency:raise ValueError('Missing APK native dependency: '+name)
            pending.append(dependency)
    return result


def check_apk(path,abi):
    with zipfile.ZipFile(path) as apk:
        names=apk.namelist();native='lib/'+abi+'/libhello_lvui.so'
        if len(names)!=len(set(names)):raise ValueError('Duplicate APK entries')
        for name in names:
            if re.search(r'\.dex$|\.lvui$|(?:^|/)libQt\d|(?:^|/)lvui-compile',name,re.I):
                raise ValueError('Unexpected interpreter/source/framework payload: '+name)
        for required in (native,'AndroidManifest.xml','assets/Pretendard-Regular.ttf','assets/Pretendard-OFL.txt'):
            if required not in names:raise ValueError('Missing native APK entry: '+required)
        native_hashes={}
        for name in names:
            if not name.startswith('lib/'):continue
            if not name.startswith('lib/'+abi+'/') or not name.endswith('.so'):raise ValueError('Unexpected native library path: '+name)
            image=apk.read(name)
            if len(image)<64 or image[:6]!=b'\x7fELF\x02\x01' or struct.unpack_from('<HH',image,16)!=(3,183 if abi=='arm64-v8a' else 62):
                raise ValueError('Wrong native shared-library ABI: '+name)
            native_hashes[name]=hashlib.sha256(image).hexdigest()
        return {'nativeLibrary':native,'nativeSha256':native_hashes[native],
                'libraries':native_hashes,'dexPresent':False,'lvuiSourcePackaged':False,'entries':len(names)}


def main(argv=None):
    source=Path(__file__).resolve().parents[1]
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk',type=Path,required=True)
    parser.add_argument('--ndk',type=Path,required=True)
    parser.add_argument('--jdk',type=Path,required=True)
    parser.add_argument('--host-compiler',type=Path,required=True)
    parser.add_argument('--builder',required=True)
    parser.add_argument('--abi',choices=tuple(TRIPLES),default='arm64-v8a')
    parser.add_argument('--api',type=int,default=29)
    parser.add_argument('--build-root',type=Path)
    parser.add_argument('--serial')
    parser.add_argument('--device-kind',choices=('physical','emulator'),default='physical')
    parser.add_argument('--install',action='store_true',help='Install and launch on the selected Android device kind and serial')
    parser.add_argument('--cache',action='append',default=[])
    args=parser.parse_args(argv)
    if args.install and args.device_kind=='emulator' and not args.serial:
        parser.error('--device-kind emulator requires --serial')
    output=checked_build_root(source,args.build_root or source/'build/android-hello')
    output.mkdir(parents=True,exist_ok=True)
    environment=build_environment(args.ndk)
    for key in ('CMAKE_PREFIX_PATH','CMAKE_INCLUDE_PATH','CMAKE_LIBRARY_PATH','CPPFLAGS','CFLAGS','CXXFLAGS','LDFLAGS','PKG_CONFIG_LIBDIR'):
        environment.pop(key,None)
    environment['JAVA_HOME']=str(args.jdk.resolve())
    environment['PATH']=str(args.jdk.resolve()/'bin')+os.pathsep+environment.get('PATH','')
    report={'package':PACKAGE,'abi':args.abi,'build':'not_run','installed':False,'launched':False,
            'deviceKind':args.device_kind,'physicalDeviceTested':False,'emulatorDeviceTested':False,'commands':[]}

    def run(label,command,binary=False):
        log=output/(label+('.png' if binary else '.log'))
        print(label+': '+str(log),flush=True)
        with log.open('wb') as stream:result=subprocess.run([str(x) for x in command],stdout=stream,stderr=subprocess.PIPE,env=environment)
        report['commands'].append({'step':label,'exitCode':result.returncode,'log':str(log)})
        if result.returncode:
            if result.stderr:(output/(label+'.stderr.log')).write_bytes(result.stderr)
            raise RuntimeError(label+' failed: '+result.stderr.decode(errors='replace')[-2000:])
        return log.read_bytes() if binary else log.read_text(encoding='utf-8',errors='replace')

    try:
        manifest=make_manifest(source,output/'builder',args.ndk.resolve(),args.host_compiler.resolve(),args.abi,args.api)
        for override in args.cache:
            key,sep,value=override.partition('=')
            if not key or not sep:raise ValueError('--cache requires KEY=VALUE')
            manifest['kits'][0]['cache'][key]=value
        (output/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
        run('builder',[args.builder,'build','--manifest',output/'manifest.json'])
        kit=output/'builder/kits/android/Release'
        native=kit/'app/build/libhello_lvui.so'
        host='darwin-x86_64' if sys.platform=='darwin' else 'windows-x86_64' if sys.platform=='win32' else 'linux-x86_64'
        llvm=args.ndk/'toolchains/llvm/prebuilt'/host/'bin'
        libraries=run('native-dependencies',[llvm/'llvm-readobj','--needed-libs','--dyn-symbols',native])
        symbols=run('native-symbols',[llvm/'llvm-nm','--demangle','--defined-only',native])
        if 'ANativeActivity_onCreate' not in libraries or 'ii::runtime::Application::exec(' not in symbols or 'ii::gui::Host::mount_widgets(' not in symbols:
            raise ValueError('Missing native activity, Core application or C++ widget linkage')
        if re.search(r'libQt|libpython|libjvm',libraries,re.I) or any(s in symbols for s in ['lvui::parse(','lvui::parse_view(','lvui::compile_native(']):
            raise ValueError('Framework/interpreter/compiler linked into APK')
        staging=output/'apk';assets=staging/'assets';library=staging/'lib'/args.abi
        assets.mkdir(parents=True,exist_ok=True);library.mkdir(parents=True,exist_ok=True)
        for name in ('Pretendard-Regular.ttf','Pretendard-OFL.txt'):
            shutil.copy2(kit/'prefix/share/iiGUI/resources/native'/name,assets/name)
        for folder in ('share/iiGUI/licenses','share/licenses/iiGUI'):
            dependency_licenses=kit/'prefix'/folder
            if dependency_licenses.exists():shutil.copytree(dependency_licenses,assets/'licenses/iiGUI',dirs_exist_ok=True)
        shutil.copy2(args.ndk/'sources/android/native_app_glue/NOTICE',assets/'licenses/NativeActivity-NOTICE')
        toolchain_notice=args.ndk/'toolchains/llvm/prebuilt'/host/'NOTICE'
        if toolchain_notice.exists():shutil.copy2(toolchain_notice,assets/'licenses/LLVM-runtime-NOTICE')
        native_sysroot=args.ndk/'toolchains/llvm/prebuilt'/host/'sysroot/usr/lib'/TRIPLES[args.abi]
        def find(name):
            if Path(name).name!=name:raise ValueError('Native dependency must be a soname, not a path')
            for directory in (kit/'prefix/lib',native.parent,native_sysroot):
                candidate=directory/name
                if candidate.is_file():return candidate
            return None
        def needed(path):
            text=run('dependencies-'+path.name,[llvm/'llvm-readobj','--needed-libs',path])
            if re.search(r'libQt|libpython|libjvm',text,re.I):raise ValueError('Unexpected native framework/interpreter dependency')
            return needed_libraries(text)
        closure=native_closure([native],needed,find)
        for path in list(library.glob('*.so')):
            if path.name not in closure:path.unlink()
        for name,path in closure.items():
            shutil.copy2(path,library/name)
            run('native-strip-'+name,[llvm/'llvm-strip','--strip-unneeded',library/name])
        report['nativeDependencyClosure']=list(closure)
        versions=sorted((args.sdk/'build-tools').iterdir(),key=lambda p:tuple(int(x) for x in p.name.split('.')))
        tools=versions[-1]
        unsigned=output/'hello-unsigned.apk'
        run('apk-link',[tools/'aapt2','link','-o',unsigned,'--manifest',source/'examples/HelloAndroid/AndroidManifest.xml',
                         '-I',args.sdk/'platforms/android-35/android.jar','-A',assets])
        with zipfile.ZipFile(unsigned,'a',compression=zipfile.ZIP_DEFLATED) as apk:
            for path in sorted(library.glob('*.so')):apk.write(path,'lib/'+args.abi+'/'+path.name)
        aligned=output/'hello-aligned.apk'
        run('apk-align',[tools/'zipalign','-f','-P','16','4',unsigned,aligned])
        keystore=output/'hello-debug.keystore'
        if not keystore.exists():
            run('debug-key',[args.jdk/'bin/keytool','-genkeypair','-keystore',keystore,'-storepass','android','-keypass','android',
                             '-alias','hello','-keyalg','RSA','-keysize','2048','-validity','3650','-dname','CN=iiSDK Native Hello'])
        apk=output/'iiSDK-LVUI-HelloWorld.apk'
        run('apk-sign',[tools/'apksigner','sign','--ks',keystore,'--ks-key-alias','hello','--ks-pass','pass:android','--key-pass','pass:android','--out',apk,aligned])
        run('apk-verify',[tools/'apksigner','verify','--verbose',apk])
        report.update(build='passed',apk=str(apk),apkSha256=hashlib.sha256(apk.read_bytes()).hexdigest(),native=check_apk(apk,args.abi),
                      lvuiSha256=hashlib.sha256((source/'examples/HelloAndroid/HelloWorld.lvui').read_bytes()).hexdigest())
        if args.install:
            adb=args.sdk/'platform-tools/adb'
            if not adb.exists():adb=Path(shutil.which('adb') or 'adb')
            serial=select_device(devices_from_adb(run('devices',[adb,'devices','-l'])),args.serial,args.device_kind)
            device=[adb,'-s',serial]
            emulated=run('device-emulator',device+['shell','getprop','ro.kernel.qemu']).strip()=='1'
            if emulated!=(args.device_kind=='emulator'):
                raise ValueError('Android runtime kind differs from --device-kind; refusing installation')
            abi=run('device-abi',device+['shell','getprop','ro.product.cpu.abi']).strip()
            if abi!=args.abi:raise ValueError('Device ABI differs from APK; rebuild with --abi '+abi)
            report['device']={'serial':serial,'kind':args.device_kind,'emulated':emulated,
                              'model':run('device-model',device+['shell','getprop','ro.product.model']).strip(),
                              'api':run('device-api',device+['shell','getprop','ro.build.version.sdk']).strip(),
                              'os':run('device-os',device+['shell','getprop','ro.build.version.release']).strip()}
            run('device-install',device+['install','-r',apk]);report['installed']=True
            run('device-stop-old-hello',device+['shell','am','force-stop',PACKAGE])
            start=device_log_start(run('device-log-start',device+['shell',"date '+%m-%d %H:%M:%S.000'"]))
            run('device-launch',device+['shell','am','start','-W','-n',PACKAGE+'/android.app.NativeActivity'])
            # This delay observes our one app without repeated launches or focus changes.
            time.sleep(4)
            log=run('device-log',device+['logcat','-d','-t',start,
                                       '-s','iiSDK-LVUI:I','iiGUI:E','AndroidRuntime:E','DEBUG:F'])
            if 'HELLO_ANDROID_READY' not in log or 'HELLO_ANDROID_TIMER_OK' not in log:
                raise ValueError('Native Hello World startup/timer evidence is missing; inspect device-log.log')
            pid=run('device-pid',device+['shell','pidof',PACKAGE]).strip()
            if not pid:raise ValueError('Hello World process is no longer running')
            run('device-screen',device+['exec-out','screencap','-p'],binary=True)
            record_execution(report,args.device_kind,pid,output/'device-screen.png')
        else:report['status']='apk_built_execution_not_requested'
        return 0
    except Exception as error:
        report.update(status='failed',error=str(error));print(str(error),file=sys.stderr);return 1
    finally:
        (output/'verification.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
        print('Report: '+str(output/'verification.json'),flush=True)


if __name__=='__main__':sys.exit(main())
