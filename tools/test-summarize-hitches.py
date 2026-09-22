"""Classification regressions; run with Python 3, no game assets needed."""
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('hitches', Path(__file__).with_name('summarize-hitches.py'))
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

def frame(wall=2100, state='gameplay', start='gameplay'):
    mode = '' if state is None else f' state={state} start_state={start}'
    return f'HITCH_FRAME wall={wall} frame=10 total_ms=40 events_ms=25 session_ms=10 render_ms=5{mode} map=test\n'

class Classification(unittest.TestCase):
    def summarize(self, text):
        return module.summarize('HITCH_MAP wall=100 phase=ready map=test\n' + text)

    def test_gameplay_operation(self):
        result = self.summarize('HITCH_OP wall=2080 category=command ms=15 asset=saveGame\n' + frame())
        self.assertEqual(result['gameplay_hitches'], 1)
        self.assertEqual(result['worst_frames'][0]['operations'][0]['asset'], 'saveGame')

    def test_menu_and_transitions(self):
        result = self.summarize(frame(state='menu', start='menu') + frame(2200, 'gameplay', 'menu'))
        self.assertEqual(result['gameplay_hitches'], 0)
        self.assertEqual(result['menu_hitches'], 1)
        self.assertEqual(result['transition_hitches'], 1)

    def test_transient_menu_in_one_frame(self):
        result = self.summarize('HITCH_UI wall=2070 state=menu\nHITCH_UI wall=2080 state=gameplay\n' + frame())
        self.assertEqual(result['transition_hitches'], 1)

    def test_old_log_is_unknown(self):
        result = self.summarize(frame(state=None))
        self.assertEqual(result['gameplay_hitches'], 0)
        self.assertEqual(result['unclassified_hitches'], 1)
        self.assertEqual(result['worst_ms'], 40)
        self.assertEqual(result['worst_gameplay_ms'], 0)

    def test_loading_and_settling_excluded(self):
        result = self.summarize(frame(wall=200) + frame(state='gameplay', start='loading'))
        self.assertEqual(result['worst_frames'], [])

if __name__ == '__main__':
    unittest.main()
