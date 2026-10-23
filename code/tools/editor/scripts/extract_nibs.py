"""Extract NeXT typedstream objects without discarding geometry or connections.
Development-only: pip install pytypedstream; not needed to build DoomEd.
"""
import sys, json
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[3]/'build_tools/nib_tools'))
import typedstream
from typedstream.archiving import GenericArchivedObject

root=Path(__file__).resolve().parents[3]
output=root/'build_tools/nib-reference'
output.mkdir(parents=True,exist_ok=True)
for nib in (root/'romeo/DoomEd/English.lproj').glob('*.nib/data.nib'):
    objects=[]; ids={}
    def convert(v):
        if isinstance(v,bytes): return v.decode('latin1')
        if v is None or isinstance(v,(str,int,float,bool)): return v
        if isinstance(v,(list,tuple)): return [convert(x) for x in v]
        key=id(v)
        if key in ids:return {'ref':ids[key]}
        index=len(objects);ids[key]=index;obj={'id':index};objects.append(obj)
        if isinstance(v,GenericArchivedObject):
            obj['class']=v.clazz.name.decode()
            obj['groups']=[{'types':[e.decode() for e in g.encodings], 'values':[convert(x) for x in g.values]} for g in v.contents]
        elif hasattr(v,'elements'):
            obj['class']=type(v).__name__;obj['elements']=convert(v.elements)
        else:obj['class']=type(v).__name__;obj['description']=str(v)
        return {'ref':index}
    try:
        convert(typedstream.unarchive_from_file(str(nib)))
        (output/(nib.parent.name+'.json')).write_text(json.dumps(objects,indent=2),encoding='utf8')
        print(nib.parent.name,len(objects))
    except Exception as e: print(nib,e);raise
