#!/usr/bin/env python3
import json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
def test_artifact():
    d=json.loads((ROOT/'data/torresamat1835/standalone_glyph_facsimile.json').read_text())
    assert d['inventory_accounting']['standalone_population']==1310
    assert d['inventory_accounting']['glyph_gaps']==1310
    assert d['provenance']['source_sha256']==d['provenance']['expected_source_sha256']
    forms={f['exact_ocr_form'] for f in d['forms']}
    assert {'y','á','a','S'}<=forms
    assert d['task_136_target']=='task-128 rejected-projection occurrence audit'
    assert len(d['batch_135_reviews']) == 8
    assert d['rules']['expected_verse_used'] is False
    assert d['projection_taxonomy']['PROJECTED_TOKEN_FROM_MULTI_TOKEN_LINE'] == 1304
    assert d['projection_taxonomy']['TRUE_SINGLE_TOKEN_LINE'] == 6
    assert d['prior_reviews']['records_examined'] == 496
    assert len(d['all_true_single_occurrences']) == 6
    assert sum(d['task128_overlap'].values()) == 1304
    assert d['task128_overlap']['TASK128_REJECTED_EXACT'] == 23
if __name__=='__main__': test_artifact(); print('ok')
