"""Static routing audit; it never opens a host or asserts runtime parity."""
from pathlib import Path
import argparse,json,re
ROOT=Path(__file__).resolve().parents[1]
HOSTS=['Host.mm','HostWindows.cpp','HostLinux.cpp','HostWayland.cpp','HostAndroid.cpp','HostIOS.mm','HostWeb.cpp']
SURFACES=['NativeWidgetsMac.mm','NativeWidgetsWindows.cpp','NativeWidgetsLinux.cpp','NativeWidgetsWayland.cpp','NativeWidgetsAndroid.cpp','NativeWidgetsIOS.mm','NativeWidgetsWeb.cpp']
def audit(root=ROOT):
 sources={p.name:p for area in ('Graphics','Platform','Widgets') for p in (root/'src'/area).iterdir() if p.is_file()}
 for name in HOSTS:
  text=(sources[name]).read_text()
  for marker in ['Host::presentation_source()', 'make_shared<PresentationStream>', 'dispatch->loop.post(std::move(task))', '.presentations=presentations', 'impl_->presentations->close()']:
   if marker not in text:raise ValueError(name+': missing '+marker)
  calls=re.findall(r'(?:impl_->)?canvas\.set\(([^;]+)\);',text)
  if not calls or any(not re.search(r'(?:impl_->)?presentations\s*$',args) for args in calls):raise ValueError(name+': raw canvas source missing')
 for name in SURFACES:
  if not re.search(r'callbacks\.renderer,\s*(?:impl_->|view->)?callbacks\.presentations',(sources[name]).read_text()):raise ValueError(name+': widget source missing')
 canvas=(sources['NativeCanvas.cpp']).read_text()
 if 'draw_with_presentation(source,' not in canvas:raise ValueError('NativeCanvas draw boundary missing')
 capture=re.search(r'PixelBuffer NativeCanvas::capture\(\).*?\n',canvas)
 if not capture or 'queued' in capture[0] or 'draw_with_presentation' in capture[0]:raise ValueError('Readback publishes a presentation')
 return {'hosts':len(HOSTS),'widget_surfaces':len(SURFACES),'raw_and_widget_paths':True,'readback_publishes':False,'native_window_executed':False}
if __name__=='__main__':
 parser=argparse.ArgumentParser();parser.add_argument('--root',type=Path,default=ROOT);args=parser.parse_args();print(json.dumps(audit(args.root),indent=2))
