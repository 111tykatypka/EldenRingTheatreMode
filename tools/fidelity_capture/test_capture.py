import unittest,struct,json,zlib,tempfile
from pathlib import Path
from inspect_capture import inspect,decode_record,DEFAULT_SCHEMA
class CaptureTests(unittest.TestCase):
 def test_replay_comparisons_are_scoped_and_honest(self):
  from analyze_animation_replay import analyze
  log="REPLAY_ANIMATION_REQUEST session=2 replay_ns=0 id=4\nREPLAY_ANIMATION_COMPARE session=2 replay_ns=100 requested_id=4 observed_id=4 id_match=true phase_error=Some(-0.05)\nREPLAY_ANIMATION_COMPARE session=2 replay_ns=200 requested_id=5 observed_id=4 id_match=false phase_error=Some(9.0)\nREPLAY_ANIMATION_REQUEST session=3 replay_ns=0 id=7"
  group=analyze(log,2)['sessions'][2];self.assertEqual(group['requests'],1);self.assertEqual(group['id_match_fraction'],.5);self.assertEqual(group['max_abs_phase_error_s'],.05);self.assertEqual(len(group['mismatches']),1)

 def test_schema_unique_and_bounded(self):
  s=json.loads(DEFAULT_SCHEMA.read_text());self.assertEqual(len(s['tracks']),9)
  for t in s['tracks']:
   self.assertEqual(len({f['name']for f in t['fields']}),len(t['fields']))
   self.assertEqual(t['record_bytes'],32+4*t['mask_words']+4*t['words'])
 def test_nan_and_unknown_retained(self):
  s=json.loads(DEFAULT_SCHEMA.read_text());t=s['tracks'][0];b=bytearray(t['record_bytes']);struct.pack_into('<QQQQ',b,0,0,99,4,2);struct.pack_into('<I',b,32,15);struct.pack_into('<I',b,32+t['mask_words']*4,0x7fc01234)
  r=decode_record(b,t,s);f=r['fields']['position'];self.assertTrue(f['available']);self.assertEqual(f['raw_hex'][0],'7FC01234');self.assertEqual(f['value'][0],'nan');self.assertEqual(r['sequence'],4)
 def test_unavailable_distinct_from_zero(self):
  s=json.loads(DEFAULT_SCHEMA.read_text());t=s['tracks'][0];r=decode_record(bytes(t['record_bytes']),t,s);self.assertFalse(r['fields']['position']['available']);self.assertIsNone(r['fields']['position']['value'])
 def test_truncated_rejected(self):
  with tempfile.TemporaryDirectory() as d:
   p=Path(d)/'broken.erplay';p.write_bytes(b'ERPLAY03');self.assertRaises(ValueError,inspect,p)
if __name__=='__main__':unittest.main()
