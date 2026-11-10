"""Compile the Android IME bridge and verify its JNI and DEX contracts, without an Android runtime."""
from pathlib import Path
import argparse
import os
import re
import subprocess
import tempfile


def run(command, env=None):
    result=subprocess.run([str(value) for value in command],text=True,stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT,env=env)
    if result.returncode:
        raise RuntimeError('Android IME verification failed: '+result.stdout)
    return result.stdout


def main():
    parser=argparse.ArgumentParser()
    for name in ('jar','javac','javap','d8','build'):
        parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args()
    root=Path(__file__).resolve().parents[1]
    source=root/'src/Platform/AndroidImeBridge.java'
    native=(root/'src/Platform/AndroidIme.cpp').read_text(encoding='utf-8')
    keep=(root/'src/Platform/AndroidImeBridge.pro').read_text(encoding='utf-8')
    if '-keep class com.iisacc.iigui.AndroidImeBridge { *; }' not in keep:
        raise RuntimeError('Android shrinker rules do not preserve the native bridge')
    methods={
        'nativeRead':'(JJII)Ljava/lang/String;',
        'nativeCommand':'(JJILjava/lang/String;II)Z',
        'attach':'(Landroid/app/Activity;J)V',
        'update':'(Landroid/app/Activity;JJIIIIIIZ)V',
        'detach':'(Landroid/app/Activity;J)V',
    }
    args.build.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='android-ime-java-',dir=args.build) as temporary:
        output=Path(temporary);classes=output/'classes';dex=output/'dex';classes.mkdir();dex.mkdir()
        run([args.javac,'--release','8','-Xlint:-options','-classpath',args.jar,'-d',classes,source])
        listing=run([args.javap,'-s','-private','-classpath',classes,'com.iisacc.iigui.AndroidImeBridge'])
        for name,descriptor in methods.items():
            pattern=r'\b'+re.escape(name)+r'\([^\n]*\);\s*descriptor: '+re.escape(descriptor)+r'\s'
            if not re.search(pattern,listing):raise RuntimeError('Java method has the wrong JNI descriptor: '+name)
            if '"'+name+'"' not in native or '"'+descriptor+'"' not in native:
                raise RuntimeError('Native bridge does not register or invoke Java method: '+name)
        connection=run([args.javap,'-c','-private','-classpath',classes,
                        'com.iisacc.iigui.AndroidImeBridge$Connection'])
        method=re.search(r'public boolean deleteSurroundingTextInCodePoints\(int, int\);(.*?)(?=\n  (?:public|private|protected) )',
                         connection,re.S)
        if not method or not re.search(r'\bbipush\s+6\b',method.group(1)) or 'Method command:' not in method.group(1):
            raise RuntimeError('Java codepoint deletion is not routed to the native text service')
        if not re.search(r'case\s+6\s*:\s*return\s+state->protocol\.erase_surrounding_codepoints\(',native):
            raise RuntimeError('Native codepoint deletion command is not connected to its protocol')
        method=re.search(r'public boolean setComposingRegion\(int, int\);(.*?)(?=\n  (?:public|private|protected) )',
                         connection,re.S)
        if not method or not re.search(r'\bbipush\s+7\b',method.group(1)) or 'Method command:' not in method.group(1):
            raise RuntimeError('Java composition region is not routed to the native text service')
        if not re.search(r'case\s+7\s*:.*?state->protocol\.composing_region\(',native,re.S):
            raise RuntimeError('Native composition-region command is not connected to its protocol')
        view=run([args.javap,'-c','-private','-classpath',classes,
                  'com.iisacc.iigui.AndroidImeBridge$ImeView'])
        if not re.search(r'\biload\s+9\s+\d+:\s+ifeq\s+\d+\s+\d+:\s+aload_0\s+\d+:\s+invokevirtual[^\n]*Method publishSelection:',view):
            raise RuntimeError('Java bridge does not conditionally suppress region-only selection notifications')
        environment=os.environ.copy();environment['JAVA_HOME']=str(args.javac.resolve().parent.parent)
        class_files=sorted(classes.rglob('*.class'))
        run([args.d8,'--min-api','24','--lib',args.jar,'--output',dex,*class_files],environment)
        payload=(dex/'classes.dex').read_bytes()
        if not payload.startswith(b'dex\n') or b'Lcom/iisacc/iigui/AndroidImeBridge;' not in payload:
            raise RuntimeError('DEX does not include the Android IME bridge')
        if list(dex.glob('*.dex'))!=[dex/'classes.dex']:
            raise RuntimeError('Android IME unexpectedly requires multiple DEX files')
    print('Android IME Java API, JNI descriptors and DEX packaging match; no app was executed')


if __name__=='__main__':main()
