from pathlib import Path
import re,subprocess,sys
source=(Path(__file__).resolve().parents[1]/'src/Widgets/NativePopoverView.h').read_text(encoding='utf-8')
for macro,message in (("IIGUI_POPOVER_UNDERLAY_BOUNDS_API","requires bounds-aware underlays"),
                      ("IIGUI_POPOVER_COMBINED_MATERIAL_MOTION_API","requires combined material motion"),
                      ("IIGUI_POPOVER_UNTINTED_UNDERLAY_API","requires colored underlays")):
    guard=re.search(r"#if !defined\("+re.escape(macro)+r"\).*?\n#endif",source,re.S)
    if guard is None:raise AssertionError(f"Popover lacks {macro} compatibility guard")
    def run(defines):return subprocess.run([sys.argv[1],'-std=c++23','-E','-x','c++','-'],input=defines+guard.group(),text=True,stdout=subprocess.DEVNULL,stderr=subprocess.PIPE)
    old=run('')
    if old.returncode==0 or message not in old.stderr:raise AssertionError(f"Old {macro} host was not diagnosed: {old.stderr}")
    new=run(f"#define {macro} 1\n")
    if new.returncode:raise AssertionError(f"New {macro} host was rejected: {new.stderr}")
