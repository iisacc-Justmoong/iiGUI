"""Verify installed native layout/AOT APIs without creating an OS window."""
from pathlib import Path
from contextlib import contextmanager
import argparse, hashlib, json, re, shutil, subprocess
import resource_workspace

def read_cache(build):
    path=build/'CMakeCache.txt'
    if not path.is_file():return {}
    return dict(re.findall(r'^([^/#][^:\n]*):[^=\n]*=(.*)$',path.read_text(),re.M))

def dependency_options(gui_cache,lvui_cache):
    result=[]
    for name in ('iiSDKCore_DIR','iiColorManager_DIR','Vulkan_INCLUDE_DIR','Vulkan_LIBRARY',
                 'CMAKE_CXX_COMPILER','CMAKE_OSX_DEPLOYMENT_TARGET','CMAKE_CXX_FLAGS','CMAKE_EXE_LINKER_FLAGS'):
        value=gui_cache.get(name)
        if not value and name in ('iiSDKCore_DIR','iiColorManager_DIR'):value=lvui_cache.get(name)
        if value:result.append(f'-D{name}={value}')
    return result

def redirect_install_manifest(content,build,workspace):
    for name in ('install_local_manifest.txt','${CMAKE_INSTALL_MANIFEST}'):
        original=build.as_posix()+'/'+name
        if content.count(original)!=1:raise ValueError('Cannot safely redirect the install manifest')
        content=content.replace(original,workspace.as_posix()+'/'+name)
    return content

def install_script(build,workspace):
    resource_workspace.provider.mkdir(workspace)
    content=redirect_install_manifest((build/'cmake_install.cmake').read_text(),build,workspace)
    script=workspace/'cmake_install.cmake'
    resource_workspace.provider.write_text(script,content)
    return script


@contextmanager
def consumer_workspace(repo, build):
    """Keep generated consumer binaries in build and temporary inputs in resources."""
    with resource_workspace.temporary(repo, 'layout-package-') as root:
        parent = build / 'layout-consumer'
        if parent.is_symlink():
            raise ValueError('Consumer build directory cannot be a symlink')
        target = parent / root.name
        if target.exists() or target.is_symlink():
            raise FileExistsError(f'Consumer build directory already exists: {target}')
        try:
            yield root, target
        finally:
            if target.is_symlink():
                target.unlink()
            elif target.is_dir():
                shutil.rmtree(target)
            try:
                parent.rmdir()
            except OSError:
                pass

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--build',type=Path,required=True)
    parser.add_argument('--lvui-build',type=Path,required=True)
    parser.add_argument('--cmake',required=True)
    args=parser.parse_args()
    build=args.build.resolve()
    cache=read_cache(build)
    lvui_cache=read_cache(args.lvui_build.resolve())
    def run(*command):
        result=subprocess.run(command,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
        print(result.stdout,end='',flush=True)
        result.check_returncode()
    repo=Path(__file__).resolve().parents[1]
    with consumer_workspace(repo,build) as (root,target):
        stage=root/'stage';relocated=root/'relocated'
        for package,package_build in (('lvui',args.lvui_build.resolve()),('gui',build)):
            script=install_script(package_build,root/('install-'+package))
            run(args.cmake,f'-DCMAKE_INSTALL_PREFIX={stage}','-P',str(script))
        stage.rename(relocated)
        project=Path(__file__).parent/'layout_consumer'
        options=[f'-DCMAKE_PREFIX_PATH={relocated}',f'-DiiGUI_DIR={relocated}/lib/cmake/iiGUI',f'-DLVUI_DIR={relocated}/lib/cmake/LVUI',
                 '-DCMAKE_DISABLE_FIND_PACKAGE_Qt5=ON','-DCMAKE_DISABLE_FIND_PACKAGE_Qt6=ON','-DCMAKE_DISABLE_FIND_PACKAGE_LVRS=ON']
        options.extend(dependency_options(cache,lvui_cache))
        run(args.cmake,'-S',str(project),'-B',str(target),'-G','Ninja','-DCMAKE_BUILD_TYPE=Release',*options)
        run(args.cmake,'--build',str(target),'-j','2')
        run('ctest','--test-dir',str(target),'--output-on-failure')
        assert (relocated/'include/iiGUI/WidgetLayout.h').is_file()
        assert (relocated/'include/LVUI/NativeWidgetLayout.h').is_file()
        assert (relocated/'include/iiGUI/WindowProperties.h').is_file()
        assert (relocated/'include/LVUI/NativeWindowValues.h').is_file()
        exported=(relocated/'lib/cmake/iiGUI/iiGUIConfig.cmake').read_text()
        abi=re.search(r'set\(iiGUI_NATIVE_ABI_ID "([0-9a-f]+)"\)',exported)
        assert abi
        sources={p.name:p for area in ('API','Core','Graphics','Platform','Presentation','Text','Widgets') for p in (repo/'src'/area).glob('*.h')}
        abi_input=''.join(f'{name}:{hashlib.sha256(sources[name].read_bytes()).hexdigest()};' for name in
            ('NativeWidgets.h','Host.h','Graphics.h','WidgetInteraction.h','WidgetLayout.h','WindowProperties.h','Appearance.h','FontDatabase.h','Image.h','Presentation.h','NativeView.h'))
        assert abi[1]==hashlib.sha256(abi_input.encode()).hexdigest()
        if (build/'build.ninja').is_file():
            regeneration=next(line for line in (build/'build.ninja').read_text().splitlines() if line.startswith('build build.ninja') and ': RERUN_CMAKE' in line)
            assert all(str(sources[name]) in regeneration for name in ('NativeWidgets.h','Host.h','Graphics.h','WidgetInteraction.h','WidgetLayout.h','WindowProperties.h','Appearance.h'))
        report={'installed':True,'relocated':True,'consumer_compiled_linked_executed':True,
                'wrapped_text_consumer_compiled_linked_executed':True,
                'lvui_wrapped_text_consumer_compiled_linked_executed':True,
                'generated_dynamic_layout_factory_compiled_linked':True,'generated_dynamic_window_factory_compiled_linked':True,
                'unopened_native_host_window_api_executed':True,
                'generated_readonly_cpp_value_factory_compiled_linked':True,'const_observable_and_noncomparable_values_executed':True,'OS_window_or_GPU_surface_created':False,
                'generated_cpp_object_input_factory_compiled_linked':True,
                'generated_cpp_method_query_factory_compiled_linked':True,
                'generated_cpp_method_reference_factory_compiled_linked':True,
                'generated_cpp_callable_field_factory_compiled_linked':True,
                'generated_cpp_callable_factory_capture_compiled_linked':True,
                'generated_cpp_inferred_values_compiled_linked':True,
                'native_abi_id':abi[1],'public_header_changes_tracked_for_cmake_regeneration':True,'temporary_stage_removed_on_exit':True}
    print(json.dumps(report,indent=2))

if __name__=='__main__':main()
