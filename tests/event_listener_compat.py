"""Check event-kind bounds against host headers with progressively newer events."""
from pathlib import Path
import re
import subprocess
import sys

source=(Path(__file__).resolve().parents[1]/'src/Widgets/NativeEventListenerView.h').read_text(encoding='utf-8')
guard=re.search(r'#if defined\(IIGUI_POINTER_CANCEL_API\).*?\n#endif',source,re.S)
if guard is None:
    raise AssertionError('EventListener lacks an event-kind compatibility guard')
hosts=(
    ('old','commit_composition',''),
    ('key cancel','commit_composition, key_cancel','#define IIGUI_KEY_CANCEL_API 1\n'),
    ('text selection','commit_composition, key_cancel, text_selection',
     '#define IIGUI_KEY_CANCEL_API 1\n#define IIGUI_TEXT_SELECTION_API 1\n'),
    ('pointer cancel','commit_composition, key_cancel, text_selection, pointer_cancel',
     '#define IIGUI_KEY_CANCEL_API 1\n#define IIGUI_TEXT_SELECTION_API 1\n#define IIGUI_POINTER_CANCEL_API 1\n'),
)
for host,names,defines in hosts:
    snippet=(defines+'namespace ii::gui { enum class WidgetInputKind { '+names+' }; }\n'
             'int main() {\n'+guard.group()+'\nreturn unsigned(last_input_kind);\n}\n')
    compiled=subprocess.run([sys.argv[1],'-std=c++23','-fsyntax-only','-x','c++','-'],
                            input=snippet,text=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    if compiled.returncode:raise AssertionError(f'EventListener rejected {host} iiGUI: {compiled.stderr}')
