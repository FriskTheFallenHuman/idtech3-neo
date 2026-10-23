import json
from pathlib import Path
root=Path(__file__).resolve().parents[3]
panels=[]
for path in sorted((root/'build_tools/nib-reference').glob('*.nib.json')):
    obs=json.loads(path.read_text()); nib=path.stem.replace('.nib','')
    def obj(ref):return obs[ref['ref']] if isinstance(ref,dict) and 'ref' in ref else {}
    def groups(o):return o.get('groups',[])
    def values(o):return [v for g in groups(o) for v in g['values']]
    outlets={};actions={}
    for o in obs:
        if o['class'] in ('IBOutletConnector','IBControlConnector'):
            a,b,name=o['groups'][0]['values']
            if o['class']=='IBOutletConnector':outlets[obj(b).get('id',-1)]=name
            else:actions[obj(a).get('id',-1)]=name
    def rect(o):
        return next((g['values'] for g in groups(o) if g['types']==['f']*4),None)
    def children(o):
        return next((obj(g['values'][0]).get('elements',[]) for g in groups(o) if g['types']==['@','s','s','@']),[])
    def cell(o):
        return obj(next((g['values'][1] for g in groups(o) if g['types']==['i','@','s']),None))
    def detail(c):
        if not groups(c):return ('','Helvetica',12,0,'')
        g=groups(c)[0]['values'];text=g[0] if isinstance(g[0],str) else ''
        font=obj(g[1]) if len(g)>1 else {};fv=values(font)
        fname=fv[0] if fv and isinstance(fv[0],str) else 'Helvetica';fs=fv[1] if len(fv)>1 else 12
        images=[]
        for v in values(c):
            im=obj(v)
            if im.get('class')=='NXImage':images += [s for s in values(im) if isinstance(s,str)]
        return text,fname,fs,g[2] if len(g)>2 else 0,next((s for s in images if s.startswith('NX') or (root/'tools/DoomEd/assets'/ (s+'.tiff')).exists()), '')
    for win in obs:
        if win['class']!='WindowTemplate':continue
        f=rect(win);v=win['groups'][1]['values'];title=v[4];view=obj(v[7]);controls=[];visited=set()
        def walk(o,ox=0,oy=0,isroot=False):
            if o.get('id') in visited:return
            visited.add(o.get('id'));r=rect(o)
            if not r:return
            x,y,w,h=r
            if isroot:x=y=0
            x+=ox;y+=oy
            cls=o['class'];out=outlets.get(o['id'],'');action=actions.get(o['id'],'')
            def add(kind,c,rr,row=0):
                text,font,fs,flags,im=detail(c)
                if kind=='button' and im.startswith('NXswitch'):kind='check'
                if kind=='button' and im.startswith('NXradio'):kind='radio'
                if kind=='field' and not (flags&0x1000):kind='label'
                controls.append(dict(kind=kind,text=text,font=font,fontsize=fs,flags=flags,image=im,
                    action=action,outlet=out,rect=rr,row=row))
            if cls in ('Button','TextField'):add('button' if cls=='Button' else 'field',cell(o),[x,y,w,h])
            elif cls in ('Matrix','Form'):
                mg=next((g['values'] for g in groups(o) if ''.join(g['types'])=='@:@iiii'),None)
                if mg:
                    cells=obj(mg[0]).get('elements',[]);rows,cols=mg[-2:]
                    ffs=[g['values'] for g in groups(o) if g['types']==['f','f']];cw,ch=ffs[0];sx,sy=ffs[1]
                    for i,ref in enumerate(cells):
                        c=obj(ref);cr=[x+(i%max(1,cols))*(cw+sx),y+h-(i//max(1,cols)+1)*ch-(i//max(1,cols))*sy,cw,ch]
                        if cls=='Form':
                            fg=next((g['values'] for g in groups(c) if g['types']==['f','@']),None)
                            if fg:
                                label=obj(fg[1]);lw=fg[0];add('label',label,[cr[0],cr[1],lw,cr[3]],i)
                                cr=[cr[0]+lw,cr[1],cr[2]-lw,cr[3]]
                            add('field',c,cr,i)
                        else:add('button',c,cr,i)
            elif cls in ('ScrollView','NXBrowser','CustomView','NXSplitView','Slider','NXColorWell'):
                custom=next((s for s in values(o) if isinstance(s,str)),cls)
                controls.append(dict(kind=cls,text=custom,font='Helvetica',fontsize=12,flags=0,image='',action=action,outlet=out,rect=[x,y,w,h],row=0))
            if cls not in ('NXBrowser','ScrollView','Matrix','Form'):
                for child in children(o):walk(obj(child),x,y)
        walk(view,isroot=True)
        panels.append(dict(key=nib+':'+title,title=title,frame=f,controls=controls))
out=root/'tools/DoomEd/NextLayouts.h'
def s(x):return json.dumps(x,ensure_ascii=True)
lines=['// Generated from the original NeXT NIBs by scripts/make_next_layouts.py.','#pragma once','#include "NextUI.h"','namespace de {','inline const std::vector<NextPanel>& originalPanels() {','static const std::vector<NextPanel> panels = {']
for p in panels:
    lines.append('{'+','.join([s(p['key']),s(p['title']),'{'+','.join(str(float(n))+'f' for n in p['frame'])+'}'])+',{')
    for c in p['controls']:
        fields=[s(c[k]) for k in ('kind','text','font','image','action','outlet')]
        fields+=['{'+','.join(str(float(n))+'f' for n in c['rect'])+'}',str(float(c['fontsize']))+'f',str(c['row']),str(c['flags'])]
        lines.append('{'+','.join(fields)+'},')
    lines.append('}},')
lines+=['}; return panels;','}','}']
out.write_text('\n'.join(lines))
(root/'build_tools/nib-reference/layouts.json').write_text(json.dumps(panels,indent=2))
for p in panels:print(p['key'],p['frame'],len(p['controls']))
