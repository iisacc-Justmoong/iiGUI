"""Android delivery tests run on the build host, without a connected phone."""
import importlib.util
import contextlib
import errno
import io
from pathlib import Path
import shutil
import struct
import sys
from types import SimpleNamespace
import unittest
from unittest.mock import patch
import zipfile
import xml.etree.ElementTree as ET
import resource_workspace

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
spec=importlib.util.spec_from_file_location('android_hello',ROOT/'tools/android_hello.py')
tool=importlib.util.module_from_spec(spec)
spec.loader.exec_module(tool)

def remove_test_output(output):
    """Remove our fixture even if a provider preview arrives during cleanup."""
    for attempt in range(3):
        if not output.is_dir() or output.is_symlink():
            return
        try:
            shutil.rmtree(output)
            return
        except OSError as error:
            if error.errno != errno.ENOTEMPTY or attempt == 2:
                raise

class AndroidHello(unittest.TestCase):
    def test_apk_and_verification_state_have_separate_locations(self):
        with tool.verification_workspace.temporary(ROOT, 'android-') as workspace:
            output=ROOT/'build/android-test'
            paths=tool.android_paths(ROOT,output,workspace)
            for name in ('manifest','staging','keystore','unsigned','aligned','signed'):
                self.assertTrue(paths[name].is_relative_to(workspace))
            self.assertTrue(paths['apk'].is_relative_to(output))
            self.assertTrue(paths['screenshot'].is_relative_to(ROOT/'resources/outputs'))
            self.assertEqual(paths['native'],output/'builder/kits/android/Release/app/isolated/build/libhello_lvui.so')
        self.assertFalse(workspace.exists())

    def test_screenshot_output_never_overwrites_existing_data(self):
        with resource_workspace.temporary(ROOT, 'android-test-') as fixture:
            screenshot=fixture/'resources/outputs/android-hello/run/device-screen.png'
            tool.save_screenshot(fixture,screenshot,b'first')
            with self.assertRaises(FileExistsError):tool.save_screenshot(fixture,screenshot,b'second')
            self.assertEqual(screenshot.read_bytes(),b'first')
            with self.assertRaises(FileExistsError):
                tool.save_screenshot(fixture,fixture/'resources/outputs/android-hello/../escape.png',b'escape')

    def test_apk_publication_is_exclusive_and_preserves_existing_output(self):
        with resource_workspace.temporary(ROOT, 'android-test-') as fixture:
            provider=resource_workspace.provider
            signed=fixture/'signed.apk';output=fixture/'final.apk'
            provider.write_bytes(signed,b'first')
            tool.publish_apk(signed,output)
            self.assertEqual(output.read_bytes(),b'first')
            provider.write_bytes(signed,b'second')
            with self.assertRaises(FileExistsError):tool.publish_apk(signed,output)
            self.assertEqual(output.read_bytes(),b'first')

    def test_existing_built_apk_is_not_overwritten(self):
        with resource_workspace.temporary(ROOT, 'android-test-') as fixture:
            provider=resource_workspace.provider
            output=ROOT/'build'/('android-test-'+fixture.name)
            try:
                provider.mkdir(output,parents=True)
                apk=output/'iiSDK-LVUI-HelloWorld.apk'
                provider.write_bytes(apk,b'user-apk')
                with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as raised:
                    tool.main(['--sdk',str(fixture),'--ndk',str(fixture),'--jdk',str(fixture),
                               '--host-compiler',str(fixture/'compiler'),'--builder','fake-builder',
                               '--build-root',str(output)])
                self.assertEqual(raised.exception.code,2)
                self.assertEqual(apk.read_bytes(),b'user-apk')
            finally:
                remove_test_output(output)

    def test_failed_build_cleans_verification_state_without_running_device(self):
        calls=[]
        def failed(command, **kwargs):
            calls.append([str(value) for value in command])
            return SimpleNamespace(returncode=1,stderr=b'failure')
        with resource_workspace.temporary(ROOT, 'android-test-') as fixture:
            output=ROOT/'build'/('android-test-'+fixture.name)
            try:
                with patch.object(tool.subprocess, 'run', side_effect=failed), \
                     contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                    result=tool.main(['--sdk',str(fixture),'--ndk',str(fixture),'--jdk',str(fixture),
                                      '--host-compiler',str(fixture/'compiler'),'--builder','fake-builder',
                                      '--build-root',str(output)])
                self.assertEqual(result,1)
                manifest=Path(calls[0][calls[0].index('--manifest')+1])
                self.assertTrue(manifest.is_relative_to(ROOT/'resources/.state/verification'))
                self.assertFalse(manifest.exists())
                self.assertFalse((output/'verification.json').exists())
                self.assertFalse((output/'hello-debug.keystore').exists())
            finally:
                remove_test_output(output)

    def test_discovery_requires_authorized_physical_device(self):
        rows=tool.devices_from_adb('List of devices attached\nabc device usb:1 model:SM_S918N\ndef unauthorized usb:2\nghi offline\nemulator-5554 device product:sdk_gphone\n')
        self.assertEqual(tool.select_device(rows,None),'abc')
        with self.assertRaises(ValueError):tool.select_device(rows,'def')
        with self.assertRaises(ValueError):tool.select_device(rows,'emulator-5554')

    def test_multiple_devices_need_an_explicit_serial(self):
        rows=tool.devices_from_adb('a device usb:1\nb device usb:2\n')
        with self.assertRaises(ValueError):tool.select_device(rows,None)
        self.assertEqual(tool.select_device(rows,'b'),'b')
        with self.assertRaises(ValueError):tool.select_device([],None)

    def test_emulator_requires_explicit_kind_and_serial(self):
        rows=tool.devices_from_adb('phone device usb:1\nemulator-5580 device product:sdk_gphone64_arm64\n')
        self.assertEqual(tool.select_device(rows,'emulator-5580','emulator'),'emulator-5580')
        self.assertEqual(tool.select_device(rows,None),'phone')
        for serial,kind in [('phone','emulator'),('emulator-5580','physical'),(None,'emulator'),('phone','unknown')]:
            with self.assertRaises(ValueError):tool.select_device(rows,serial,kind)

    def test_emulator_offline_and_other_serial_are_never_selected(self):
        rows=tool.devices_from_adb('emulator-5580 offline\nemulator-5582 device\nphone device usb:1\n')
        with self.assertRaises(ValueError):tool.select_device(rows,'emulator-5580','emulator')
        self.assertEqual(tool.select_device(rows,'emulator-5582','emulator'),'emulator-5582')

    def test_runtime_status_keeps_emulator_and_physical_proof_separate(self):
        for kind,flag,status in [('physical','physicalDeviceTested','native_device_verified'),('emulator','emulatorDeviceTested','native_emulator_verified')]:
            report={'physicalDeviceTested':False,'emulatorDeviceTested':False}
            tool.record_execution(report,kind,'42','screen.png')
            self.assertTrue(report['launched']);self.assertTrue(report[flag])
            self.assertEqual(report['physicalDeviceTested'],kind=='physical')
            self.assertEqual(report['emulatorDeviceTested'],kind=='emulator')
            self.assertEqual(report['status'],status)

    def test_log_start_uses_device_clock_without_host_timezone_conversion(self):
        self.assertEqual(tool.device_log_start('10-27 05:00:12.000\n'),'10-27 05:00:12.000')
        for value in ('','date: invalid option','10-27 05:00:12.000\nextra','10-27 05:00:12.000; echo unsafe'):
            with self.assertRaises(ValueError):tool.device_log_start(value)

    def test_native_manifest_has_complete_graph(self):
        m=tool.make_manifest(ROOT,ROOT/'build/test-android',Path('/ndk'),Path('/host/lvui-compile'),'arm64-v8a',29)
        self.assertEqual({p['name'] for p in m['sdks']},{'iiSDKCore','iiColorManager','LVUI','iiGUI'})
        self.assertEqual(set(m['app']['depends']), {'LVUI','iiGUI'})
        self.assertTrue(all(not p.get('depends') for p in m['sdks']))
        self.assertEqual(m['kits'][0]['platform'],'android')
        self.assertEqual(m['kits'][0]['androidAbi'],'arm64-v8a')
        self.assertEqual(m['kits'][0]['cache']['LVUI_HOST_COMPILER'],'/host/lvui-compile')
        self.assertEqual(m['kits'][0]['cache']['CMAKE_DISABLE_FIND_PACKAGE_Qt6'],'ON')
        self.assertEqual(m['kits'][0]['cache']['ANDROID_STL'],'c++_shared')
        self.assertEqual(m['kits'][0]['prefixes'],[
            str(ROOT/'build/test-android/kits/android/Release/prefix/iiSDKCore'),
            str(ROOT/'build/test-android/kits/android/Release/prefix/iiColorManager')])
        manifest=ET.parse(ROOT/'examples/HelloAndroid/AndroidManifest.xml').getroot()
        self.assertEqual(manifest.find('application').get('{http://schemas.android.com/apk/res/android}hasCode'),'true')

    def test_missing_observation_runtime_fails_before_installation(self):
        dependencies={'hello.so':['libiiSDKCoreObservation.so','libc.so'],
                      'libiiSDKCoreObservation.so':['libc++_shared.so'],'libc++_shared.so':['libc.so']}
        files={name:Path('/kit/lib')/name for name in dependencies}
        result=tool.native_closure([files['hello.so']],lambda p:dependencies[p.name],files.get)
        self.assertEqual(set(result),set(dependencies))
        del files['libiiSDKCoreObservation.so']
        with self.assertRaisesRegex(ValueError,'libiiSDKCoreObservation.so'):
            tool.native_closure([files['hello.so']],lambda p:dependencies[p.name],files.get)

    def test_native_closure_deduplicates_diamonds_and_cycles(self):
        dependencies={'a.so':['b.so','c.so'],'b.so':['c.so'],'c.so':['a.so','liblog.so']}
        result=tool.native_closure([Path('/kit/a.so')],lambda p:dependencies[p.name],lambda n:Path('/kit')/n)
        self.assertEqual(set(result),set(dependencies))

    def test_native_apk_requires_only_the_android_ime_dex_and_correct_architecture(self):
        with resource_workspace.temporary(ROOT, 'android-') as temporary:
            apk=temporary/'hello.apk'
            image=bytearray(64);image[:6]=b'\x7fELF\x02\x01';struct.pack_into('<HH',image,16,3,183)
            dex=bytearray(112);dex[:8]=b'dex\n035\x00';dex.extend(b'Lcom/iisacc/iigui/AndroidImeBridge;')
            struct.pack_into('<III',dex,32,len(dex),112,0x12345678)
            def write(extra=None,include_dex=True,dex_image=dex):
                with resource_workspace.provider.staged_path(apk) as staged:
                    with zipfile.ZipFile(staged,'w') as z:
                        z.writestr('lib/arm64-v8a/libhello_lvui.so',image)
                        z.writestr('assets/Pretendard-Regular.ttf',b'font')
                        z.writestr('assets/Pretendard-OFL.txt',b'license')
                        z.writestr('AndroidManifest.xml',b'manifest')
                        if include_dex:z.writestr('classes.dex',dex_image)
                        if extra:z.writestr(extra,b'unexpected')
            write();self.assertTrue(tool.check_apk(apk,'arm64-v8a')['dexPresent'])
            write(include_dex=False)
            with self.assertRaises(ValueError):tool.check_apk(apk,'arm64-v8a')
            write(dex_image=b'wrong')
            with self.assertRaises(ValueError):tool.check_apk(apk,'arm64-v8a')
            for name in ['classes2.dex','classes2.DEX','assets/HelloWorld.lvui','assets/Old.qml',
                         'lib/arm64-v8a/libQt6Core.so','lib/arm64-v8a/libLVRS.so']:
                write(name)
                with self.assertRaises(ValueError):tool.check_apk(apk,'arm64-v8a')
            struct.pack_into('<H',image,18,62);write()
            with self.assertRaises(ValueError):tool.check_apk(apk,'arm64-v8a')

if __name__=='__main__':unittest.main()
