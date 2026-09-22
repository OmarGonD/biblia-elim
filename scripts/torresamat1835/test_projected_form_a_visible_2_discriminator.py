#!/usr/bin/env python3
"""Direct task-142 contracts: real corpus, adversarial leakage and invariants."""
import ast
import copy
import hashlib
import inspect
import json
import os
import subprocess
import tempfile
import time
from collections import Counter
from pathlib import Path
from unittest.mock import patch

import audit_volume
import projected_form_a_visible_2_discriminator as diagnostic

ROOT=diagnostic.ROOT
HISTORICAL=('projected_form_a_facsimile.json','remaining_glyph_reprioritization.json',
            'zero_anchor_io_recovery_validation.json','no_trusted_band_discriminator.json',
            'projected_rejection_audit.json','standalone_glyph_facsimile.json','remaining_glyph_inventory.json')


def raises_value_error(fn):
    try:
        fn()
    except ValueError:
        return
    raise AssertionError('Forbidden discriminator input was accepted')


def test_leakage(frozen,truth,rows,data):
    # Immutable nested vectors, with labels physically absent, not merely ignored.
    for meta,vector in frozen:
        assert set(vector)==set(diagnostic.SCHEMA)
        assert not set(diagnostic.LABELS)&set(vector)
        try:
            vector['visible_printed_value']=2
        except TypeError:
            pass
        else:
            raise AssertionError('Feature vector is mutable')
    mutated=copy.deepcopy(truth)
    for row in mutated:
        row['visible_printed_value']=999
        row['facsimile_review_class']='ADVERSARIAL_LABEL'
    joined=diagnostic.join_labels(frozen,mutated)
    assert [r['runtime_features'] for r in joined]==[r['runtime_features'] for r in rows]
    for rule in data['candidate_rules']:
        assert [diagnostic.accepts(rule,r['runtime_features']) for r in joined]==[
            diagnostic.accepts(rule,r['runtime_features']) for r in rows]
    # Closed feature vocabulary rejects identifiers, labels and inferred refs,
    # including new unknown aliases instead of relying only on a denylist.
    forbidden=('visible_printed_value','review_class','facsimile_review_class',
               'occurrence_id','stable_occurrence_id','expected_ref','expected_verse',
               'VerseRef','page','book','chapter','previous_verse','next_verse',
               'previous_plus_one','next_minus_one','positive_id','unknown_alias')
    for feature in forbidden:
        rule=copy.deepcopy(data['selected_rule'])
        rule['conditions'][0]['feature']=feature
        raises_value_error(lambda:diagnostic.accepts(rule,{}))
    rule=copy.deepcopy(data['selected_rule']);rule['conditions'][0]['operator']='allowlist'
    raises_value_error(lambda:diagnostic.validate_rule(rule))
    # Extractor call graph has no artifact/label parameter or artifact reader.
    for function in (diagnostic.line_features,diagnostic.extract):
        code=inspect.getsource(function)
        for bad in ('visible_printed_value','review_class','expected_verse','previous_verse','next_verse','read_text','json.load'):
            assert bad not in code,(function.__name__,bad)
    build_source=inspect.getsource(diagnostic.build)
    assert build_source.index('frozen=extract(')<build_source.index("truth=json.loads(")<build_source.index('rows=join_labels(')
    for function in (diagnostic.line_features,diagnostic.accepts,diagnostic.rules):
        code=inspect.getsource(function)
        assert not any(t in code for t in ('occurrence_id',"['page']","['book']","['chapter']",'previous + 1','next - 1'))


