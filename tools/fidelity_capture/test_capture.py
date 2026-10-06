import unittest,struct,json,zlib,tempfile
from pathlib import Path
from inspect_capture import inspect,decode_record,DEFAULT_SCHEMA
class CaptureTests(unittest.TestCase):
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
