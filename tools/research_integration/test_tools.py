import json, unittest
from analyze_runtime_trace import analyze
from integrate_sources import find_pattern

class Tools(unittest.TestCase):
    def test_wildcards_and_ambiguity(self):
        self.assertEqual(find_pattern(bytes.fromhex('aa 00 bb aa 11 bb'),'AA ?? BB'),[0,3])
        self.assertEqual(find_pattern(bytes.fromhex('c3 00 00 00 00 00 00 57'),'C3 ?? ?? ???????? 57'),[0])
        with self.assertRaises(ValueError):find_pattern(b'abc','?? ?')
    def test_differential_trace(self):
        base={'schema':1,'run':1,'handle':'1','has_transform':True,'orientation':[0,0,0,1]}
        rows=[dict(base,stage='player_write_after',time_ns=100,position=[1,2,3],target={'position':[1,2,3]}),
              dict(base,stage='player_post_physics_before',time_ns=200,position=[1,2.5,3]),
              dict(base,stage='actor_lookup_failed',time_ns=300,has_transform=False)]
        result=analyze(map(json.dumps,rows));self.assertEqual(result['max_immediate_position_error'],0)
        self.assertEqual(result['max_next_callback_position_error'],.5);self.assertEqual(result['failure_counts']['actor_lookup_failed'],1)
    def test_corruption_is_not_a_native_sample(self):
        result=analyze(['{','[]',json.dumps({'schema':1,'stage':'actor_ipc_received','has_transform':False})])
        self.assertEqual(result['malformed_rows'],2);self.assertIsNone(result['max_next_callback_position_error'])

if __name__=='__main__':unittest.main()
