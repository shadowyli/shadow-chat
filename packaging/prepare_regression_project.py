"""Redirect an unchanged test project to the selected final production objects."""
from pathlib import Path
import argparse
import re
from release_config import ROOT

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--name', required=True)
parser.add_argument('--production-root', type=Path, required=True)
parser.add_argument('--output-root', type=Path, required=True)
args = parser.parse_args()
if not re.fullmatch(r'[a-z_]+', args.name):
    raise SystemExit('Invalid regression project name')
source = ROOT/'tests'/(args.name+'_test.pro')
text = source.read_text(encoding='utf-8')
objects = (args.production_root/'objects').resolve().as_posix()
text = text.replace('$$PWD/../src/release', objects).replace('../src/release', objects)
text = text.replace('../src/', (ROOT/'src').as_posix()+'/')
text = text.replace('../locale/', (ROOT/'locale').as_posix()+'/')
lines = []
for line in text.splitlines():
    match = re.match(r'(\s*(?:SOURCES|HEADERS|FORMS)\s*\+=\s*)(.*)', line)
    if match:
        values = []
        for item in match.group(2).split():
            if not item.startswith('$') and Path(item).suffix in {'.cpp', '.h', '.ui'} and not Path(item).is_absolute():
                item = (ROOT/'tests'/item).resolve().as_posix()
            values.append(item)
        line = match.group(1)+' '.join(values)
    lines.append(line)
lines.append('INCLUDEPATH += '+(ROOT/'tests').as_posix()+' '+(ROOT/'src').as_posix()+' '+(args.production_root/'generated').resolve().as_posix())
lines.append('win32-msvc*: QMAKE_CXXFLAGS += /FC')
destination = args.output_root/'projects'/(args.name+'_test.pro')
destination.parent.mkdir(parents=True, exist_ok=True)
destination.write_text('\n'.join(lines)+'\n', encoding='utf-8')
print('Prepared '+str(destination))
