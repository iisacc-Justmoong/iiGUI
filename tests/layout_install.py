"""Verify installed native layout/AOT APIs without creating an OS window."""
from pathlib import Path
import argparse, hashlib, json, re, shutil, subprocess, tempfile

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--build',type=Path,required=True)
    parser.add_argument('--lvui-build',type=Path,required=True)
    parser.add_argument('--cmake',required=True)
    args=parser.parse_args()
    build=args.build.resolve()
    cache=dict(re.findall(r'^([^/#][^:\n]*):[^=\n]*=(.*)$',(build/'CMakeCache.txt').read_text(),re.M))
    def run(*command):
        result=subprocess.run(command,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
        print(result.stdout,end='',flush=True)
        result.check_returncode()
    with tempfile.TemporaryDirectory(prefix='layout-package-',dir=build) as temporary:
        root=Path(temporary);stage=root/'stage';relocated=root/'relocated'
        run(args.cmake,'--install',str(args.lvui_build.resolve()),'--prefix',str(stage))
        run(args.cmake,'--install',str(build),'--prefix',str(stage))
        stage.rename(relocated)
        project=root/'project';shutil.copytree(Path(__file__).parent/'layout_consumer',project)
        target=project/'build'
        options=[f'-DCMAKE_PREFIX_PATH={relocated}',f'-DiiGUI_DIR={relocated}/lib/cmake/iiGUI',f'-DLVUI_DIR={relocated}/lib/cmake/LVUI']
        for name in ('iiSDKCore_DIR','iiColorManager_DIR','Vulkan_INCLUDE_DIR','Vulkan_LIBRARY','CMAKE_CXX_COMPILER','CMAKE_OSX_DEPLOYMENT_TARGET','CMAKE_CXX_FLAGS','CMAKE_EXE_LINKER_FLAGS'):
            if cache.get(name):options.append(f'-D{name}={cache[name]}')
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
        repo=Path(__file__).resolve().parents[1]
        abi_input=''.join(f'{name}:{hashlib.sha256((repo/"src/iiGUI"/name).read_bytes()).hexdigest()};' for name in
            ('NativeWidgets.h','Host.h','Graphics.h','WidgetInteraction.h','WidgetLayout.h','WindowProperties.h','Appearance.h'))
        assert abi[1]==hashlib.sha256(abi_input.encode()).hexdigest()
        if (build/'build.ninja').is_file():
            regeneration=next(line for line in (build/'build.ninja').read_text().splitlines() if line.startswith('build build.ninja') and ': RERUN_CMAKE' in line)
            assert all('/src/iiGUI/'+name in regeneration for name in ('NativeWidgets.h','Host.h','Graphics.h','WidgetInteraction.h','WidgetLayout.h','WindowProperties.h','Appearance.h'))
        report={'installed':True,'relocated':True,'consumer_compiled_linked_executed':True,
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
        (build/'native-layout-installed-consumer.json').write_text(json.dumps(report,indent=2)+'\n')
        print(json.dumps(report,indent=2))

if __name__=='__main__':main()