def test_contract():
    os.chdir(ROOT)
    start=time.perf_counter()
    # Explicit frozen provenance, independent of whatever HEAD is later.
    assert diagnostic.BASELINE=='edef6d28d0e647ca97aa8ac91c3c68284137711c'
    for filename in HISTORICAL:
        path='data/torresamat1835/'+filename
        baseline=subprocess.check_output(['git','show',diagnostic.BASELINE+':'+path],cwd=ROOT)
        assert (ROOT/path).read_bytes()==baseline,filename
    for filename in ('page_parser.py','compound_glyphs.py','parser.py','layout.py','verse_gaps.py'):
        path='scripts/torresamat1835/'+filename
        assert (ROOT/path).read_bytes()==subprocess.check_output(['git','show',diagnostic.BASELINE+':'+path],cwd=ROOT)

    edition,audit=audit_volume.audit(str(diagnostic.XML),volume='3',witness='ia-lasagradabiblia01unkngoog',book='Ps')
    before_refs=copy.deepcopy(audit_volume._reference_map(edition))
    before_owners=copy.deepcopy(audit_volume._owner_map(before_refs))
    before_audit=copy.deepcopy(audit)
    timings={}
    data=diagnostic.build(audit,diagnostic.BASELINE,timings=timings)
    assert audit==before_audit
    assert audit_volume._reference_map(edition)==before_refs
    assert audit_volume._owner_map(audit_volume._reference_map(edition))==before_owners
    assert not any(len(owners)>1 for owners in before_owners.values())
    owners_hash=hashlib.sha256(diagnostic.encode(before_owners).encode()).hexdigest()
    assert data['schema_version']==1
    assert data['provenance']['baseline_commit']==diagnostic.BASELINE
    assert data['runtime_invariants']==diagnostic.runtime_snapshot(audit)|{'ownership_unchanged':True}
    runtime=data['runtime_invariants']
    for key,value in {'verse_refs':3849,'physical_gaps':3254,'glyph_gaps':1309,'chapters':337,
                      'unresolved_chapter_claims':0,'canonical_chapter_gaps':0,'duplicate_refs':0,
                      'out_of_order_refs':0,'outside_canon':0,'ocr_blocks':57700,'block_loss':0,'dual_ownership':0}.items():
        assert runtime[key]==value,(key,runtime[key])
    assert runtime['task128']=={'markers':183,'refs':177,'moves':1355}
    assert runtime['task131']=={'markers':276,'refs':276,'moves':1900}
    assert runtime['task139']['ownership_moves']==27 and runtime['task139']['new_refs']==1
    assert runtime['GLUED_FRAME']=='CLOSED_UNSAFE'
    assert runtime['new_recovery'] is runtime['runtime_image_reads'] is False

    rows=data['occurrence_features']
    assert len(rows)==len({r['metadata']['occurrence_id'] for r in rows})==260
    assert Counter(diagnostic.group(r) for r in rows)==Counter(positives=77,other_marker_controls=161,non_marker_controls=22)
    assert data['ground_truth_summary']['controls']==183
    truth=json.loads((diagnostic.DATA/'projected_form_a_facsimile.json').read_text())
    assert data['ground_truth_summary']['visible_value_counts']==truth['visible_printed_value_counts']
    assert data['ground_truth_summary']['review_classes']==truth['class_counts']
    assert all(r['runtime_features']['physical_token_count']>1 for r in rows)
    candidates=diagnostic.family(audit['verse_segmentation_audit']['gaps'])
    # Trap *any* label/history file access while independently extracting.
    with patch.object(Path,'read_text',side_effect=AssertionError('Label/history read during extraction')):
        frozen=diagnostic.extract(candidates,diagnostic.XML)
    test_leakage(frozen,truth['occurrence_reviews'],rows,data)
    # Changing all forbidden sequence/identity context leaves each vector intact.
    renamed=[dict(c,occurrence_id='opaque-'+str(i),book='opaque',chapter=-999) for i,c in enumerate(candidates)]
    with patch.object(Path,'read_text',side_effect=AssertionError('Label read')):
        independent=diagnostic.extract(renamed,diagnostic.XML)
    assert [dict(f) for m,f in frozen]==[dict(f) for m,f in independent]

    assert len(data['candidate_rules'])==len(data['confusion_results'])==298
    for rule,result in zip(data['candidate_rules'],data['confusion_results']):
        assert rule['name']==result['rule_name']
        diagnostic.validate_rule(rule)
        decisions=[diagnostic.accepts(rule,r['runtime_features']) for r in rows]
        expected=diagnostic.confusion(rows,decisions)
        assert {k:result[k] for k in expected}==expected
        assert result['TP']+result['FN']==77 and result['FP']+result['TN']==183
        population=data['evaluation_populations'][result['evaluation_population']]
        assert len(population['false_positive_ids'])==result['FP']
        assert len(population['false_negative_ids'])==result['FN']
        assert population['accepted_by_visible_value']==diagnostic.buckets(rows,decisions)
        assert population['generalization']==diagnostic.grouped(rows,decisions)
    assert data['result_status']=='NARROWER_SAFE_SUBFAMILY_FOUND'
    assert data['selected_confusion']=={'TP':53,'FN':24,'FP':0,'TN':183}
    assert data['selected_positive_subset']['occurrences']==53
    assert data['selected_positive_subset']['distinct_source_blocks']==47
    assert len(data['overfitting_analysis']['selected_books'])==6
    for value,bucket in data['accepted_by_visible_value'].items():
        assert bucket['accepted']+bucket['rejected']==bucket['population']
        assert bucket['accepted']==(53 if value=='2' else 0)
    assert len(data['near_miss_analysis']['controls'])==183
    assert all(r['failed_conditions'] for r in data['near_miss_analysis']['controls'])
    assert len(data['false_negative_analysis']['occurrences'])==24
    assert isinstance(data['task143_recommendation'],dict)
    assert data['task143_recommendation']['task_id'].endswith('-143')
    assert data['audit_summary']==audit['verse_segmentation_audit']['projected_form_a_visible_2_discriminator']

    # Exact output, second generation, overwrite/idempotence and changed cwd.
    rendered=diagnostic.encode(data).encode()
    assert rendered==diagnostic.ART.read_bytes()
    with tempfile.TemporaryDirectory() as td:
        out=Path(td)/'artifact.json'
        out.write_bytes(rendered)
        second=diagnostic.build(audit,diagnostic.BASELINE)
        out.write_text(diagnostic.encode(second),encoding='utf-8')
        assert out.read_bytes()==rendered
    raises_value_error(lambda:diagnostic.build(audit,'0'*40))
    module_source=Path(diagnostic.__file__).read_text()
    tree=ast.parse(module_source)
    imports=[a.name for n in ast.walk(tree) if isinstance(n,(ast.Import,ast.ImportFrom)) for a in n.names]
    assert not any(x in imports for x in ('torch','tensorflow','sklearn','subprocess','page_parser'))
    assert 'rev-parse' not in module_source and 'datetime' not in module_source
    assert not any(isinstance(n,ast.Call) and isinstance(n.func,ast.Attribute) and n.func.attr in ('recover','fit','predict') for n in ast.walk(tree))
    integration_start=time.perf_counter()
    for _ in range(10):
        loaded=json.loads(diagnostic.ART.read_text())['audit_summary']
        assert loaded==data['audit_summary']
    timings['audit_integration_seconds']=(time.perf_counter()-integration_start)/10
    timings['total_test_seconds']=time.perf_counter()-start
    print(json.dumps({'timings':timings,'ownership_before_sha256':owners_hash,
                      'ownership_after_sha256':owners_hash,'owned_blocks':len(before_owners)},sort_keys=True))


if __name__=='__main__':
    test_contract()
    print('ok')
