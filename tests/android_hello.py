"""Android delivery tests run on the build host, without a connected phone."""
import importlib.util
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import zipfile

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
spec=importlib.util.spec_from_file_location('android_hello',ROOT/'tools/android_hello.py')
tool=importlib.util.module_from_spec(spec)
spec.loader.exec_module(tool)

class AndroidHello(unittest.TestCase):
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
        self.assertEqual(set(next(p for p in m['sdks'] if p['name']=='iiGUI')['depends']), {'iiSDKCore'})
        self.assertEqual(m['kits'][0]['platform'],'android')
        self.assertEqual(m['kits'][0]['androidAbi'],'arm64-v8a')
        self.assertEqual(m['kits'][0]['cache']['LVUI_HOST_COMPILER'],'/host/lvui-compile')
        self.assertEqual(m['kits'][0]['cache']['CMAKE_DISABLE_FIND_PACKAGE_Qt6'],'ON')
        self.assertEqual(m['kits'][0]['cache']['ANDROID_STL'],'c++_shared')

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

    def test_native_apk_rejects_dex_and_wrong_architecture(self):
        (ROOT/'build').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=ROOT/'build') as temporary:
            apk=Path(temporary)/'hello.apk'
            image=bytearray(64);image[:6]=b'\x7fELF\x02\x01';struct.pack_into('<HH',image,16,3,183)
            def write(extra=None):
                with zipfile.ZipFile(apk,'w') as z:
                    z.writestr('lib/arm64-v8a/libhello_lvui.so',image)
                    z.writestr('assets/Pretendard-Regular.ttf',b'font')
                    z.writestr('assets/Pretendard-OFL.txt',b'license')
                    z.writestr('AndroidManifest.xml',b'manifest')
                    if extra:z.writestr(extra,b'unexpected')
            write();self.assertEqual(tool.check_apk(apk,'arm64-v8a')['nativeLibrary'],'lib/arm64-v8a/libhello_lvui.so')
            for name in ['classes.dex','assets/HelloWorld.lvui','lib/arm64-v8a/libQt6Core.so']:
                write(name)
                with self.assertRaises(ValueError):tool.check_apk(apk,'arm64-v8a')
            struct.pack_into('<H',image,18,62);write()
            with self.assertRaises(ValueError):tool.check_apk(apk,'arm64-v8a')

if __name__=='__main__':unittest.main()
