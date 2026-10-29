"""Verify the built PE executable embeds every owner-supplied ICO frame."""
from pathlib import Path
import struct,hashlib

root=Path(__file__).resolve().parent.parent
icon=(root/'branding/generated/app.ico').read_bytes()
count=struct.unpack_from('<H',icon,4)[0]
expected=set()
for i in range(count):
 size,offset=struct.unpack_from('<II',icon,6+i*16+8)
 expected.add(hashlib.sha256(icon[offset:offset+size]).digest())

data=(root/'test/ShadowChat.exe').read_bytes()
pe=struct.unpack_from('<I',data,0x3c)[0]
assert data[pe:pe+4]==b'PE\0\0'
sections=struct.unpack_from('<H',data,pe+6)[0]
optional_size=struct.unpack_from('<H',data,pe+20)[0]
optional=pe+24
magic=struct.unpack_from('<H',data,optional)[0]
directories=optional+(96 if magic==0x10b else 112)
resource_rva=struct.unpack_from('<I',data,directories+16)[0]
section_table=optional+optional_size
def file_offset(rva):
 for i in range(sections):
  at=section_table+i*40
  virtual_size,virtual_address,raw_size,raw_offset=struct.unpack_from('<IIII',data,at+8)
  if virtual_address<=rva<virtual_address+max(virtual_size,raw_size):return raw_offset+rva-virtual_address
 raise ValueError('Resource RVA outside PE sections')
resource_root=file_offset(resource_rva)
resources={}
def walk(offset,path=()):
 named,ids=struct.unpack_from('<HH',data,resource_root+offset+12)
 for i in range(named+ids):
  name,child=struct.unpack_from('<II',data,resource_root+offset+16+i*8)
  identity=name if not name&0x80000000 else 'named'
  key=path+(identity,)
  if child&0x80000000:walk(child&0x7fffffff,key)
  else:
   rva,size=struct.unpack_from('<II',data,resource_root+child)
   at=file_offset(rva);resources[key]=data[at:at+size]
walk(0)
matched=False
for key,group in resources.items():
 if key[0]!=14:continue
 n=struct.unpack_from('<H',group,4)[0]
 actual=set()
 for i in range(n):
  icon_id=struct.unpack_from('<H',group,6+i*14+12)[0]
  frames=[value for path,value in resources.items() if path[0]==3 and path[1]==icon_id]
  actual.update(hashlib.sha256(frame).digest() for frame in frames)
 if actual==expected:matched=True
assert matched,'Executable still embeds an older/different application icon'
print(f'PASS executable PE icon resources exactly match all {count} generated ICO frames')
