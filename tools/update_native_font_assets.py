"""Refresh the complete original font inventory without a runtime LVRS link."""
from pathlib import Path
import argparse,hashlib,json,re,shutil
ROOT=Path(__file__).resolve().parents[1]

def expected():
    original=ROOT.parent/'LVRS'
    policy=(original/'src/backend/fonts/fontpolicy.cpp').read_text()
    block=re.search(r'kBundledFontResources\[\]\s*=\s*\{(.*?)\};',policy,re.S)[1]
    files=re.findall(r'resources/font/([^"/]+\.ttf)',block)
    if len(files)!=10 or len(set(files))!=10:raise ValueError('Original bundled font inventory changed')
    weights={'Thin':100,'ExtraLight':200,'Light':300,'Regular':400,'Medium':500,'SemiBold':600,'Bold':700,'ExtraBold':800,'Black':900}
    result=[]
    for name in files:
        family,style=Path(name).stem.rsplit('-',1)
        result.append({'file':name,'source':'LVRS/resources/font/'+name,'sha256':hashlib.sha256((original/'resources/font'/name).read_bytes()).hexdigest(),'family':family,'style':style,'weight':weights[style]})
    return result

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--check',action='store_true');args=parser.parse_args()
    path=ROOT/'resources/lvrs-native-appearance.json';manifest=json.loads(path.read_text());fonts=expected()
    if args.check:
        if manifest['fonts']!=fonts:raise ValueError('Native font manifest differs from the original complete inventory')
        for entry in fonts:
            if hashlib.sha256((ROOT/'resources/native'/entry['file']).read_bytes()).hexdigest()!=entry['sha256']:raise ValueError('Native font asset differs: '+entry['file'])
    else:
        for entry in fonts:
            target=ROOT/'resources/native'/entry['file'];source=ROOT.parent/entry['source']
            if not target.exists() or hashlib.sha256(target.read_bytes()).hexdigest()!=entry['sha256']:shutil.copyfile(source,target)
        manifest['fonts']=fonts;path.write_text(json.dumps(manifest,indent=2)+'\n')
    print(json.dumps({'fonts':len(fonts),'resources':len(fonts)+2,'original_order':True}))

if __name__=='__main__':main()
