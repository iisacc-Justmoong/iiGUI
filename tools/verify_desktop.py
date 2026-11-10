#!/usr/bin/env python3
"""Build-host tooling. Target applications contain no Python dependency."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

tool_directory = str(Path(__file__).resolve().parent)
if tool_directory not in sys.path:
    sys.path.insert(0, tool_directory)
import verification_workspace


def host_platform():
    return {'darwin': 'macos', 'win32': 'windows', 'linux': 'linux'}.get(sys.platform)


def checked_build_root(source, destination):
    destination = Path(destination).resolve()
    if not destination.is_relative_to(source.resolve() / 'build'):
        raise ValueError('Verification output must be inside iiGUI/build/')
    return destination


def validate_execution(platform, compile_only, host):
    if platform != host and not compile_only:
        raise ValueError('Foreign target execution is unavailable; explicitly select --compile-only')


def make_manifest(source, output, platform, toolchain=None, cache=None, prefixes=()):
    example = source / 'examples/HelloGUI'
    data = json.loads((example / 'iiSDKBuilder.json').read_text(encoding='utf-8'))
    data['app']['source'] = example.as_posix()
    data['sdkRoot'] = source.parent.as_posix()
    data['buildRoot'] = output.as_posix()
    for sdk in data['sdks']:
        sdk['source'] = (source.parent / sdk['name']).as_posix()
    data['kits'] = [kit for kit in data['kits'] if kit['platform'] == platform]
    if len(data['kits']) != 1:
        raise ValueError('Expected exactly one desktop kit')
    if toolchain:
        data['kits'][0]['toolchain'] = Path(toolchain).resolve().as_posix()
        data['kits'][0]['environment']['PKG_CONFIG_PATH'] = ''
    data['kits'][0]['prefixes'] = [Path(prefix).resolve().as_posix() for prefix in prefixes]
    data['kits'][0]['cache'].update(cache or {})
    return data


def kit_builds(output, platform):
    kit = output/'kits'/platform/'Release'
    return {name: kit/'sdk'/name/'isolated/build' for name in ('iiSDKCore', 'LVUI', 'iiGUI')} | {
        'app': kit/'app/isolated/build'}


def build_environment(toolchain, original=None):
    environment = dict(os.environ if original is None else original)
    for key in ('CPATH', 'CPLUS_INCLUDE_PATH', 'C_INCLUDE_PATH', 'LIBRARY_PATH',
                'LD_LIBRARY_PATH', 'DYLD_LIBRARY_PATH', 'DYLD_FALLBACK_LIBRARY_PATH',
                'DYLD_FRAMEWORK_PATH', 'DYLD_FALLBACK_FRAMEWORK_PATH'):
        environment.pop(key, None)
    if toolchain:
        environment.pop('PKG_CONFIG_PATH', None)
    return environment


def main(argv=None):
    source = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description='Build HelloGUI and verify relocated installed SDK consumers')
    parser.add_argument('--builder', default=shutil.which('iisdk-builder') or 'iisdk-builder')
    parser.add_argument('--platform', choices=['windows', 'macos', 'linux'], default=host_platform())
    parser.add_argument('--toolchain', type=Path)
    parser.add_argument('--compile-only', action='store_true', help='Explicitly skip all target executable tests')
    parser.add_argument('--build-root', type=Path)
    parser.add_argument('--prefix', action='append', default=[], type=Path,
                        help='Explicit target package prefix; repeat for each required dependency')
    parser.add_argument('--cache', action='append', default=[], metavar='KEY=VALUE')
    args = parser.parse_args(argv)
    try:
        if not args.platform:
            raise ValueError('Unsupported build host')
        validate_execution(args.platform, args.compile_only, host_platform())
        if args.platform != host_platform() and not args.toolchain:
            raise ValueError('A foreign desktop target requires --toolchain')
        if args.toolchain and not args.toolchain.resolve().is_file():
            raise ValueError('Toolchain file does not exist')
        if any(not prefix.resolve().is_dir() for prefix in args.prefix):
            raise ValueError('Every explicit target prefix must be an existing directory')
        cache = {}
        for entry in args.cache:
            key, separator, value = entry.partition('=')
            if not separator or not key:
                raise ValueError('--cache requires KEY=VALUE')
            cache[key] = value
        output = checked_build_root(source, args.build_root or source/'build/verify-desktop'/args.platform)
    except ValueError as error:
        parser.error(str(error))
    with verification_workspace.temporary(source, 'desktop-') as run:
        return run_verification(source, args, output, run, cache)


def run_verification(source, args, output, run, cache):
    consumer_build = output/'installed-consumer'/run.name
    if consumer_build.exists() or consumer_build.is_symlink():
        raise FileExistsError('Consumer build already exists: '+consumer_build.as_posix())
    manifest = run/'manifest.json'
    manifest.write_text(json.dumps(make_manifest(source, output, args.platform, args.toolchain, cache, args.prefix),
                                   indent=2)+'\n', encoding='utf-8')
    report = {'platform': args.platform, 'buildHost': host_platform(), 'manifest': manifest.as_posix(),
              'compileOnly': args.compile_only, 'build': 'not_run', 'hostChecks': 'not_run',
              'targetTests': 'not_run', 'helloGUI': 'not_run', 'installedConsumerBuild': 'not_run',
              'installedConsumerExecution': 'not_run', 'commands': []}
    environment = build_environment(args.toolchain)
    cmake = shutil.which('cmake') or 'cmake'
    ctest = shutil.which('ctest') or 'ctest'

    def execute(label, command):
        command = [str(value) for value in command]
        log = run/(label+'.log')
        print(label, flush=True)
        with log.open('w', encoding='utf-8') as stream:
            result = subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT,
                                    env=environment, check=False)
        report['commands'].append({'step': label, 'arguments': command, 'exitCode': result.returncode})
        if result.returncode:
            print(log.read_text(encoding='utf-8', errors='replace'), file=sys.stderr)
            raise RuntimeError(label+' failed')

    try:
        execute('builder', [args.builder, 'build', '--manifest', manifest])
        report['build'] = 'passed'
        builds = kit_builds(output, args.platform)
        gui_build = builds['iiGUI']
        if args.compile_only:
            execute('host-checks', [ctest, '--test-dir', gui_build, '-R', r'^iiGUI\.(boundary|tooling)$',
                                   '--output-on-failure', '--output-junit', run/'host-checks.xml'])
            report['hostChecks'] = 'passed'
        else:
            execute('host-suite', [ctest, '--test-dir', gui_build, '--output-on-failure',
                                  '--output-junit', run/'host-tests.xml'])
            report['hostChecks'] = report['targetTests'] = 'passed'
            execute('hello-gui', [ctest, '--test-dir', builds['app'], '--output-on-failure',
                                 '--output-junit', run/'hello-gui-tests.xml'])
            report['helloGUI'] = 'passed'
        package = [cmake, '-DSOURCE='+source.as_posix(), '-DBUILD='+consumer_build.as_posix(),
                   '-DRESOURCE_ROOT='+run.as_posix(),
                   '-DCORE_BUILD='+builds['iiSDKCore'].as_posix(),
                   '-DLVUI_BUILD='+builds['LVUI'].as_posix(),
                   '-DGUI_BUILD='+gui_build.as_posix()]
        if args.toolchain:
            package.append('-DTOOLCHAIN='+args.toolchain.resolve().as_posix())
        if args.compile_only:
            package.append('-DSKIP_EXECUTION=ON')
        execute('installed-consumer', package+['-P', source/'tests/package.cmake'])
        report['installedConsumerBuild'] = 'passed'
        if not args.compile_only:
            report['installedConsumerExecution'] = 'passed'
        report['status'] = 'built_execution_deferred' if args.compile_only else 'native_verification_passed'
        return 0
    except Exception as error:
        report['status'] = 'failed'
        report['error'] = str(error)
        print(str(error), file=sys.stderr)
        return 1
    finally:
        verification_workspace.remove_owned_tree(consumer_build)
        try:
            consumer_build.parent.rmdir()
        except OSError:
            pass
        print(json.dumps(report, indent=2), flush=True)


if __name__ == '__main__':
    sys.exit(main())
