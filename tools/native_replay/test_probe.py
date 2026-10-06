import unittest,tempfile,json
from pathlib import Path
from analyze_probe import analyze
class ProbeTests(unittest.TestCase):
 def test_changes_and_lengths(self):
  with tempfile.TemporaryDirectory()as d:
   p=Path(d)/'capture.jsonl';b=bytearray(0x248);b[0]=1
   r={'schema':1,'prefix':'REPLAY_FRAME','address':'0x10000','time_ns':1,'source_drops':0,'message':'role=active_head','raw_hex':b.hex()};p.write_text(json.dumps(r)+'\n',encoding='utf-8');b[4]=7;r['raw_hex']=b.hex();r['time_ns']=2
   with p.open('a',encoding='utf-8')as f:f.write(json.dumps(r)+'\n')
   a=analyze(p);self.assertFalse(a['errors']);self.assertEqual(a['changed_words'][0]['offset'],'0x4');self.assertTrue(a['nodes'][0]['lengths_within_static_capacity'])
 def test_torn_line_reported(self):
  with tempfile.TemporaryDirectory()as d:
   p=Path(d)/'capture.jsonl';p.write_text('{broken',encoding='utf-8');a=analyze(p);self.assertEqual(len(a['errors']),1)
if __name__=='__main__':unittest.main()
