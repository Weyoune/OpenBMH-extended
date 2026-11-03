# SPDX-License-Identifier: GPL-3.0-only
"""Authored synthetic inputs only: no real clubs, players or artwork."""
import gzip, hashlib, json, tempfile, unittest, zipfile
from pathlib import Path
from refresh_english import refresh

class RefreshTests(unittest.TestCase):
    def test_membership_rosters_assets_and_source_preservation(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);tables=root/'tables';tables.mkdir();english=root/'english';(english/'KB').mkdir(parents=True)
            for name in ['8.VGA','9.VGA','12.VGA']:(english/'KB'/name).write_bytes(b'authored-synthetic-resource')
            (english/'TEXTE.BLK').write_bytes(b'authored-synthetic-text')
            clubs=[{'id':i,'name':'TEAM '+str(i),'country':2,'kind':0,'league':None,'crest':i,'roster':[{'player':None,'flags':0} for _ in range(24)]} for i in range(1,93)]
            world={'players_initialized':False,'managers':[],'loans':[],'clubs':clubs,'players':[],'leagues':[{'league':i,'clubs':[]} for i in range(13)],'version':42,'crests':[{'id':i,'resource':'W/SYNTHETIC.VGA','cell':0} for i in range(1,93)]}
            for slot,ids in zip(range(5,10),[range(3,23),range(23,43),range(43,63),range(63,83),range(83,93)]):
                world['leagues'][slot]['clubs']=list(ids)
                for cid in ids:
                    clubs[cid-1]['league']=slot
                    for n in range(24):
                        pid=cid*100+n;world['players'].append({'id':pid,'name':'BEFORE'});clubs[cid-1]['roster'][n]={'player':pid,'flags':0}
            extra=[{'id':1000000+i,'name':'UNCHANGED'} for i in range(54)];world['players']+=extra
            teams=[{'teamid':i,'teamname':'Team '+str(i),'overallrating':80-i%10} for i in range(1,93)]
            league_links=[{'teamid':i,'leagueid':13 if i<=20 else 14 if i<=44 else 60 if i<=68 else 61} for i in range(1,93)]
            players=[];links=[]
            for tid in range(1,93):
                for n in range(24):
                    pid=tid*100+n;players.append({'playerid':pid,'firstnameid':1,'lastnameid':2,'preferredposition1':0 if n<2 else 4 if n<10 else 14 if n<18 else 25,'nationality':14,'birthdate':150000,'overallrating':75});links.append({'teamid':tid,'playerid':pid})
            for name,value in {'teams':teams,'players':players,'leagueteamlinks':league_links,'teamplayerlinks':links,'playernames':[{'nameid':1,'name':'Alex'},{'nameid':2,'name':'Example'}],'nations':[{'nationid':14,'nationname':'England','isocountrycode':'GB'}]}.items():(tables/(name+'.json.gz')).write_bytes(gzip.compress(json.dumps(value).encode()))
            legacy=root/'nations.obw'
            with zipfile.ZipFile(legacy,'w') as z:z.writestr('world/nations.json',json.dumps({'items':[{'id':'nation:x-eng','x-legacy':{'nationality_index':2}}]}))
            source=root/'source.obd'
            with zipfile.ZipFile(source,'w') as z:
                z.writestr('WORLD12.JSON',json.dumps(world));z.writestr('PACKAGE.json',json.dumps({'files':{}}));z.writestr('M3PC.COL',bytes([0]*768));z.writestr('W/SYNTHETIC.VGA',b'keep-this-exact')
            original=source.read_bytes();result=refresh(source,tables,legacy,english,root/'output');self.assertEqual(result['clubs'],90);self.assertEqual(result['players'],2160);self.assertEqual(source.read_bytes(),original)
            with zipfile.ZipFile(root/'output/COMMUNITY.OBD') as z:
                updated=json.loads(z.read('WORLD12.JSON'));self.assertEqual(set(updated['leagues'][5]['clubs']),set(range(1,21)));self.assertEqual([len(updated['leagues'][i]['clubs']) for i in range(5,10)],[20,20,20,20,10]);self.assertEqual([p for p in updated['players'] if p['id']<0x10000000],extra);self.assertEqual(len({p['id'] for p in updated['players']}),2214)
                for p in updated['players']:
                    if p['id']>=0x10000000:self.assertLessEqual(p['form_word']&127,99);self.assertEqual(p['skill_word']>>27,2)
                self.assertEqual(z.read('W/SYNTHETIC.VGA'),b'keep-this-exact');self.assertEqual(z.read('EN/TEXTD.BLK'),b'authored-synthetic-text');metadata=json.loads(z.read('PACKAGE.json'))
                for name,digest in metadata['files'].items():self.assertEqual(hashlib.sha256(z.read(name)).hexdigest(),digest)

if __name__=='__main__':unittest.main()
