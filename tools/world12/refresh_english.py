#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Refresh a supplied Extended master from supplied EA tables. No game data shipped."""
import argparse, gzip, hashlib, json, re, unicodedata, zipfile, sys
from collections import defaultdict
from datetime import date, timedelta
from pathlib import Path


def norm(value):
    value=''.join(c for c in unicodedata.normalize('NFKD',value.upper()) if not unicodedata.combining(c))
    value=re.sub(r'\b(?:FC|FK|CF|SC|SL|CP|AC|AFC|CA)\b','',value)
    return re.sub('[^A-Z0-9]','',value)


def refresh(source,tables,nation_world,english,out,crests=None):
    source=Path(source);source=source/'COMMUNITY.OBD' if source.is_dir() else source
    out=Path(out);out.mkdir(parents=True,exist_ok=False)
    with zipfile.ZipFile(source) as z:entries={n:z.read(n) for n in z.namelist()}
    world=json.loads(entries['WORLD12.JSON'])
    if world['players_initialized'] or world['managers'] or world['loans']:raise ValueError('Master world required')
    def rows(name):return json.loads(gzip.decompress((Path(tables)/(name+'.json.gz')).read_bytes()))
    teams={x['teamid']:x for x in rows('teams')};players={x['playerid']:x for x in rows('players')};names={x['nameid']:x.get('name') or '' for x in rows('playernames')}
    links=rows('leagueteamlinks');rosters=defaultdict(list)
    for x in rows('teamplayerlinks'):
        if x['playerid'] in players:rosters[x['teamid']].append(players[x['playerid']])
    clubs={c['id']:c for c in world['clubs']};english_clubs={norm(c['name']):c for c in world['clubs'] if c['country']==2 and c['kind']==0}
    if norm('Brighton') in english_clubs:english_clubs.setdefault(norm('Brighton & Hove Albion'),english_clubs[norm('Brighton')])
    all_ea={x['teamid'] for x in links if x['leagueid'] in [13,14,60,61]}
    missing=[teams[i]['teamname'] for i in all_ea if norm(teams[i]['teamname']) not in english_clubs]
    if missing:raise ValueError('Missing canonical club identity mappings: '+str(missing))
    by_ea={i:english_clubs[norm(teams[i]['teamname'])] for i in all_ea}
    selected=[];excluded=[];membership={}
    rank=lambda i:(-int(teams[i]['overallrating']),i)
    for slot,league in enumerate([13,14,60,61],5):
        ids=sorted({x['teamid'] for x in links if x['leagueid']==league},key=rank)
        if len(ids)<20:raise ValueError('Source league too short')
        membership[slot]=ids[:20];selected+=ids[:20];excluded+=ids[20:]
    # The fifth, hidden ten-team pool remains the existing structural extension.
    membership[9]=sorted(excluded,key=rank)[:10];selected+=membership[9]
    if len(set(selected))!=90:raise ValueError('Repeated/missing English membership')
    old_ids={s['player'] for c in world['clubs'] if c.get('league') in range(5,10) for s in c['roster'] if s['player'] is not None}
    world['players']=[p for p in world['players'] if p['id'] not in old_ids]
    for c in world['clubs']:
        if c.get('league') in range(5,10):c['league']=None;c['roster']=[{'player':None,'flags':0} for _ in range(24)]
    with zipfile.ZipFile(nation_world) as z:nations=json.loads(z.read('world/nations.json'))['items']
    nationalities={}
    special={'x-eng':'ENGLAND','x-sco':'SCOTLAND','x-wal':'WALES','x-kos':'XK','x-legacy-8':'TR','x-legacy-59':'CI','x-legacy-60':'KR'}
    for n in nations:
        index=n.get('x-legacy',{}).get('nationality_index')
        if index is not None:nationalities[special.get(n['id'].split(':')[1],n['id'].split(':')[1].upper())]=index
    ea_nations={n['nationid']:n for n in rows('nations')}
    records=[];taken=set();fictional=[]
    def short_name(p):
        first=names.get(p.get('firstnameid'),'?');last=names.get(p.get('lastnameid'),'') or names.get(p.get('commonnameid'),'')
        value=(first[:1]+'.'+last).strip('.') if last else first
        value=''.join(c for c in unicodedata.normalize('NFKD',value) if not unicodedata.combining(c)).encode('ascii','replace').decode().upper()
        return value[:22]
    def position(p):
        i=int(p.get('preferredposition1',25));return 0 if i==0 else 1 if i<=8 else 2 if i<=19 else 3
    for slot,ids in membership.items():
        world['leagues'][slot]['clubs']=[by_ea[i]['id'] for i in ids]
        for tid in ids:
            c=by_ea[tid];c['league']=slot
            available=sorted({p['playerid']:p for p in rosters[tid]}.values(),key=lambda p:(-p['overallrating'],p['playerid']))
            picked=[]
            for pos,count in [(0,2),(1,8),(2,8),(3,6)]:picked += [p for p in available if position(p)==pos][:count]
            picked_ids={p['playerid'] for p in picked}
            picked += [p for p in available if p['playerid'] not in picked_ids][:24-len(picked)]
            if len(picked)<11:raise ValueError('Source roster shorter than eleven: '+teams[tid]['teamname'])
            picked=sorted(picked,key=lambda p:(position(p),-p['overallrating'],p['playerid']))
            c['roster']=[]
            for row,p in enumerate(picked):
                pid=0x10000000+p['playerid']
                if pid in taken:raise ValueError('Source player assigned to two clubs')
                taken.add(pid)
                nation=ea_nations.get(p.get('nationality'),{});key=nation.get('isocountrycode','').upper()
                if nation.get('nationname','').upper() in ['ENGLAND','SCOTLAND','WALES']:key=nation['nationname'].upper()
                nationality=nationalities.get(key)
                fields=[]
                if nationality is None:nationality=2;fields.append('unsupported_nationality_uses_existing_english_compatibility_code')
                # EA serial dates are retained in provenance; Classic ages are generated at career start.
                born=(date(1582,10,15)+timedelta(days=int(p['birthdate']))).isoformat()
                age=max(0,min(31,2026-int(born[:4])-16))
                skill_fields=['positioning','sprintspeed','finishing','shortpassing','standingtackle','freekickaccuracy','crossing']
                skills=sum(min(3,int(p.get(k,0))//25)<<(2*i) for i,k in enumerate(skill_fields))
                strength=max(1,min(99,int(p['overallrating'])));value=max(1,min(127,round(strength*100/106)))
                world['players'].append({'id':pid,'name':short_name(p),'form_word':99|(value<<7)|(value<<14),'skill_word':skills|((nationality&31)<<27),'identity_word':position(p)|(age<<21),'bonus':128 if nationality>=32 else 0,'cards':0})
                c['roster'].append({'player':pid,'flags':1 if row<11 else 2 if row<17 else 0})
                records.append({'player':pid,'ea_player':p['playerid'],'club':c['id'],'ea_team':tid,'name':{'first':names.get(p.get('firstnameid')),'last':names.get(p.get('lastnameid'))},'born':born,'source_nation':nation.get('nationname'),'source_overall':p['overallrating'],'fictional_fields':fields,'attribute_mapping':'classic-scaled-overall-and-quartile-skills; existing career generator retained'})
                if fields:fictional.append(pid)
            c['roster'] += [{'player':None,'flags':0} for _ in range(24-len(c['roster']))]
    world['players'].sort(key=lambda p:p['id'])
    report={'format':'extended-english-refresh-1','season':'2026/27','source_bundle_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'source_tables':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in Path(tables).glob('*.json.gz') if p.stem.split('.')[0] in ['teams','players','teamplayerlinks','leagueteamlinks','playernames','nations']},'clubs':[{'club':by_ea[i]['id'],'source_team':i,'name':teams[i]['teamname'],'source_league':next(x['leagueid'] for x in links if x['teamid']==i and x['leagueid'] in [13,14,60,61]),'runtime_league':by_ea[i]['league']} for i in selected],'players':records,'nationality_compatibility_fallbacks':fictional,'limits':['Source EA membership is game data, not independently verified real-world registration.','Twenty-team playable tiers and the hidden ten-team reserve pool are retained; omitted lower-tier clubs populate the hidden pool.','Other countries and old saved worlds are not rewritten.','Unsupported Classic nationality codes are explicitly marked; source nationality is retained here.','The existing Classic career generator still generates ages and strengths at career start.']}
    for name in ['8.VGA','9.VGA','12.VGA']:entries['EN/KB/'+name]=(Path(english)/'KB'/name).read_bytes()
    text=Path(english)/'TEXTD.BLK';text=text if text.exists() else Path(english)/'TEXTE.BLK';entries['EN/TEXTD.BLK']=text.read_bytes()
    report['crest_imports']=[]
    if crests is not None:
        from PIL import Image
        sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'poc'))
        from prepare_international import encode_vga
        raw=entries['M3PC.COL'];palette=[((value&63)*255+31)//63 for value in raw[:768]]
        pal=Image.new('P',(1,1));pal.putpalette(palette)
        definitions={c['id']:c for c in world['crests']}
        for tid in selected:
            path=Path(crests)/'light'/(str(tid)+'.png')
            if not path.exists():path=Path(crests)/'dark'/(str(tid)+'.png')
            if not path.exists():report['crest_imports'].append({'ea_team':tid,'status':'retained-previous-crest'});continue
            art=Image.open(path).convert('RGBA');bbox=art.getchannel('A').getbbox()
            if bbox:art=art.crop(bbox)
            art.thumbnail((34,34),Image.Resampling.LANCZOS)
            bg=Image.new('RGBA',(38,38),tuple(palette[94*3:94*3+3])+ (255,));bg.alpha_composite(art,((38-art.width)//2,(38-art.height)//2))
            pixels=bg.convert('RGB').quantize(palette=pal,dither=Image.Dither.NONE).tobytes();page=bytearray([94]*(304*38))
            for y in range(38):page[y*304:y*304+38]=pixels[y*38:y*38+38]
            resource='EN/W/FC'+str(tid)+'.VGA';entries[resource]=encode_vga(304,38,page);c=by_ea[tid];definitions[c['crest']].update(resource=resource,cell=0)
            report['crest_imports'].append({'ea_team':tid,'status':'ea-fc27-source','source_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'resource':resource})
    entries['ENGLISH-REFRESH.json']=(json.dumps(report,ensure_ascii=False,indent=2)+'\n').encode()
    entries['WORLD12.JSON']=(json.dumps(world,ensure_ascii=False,sort_keys=True,separators=(',',':'))+'\n').encode()
    if 'IMPORT-REPORT.json' in entries:
        original_report=json.loads(entries['IMPORT-REPORT.json'])
        original_report['english_refresh_revision']={
            'report':'ENGLISH-REFRESH.json',
            'report_sha256':hashlib.sha256(entries['ENGLISH-REFRESH.json']).hexdigest(),
            'master_sha256':hashlib.sha256(entries['WORLD12.JSON']).hexdigest(),
            'note':'Current English membership, squads and listed crests supersede the historical initial import; all other historical entries remain provenance.'}
        entries['IMPORT-REPORT.json']=(json.dumps(original_report,ensure_ascii=False,indent=2)+'\n').encode()
    package=json.loads(entries.pop('PACKAGE.json'));package['world_schema']=world['version'];package['data_revision']='english-fc27-2026-27-1';package['files']={n:hashlib.sha256(b).hexdigest() for n,b in sorted(entries.items())};entries['PACKAGE.json']=(json.dumps(package,indent=2)+'\n').encode()
    if 'IMPORT-REPORT.json' in entries:(out/'IMPORT-REPORT.json').write_bytes(entries['IMPORT-REPORT.json'])
    (out/'WORLD12.JSON').write_bytes(entries['WORLD12.JSON']);(out/'ENGLISH-REFRESH.json').write_bytes(entries['ENGLISH-REFRESH.json'])
    with zipfile.ZipFile(out/'COMMUNITY.OBD','w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        for n,b in sorted(entries.items()):z.writestr(n,b)
    return {'clubs':len(selected),'players':len(records),'compatibility_nationalities':len(fictional),'output':str(out)}

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for k in ['source','tables','nation-world','english','out']:p.add_argument('--'+k,type=Path,required=True)
    p.add_argument('--crests',type=Path);a=p.parse_args();print(json.dumps(refresh(a.source,a.tables,a.nation_world,a.english,a.out,a.crests)))
