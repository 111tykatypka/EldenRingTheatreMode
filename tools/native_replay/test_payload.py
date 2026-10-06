import unittest,struct
from payload_codec import DecodeError,decode_payload,decode_node,packed_position

class PayloadTests(unittest.TestCase):
 def test_empty_sparse_groups(self):
  self.assertEqual(decode_payload(b'\0\0')['consumed_bytes'],2)
 def test_quantized_signed_position(self):
  x,y,z=-123,-456,789
  a=(x&0xfffff)|(((y&0x1ffff)>>5)<<20)
  b=(y&31)|((z&0xfffff)<<5)|(64<<25)
  self.assertEqual(packed_position(a,b),[x*.02,y*.04,z*.02])
 def test_observed_mask_structure(self):
  data=b'\x34'+struct.pack('<IfiIIIi',0,.125,123,0,64<<25,2048,-1)+b'\0'+bytes(24)+b'\x0c\0'+struct.pack('<I',9)
  r=decode_payload(data)
  self.assertEqual(r['groups']['2']['duration_seconds'],.125)
  self.assertEqual(r['groups']['2']['control_yaw_radians'],0)
  self.assertEqual(r['events']['3']['raw_u32'],9)
  self.assertEqual(r['consumed_bytes'],len(data))
 def test_nested_variable_entries(self):
  r=decode_payload(b'\x10\x01\x01ABC\0')
  self.assertEqual(r['groups']['4']['groups']['0']['elements_hex'],['414243'])
 def test_event_arrays(self):
  r=decode_payload(b'\0\x03\x01ABCD\x01EF')
  self.assertEqual(r['events']['0']['count'],1)
  self.assertEqual(r['events']['1']['elements_hex'],['4546'])
 def test_truncated_unknown_trailing(self):
  for data in [b'',b'\x40\0',b'\0\x10',b'\0\0x',b'\x10\x01\xff',b'\0\x01\xff']:
   with self.subTest(data=data),self.assertRaises(DecodeError):decode_payload(data)
 def test_nonfinite(self):
  with self.assertRaises(DecodeError):decode_payload(b'\x04'+struct.pack('<IfiIIIi',0,float('nan'),0,0,0,0,0)+b'\0')
 def test_node_size_and_length(self):
  with self.assertRaises(DecodeError):decode_node(bytes(10))
  node=bytearray(0x248);struct.pack_into('<I',node,0,257)
  with self.assertRaises(DecodeError):decode_node(node)
 def test_empty_node(self):
  self.assertIsNone(decode_node(bytes(0x248))['primary'])

if __name__=='__main__':unittest.main()
