#!/usr/bin/env python3
"""Task 142: source-only feature freezing followed by diagnostic label joining.

No parser recovery is called or changed here. Identity is carried separately
for reconciliation; predicates receive only a closed runtime-feature mapping.
"""
import argparse
import hashlib
import itertools
import json
import re
import statistics
import time
from collections import Counter, defaultdict
from pathlib import Path
from types import MappingProxyType

import compound_glyphs
import layout
import parser as classifier
import source_ocr

ROOT = Path(__file__).resolve().parents[2]
DATA = ROOT / 'data/torresamat1835'
XML = ROOT / 'build/torresamat1835-cache/lasagradabiblia01unkngoog_djvu.xml'
ART = DATA / 'projected_form_a_visible_2_discriminator.json'
BASELINE = 'edef6d28d0e647ca97aa8ac91c3c68284137711c'
LABELS = ('review_class', 'visible_printed_value')
SCHEMA = {
    'physical_token_count': 'Number of WORD elements on the physical source line.',
    'token_position': 'First of the first three WORDs whose safe_outer_trim is exactly a; null if unavailable.',
    'raw_token': 'Unmodified OCR word covering the diagnostic a, including glued punctuation.',
    'first_token': 'Unmodified first physical OCR word; distinct from projected token.',
    'neighbors': 'Next three physical OCR words, unmodified, in order.',
    'preceding_tokens': 'Physical OCR words before the projected token.',
    'column': 'layout.split_columns source column, independently recomputed.',
    'zone': 'layout.split_columns source zone, independently recomputed; not a visual label.',
    'token_bbox': 'Projected source WORD bbox, including glued frame; never image-segmented.',
    'line_bbox': 'Physical source LINE bbox.',
    'token_width': 'Token bbox right minus left, pixels.',
    'token_height': 'Token bbox bottom minus top, pixels.',
    'line_width': 'Line bbox right minus left, pixels.',
    'line_height': 'Line bbox bottom minus top, pixels.',
    'token_aspect': 'Token width / height.',
    'relative_x': 'Token left / page width; page identity not included.',
    'relative_y': 'Token top / page height; page identity not included.',
    'next_gap': 'Next token left minus candidate right, pixels.',
    'baseline_delta': 'Candidate bottom minus next token bottom, pixels.',
    'next_letter_count': 'Unicode alphabetic characters in immediate next OCR token.',
    'next_initial_upper': 'Immediate next OCR token starts with an uppercase letter.',
    'next_is_word': 'Immediate next OCR token contains at least two alphabetic characters.',
    'next_three_are_words': 'All three following tokens exist and contain at least two alphabetic characters.',
    'unframed_start': 'First physical token is exactly a, no skipped or glued punctuation.',
    'trusted_anchor_count': 'Same page/column/body ordinary numeric anchors per compound_glyphs.',
    'band_center': 'Median anchor left edge; null with fewer than MIN_BAND_MARKERS.',
    'digit_width': 'Median anchor width per OCR digit; null without trusted band.',
    'band_tolerance': 'Existing max(BAND_FLOOR_PX, digit_width * BAND_WIDTH_FRACTION).',
    'band_delta': 'Candidate left minus band center; null without band.',
    'in_band': 'Absolute band_delta <= existing band_tolerance; false without band.',
    'width_in_digits': 'Token width / source-derived digit_width; null without band.',
    'height_over_body': 'Token height / median same-column body LINE height.',
    'anchor_before_delta_y': 'Candidate top minus nearest preceding ordinary anchor top in same page/column/body.',
    'anchor_after_delta_y': 'Nearest following ordinary anchor top minus candidate top in same page/column/body.',
    'bracketed': 'At least one ordinary source anchor geometrically above and below in same page/column/body.',
    'nearest_before_delta_x': 'Candidate left minus preceding same-page anchor left.',
    'nearest_after_delta_x': 'Following same-page anchor left minus candidate left.',
    'local_repeated_start': 'Adjacent same-column source line starts with the identical raw first token.',
}


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def encode(value):
    return json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(',', ':')) + '\n'


def family(gaps):
    """Current parser family before any historical/facsimile data is read."""
    return sorted(({'occurrence_id': f"{g['swallowed_block']}::{g['key']}",
                    'block': g['swallowed_block'], 'book': g['book'], 'chapter': g['chapter']}
                   for g in gaps if g.get('swallowed_token') == 'a'
                   and 'lone_glyph_inside_previous_verse' in g.get('signals', [])),
                  key=lambda r: r['occurrence_id'])


def line_features(page, placed, peers):
    """Only SourcePage/PlacedLine objects enter this boundary; no review rows."""
    words = placed.line.words
    index = next((i for i, w in enumerate(words[:3])
                  if classifier.safe_outer_trim(w.text) == 'a'), None)
    if index is None:
        raise ValueError('Cannot locate projected a in the physical OCR prefix')
    word = words[index]
    box = word.bbox
    following = words[index + 1: index + 4]
    neighbor = following[0] if following else None
    body = [p.line for p in peers if p.column == placed.column and p.zone is layout.Zone.BODY]
    band = compound_glyphs.band_of(body)
    anchors = []
    for line in body:
        first, _ = compound_glyphs.marker_tokens(line.words)
        if first is not None and first + 1 < len(line.words) and re.fullmatch(r'\d{1,3}', line.words[first].text):
            anchors.append(line.words[first])
    before = max((a for a in anchors if a.bbox[1] < box[1]), key=lambda a:a.bbox[1], default=None)
    after = min((a for a in anchors if a.bbox[1] > box[1]), key=lambda a:a.bbox[1], default=None)
    same_col = sorted((p.line for p in peers if p.column == placed.column), key=lambda l:l.bbox[1])
    at = same_col.index(placed.line)
    adjacent = same_col[max(0,at-1):at] + same_col[at+1:at+2]
    width, height = box[2]-box[0], box[3]-box[1]
    tolerance = max(compound_glyphs.BAND_FLOOR_PX, band[1]*compound_glyphs.BAND_WIDTH_FRACTION) if band else None
    delta = box[0]-band[0] if band else None
    letters = lambda s:sum(c.isalpha() for c in s)
    return {
        'physical_token_count':len(words), 'token_position':index, 'raw_token':word.text,
        'first_token':words[0].text, 'neighbors':tuple(w.text for w in following),
        'preceding_tokens':tuple(w.text for w in words[:index]), 'column':placed.column.value,
        'zone':placed.zone.value, 'token_bbox':tuple(box), 'line_bbox':tuple(placed.line.bbox),
        'token_width':width, 'token_height':height, 'line_width':placed.line.width,
        'line_height':placed.line.height, 'token_aspect':width/height,
        'relative_x':box[0]/page.width, 'relative_y':box[1]/page.height,
        'next_gap':neighbor.bbox[0]-box[2] if neighbor else None,
        'baseline_delta':box[3]-neighbor.bbox[3] if neighbor else None,
        'next_letter_count':letters(neighbor.text) if neighbor else 0,
        'next_initial_upper':bool(neighbor and neighbor.text[0].isupper()),
        'next_is_word':bool(neighbor and letters(neighbor.text)>=2),
        'next_three_are_words':len(following)==3 and all(letters(w.text)>=2 for w in following),
        'unframed_start':words[0].text=='a', 'trusted_anchor_count':len(anchors),
        'band_center':band[0] if band else None, 'digit_width':band[1] if band else None,
        'band_tolerance':tolerance, 'band_delta':delta,
        'in_band':delta is not None and abs(delta)<=tolerance,
        'width_in_digits':width/band[1] if band and band[1] else None,
        'height_over_body':height/statistics.median(l.height for l in body) if body else None,
        'anchor_before_delta_y':box[1]-before.bbox[1] if before else None,
        'anchor_after_delta_y':after.bbox[1]-box[1] if after else None,
        'bracketed':before is not None and after is not None,
        'nearest_before_delta_x':box[0]-before.bbox[0] if before else None,
        'nearest_after_delta_x':after.bbox[0]-box[0] if after else None,
        'local_repeated_start':any(l.words[0].text==words[0].text for l in adjacent),
    }


def extract(candidates, xml):
    """Freeze EVERY vector before the caller is allowed to join labels."""
    wanted = {c['block'] for c in candidates}
    vectors, pages = {}, {}
    for page in source_ocr.read_pages(str(xml)):
        if not any(f'p{page.scan_page:04d}l{l.index:04d}' in wanted for l in page.lines):
            continue
        peers = layout.split_columns(page)
        for p in peers:
            block = f'p{page.scan_page:04d}l{p.line.index:04d}'
            if block in wanted and len(p.line.words)>1:
                values = line_features(page,p,peers)
                assert set(values)==set(SCHEMA)
                vectors[block] = MappingProxyType(values)
                pages[block] = page.scan_page
    if set(vectors)!=wanted:
        raise ValueError('Current exact-a family is not entirely physically multi-token')
    return tuple((MappingProxyType(dict(c, page=pages[c['block']])), vectors[c['block']]) for c in candidates)


def join_labels(frozen, reviews):
    labels = {r['stable_occurrence_id']:r for r in reviews}
    if {m['occurrence_id'] for m,f in frozen} != set(labels):
        raise ValueError('Current family does not reconcile with task 141')
    return [{'metadata':dict(m), 'runtime_features':dict(f),
             'evaluation_labels':{'review_class':labels[m['occurrence_id']]['facsimile_review_class'],
                                  'visible_printed_value':labels[m['occurrence_id']]['visible_printed_value']}}
            for m,f in frozen]


# Structural hypotheses fixed independently of labels. Numeric boundaries are
# dimensionless typographic hypotheses, not optimized pixel cutoffs.
ATOMS = [
    ('position_zero','token_position','eq',0,'Physical line start.'),
    ('unframed','unframed_start','eq',True,'Exact raw first token; excludes ambiguous frame boxes.'),
    ('following_word','next_is_word','eq',True,'At least two letters distinguishes prose words from split single-glyph numeral tokens.'),
    ('following_capital','next_initial_upper','eq',True,'Sentence-opening OCR morphology; no vocabulary allowlist.'),
    ('three_words','next_three_are_words','eq',True,'Repeated prose morphology over the next three source tokens.'),
    ('marker_band','in_band','eq',True,'Unchanged task-128 source-calibrated marker band and tolerance.'),
    ('bracketed','bracketed','eq',True,'Candidate between measured ordinary marker anchors; no verse values.'),
    ('digit_width','width_in_digits','ge',1.0,'One median printed digit width, calibrated from source anchors.'),
    ('below_two_digits','width_in_digits','lt',1.5,'Midpoint between one and two median digit widths; diagnostic hypothesis only.'),
    ('body_height','height_over_body','ge',1.0,'At least median local body line height; tests body versus smaller apparatus typography.'),
    ('body_zone','zone','eq','body','Existing measured body zone, never facsimile class.'),
    ('right_column','column','eq','right','Existing measured source column, never book/page identity.'),
]


def validate_rule(rule):
    if set(rule)!={'name','conditions','rationale'} or not 1<=len(rule['conditions'])<=3:
        raise ValueError('Only explicit one-to-three-feature conjunctions are supported')
    for c in rule['conditions']:
        if set(c)!={'feature','operator','value'} or c['feature'] not in SCHEMA or c['operator'] not in {'eq','ge','lt'}:
            raise ValueError('Forbidden or unknown feature/operator')


def accepts(rule, features):
    validate_rule(rule)
    for c in rule['conditions']:
        value = features[c['feature']]
        if value is None:
            return False
        op, threshold = c['operator'],c['value']
        if not ((op=='eq' and value==threshold) or (op=='ge' and value>=threshold) or (op=='lt' and value<threshold)):
            return False
    return True


def rules():
    for size in (1,2,3):
        for combo in itertools.combinations(ATOMS,size):
            yield {'name':' AND '.join(a[0] for a in combo),
                   'conditions':[{'feature':a[1],'operator':a[2],'value':a[3]} for a in combo],
                   'rationale':[a[4] for a in combo]}


def group(row):
    lab=row['evaluation_labels']
    return 'positives' if lab['review_class']=='PRINTED_VERSE_MARKER' and lab['visible_printed_value']==2 else (
        'other_marker_controls' if lab['review_class']=='PRINTED_VERSE_MARKER' else 'non_marker_controls')


def confusion(rows, decisions):
    count=Counter()
    for r,accepted in zip(rows,decisions):
        count[('TP' if accepted else 'FN') if group(r)=='positives' else ('FP' if accepted else 'TN')]+=1
    return {k:count[k] for k in ('TP','FN','FP','TN')}


def distributions(rows):
    out={}
    for feature in SCHEMA:
        out[feature]={}
        for category in ('positives','other_marker_controls','non_marker_controls'):
            values=[r['runtime_features'][feature] for r in rows if group(r)==category]
            freq=Counter(json.dumps(v,ensure_ascii=False,sort_keys=True) for v in values)
            numeric=[v for v in values if type(v) in (int,float)]
            out[feature][category]={'population':len(values),'frequencies':dict(sorted(freq.items())),
                'range':{'min':min(numeric),'median':statistics.median(numeric),'max':max(numeric)} if numeric else None}
    return out


def grouped(rows,decisions):
    out={}
    for name in ('book','page','layout_region','token_position','width_range'):
        groups=defaultdict(list)
        for i,r in enumerate(rows):
            f=r['runtime_features'];v=f['width_in_digits']
            key=(r['metadata'][name] if name in ('book','page') else
                 f['column']+'/'+f['zone'] if name=='layout_region' else
                 f['token_position'] if name=='token_position' else
                 'unavailable' if v is None else '<1' if v<1 else '1..1.5' if v<1.5 else '>=1.5')
            groups[str(key)].append(i)
        out[name]={k:confusion([rows[i] for i in indices],[decisions[i] for i in indices]) for k,indices in sorted(groups.items())}
    return out


def buckets(rows,decisions):
    result={}
    for value in (1,2,3,8,12,13,20,21,22,24,25,'ORDINARY_TEXT','APPARATUS_OR_NOTE'):
        indices=[i for i,r in enumerate(rows) if (r['evaluation_labels']['visible_printed_value']==value if isinstance(value,int)
                 else r['evaluation_labels']['review_class']==value)]
        accepted=sum(decisions[i] for i in indices)
        result[str(value)]={'population':len(indices),'accepted':accepted,'rejected':len(indices)-accepted}
    return result


def runtime_snapshot(audit):
    s=audit['verse_segmentation_audit']
    # Stable full recovery evidence includes ownership moves and their identities.
    recovery={'task128':s['compound_glyph_recovery']['markers'],
              'task131':s['a_glyph_pixel_recovery']['ownership_changes'],
              'task139':s['zero_anchor_io_recovery']}
    return {'verse_refs':audit['verse_refs'],'physical_gaps':len(s['gaps']),
            'glyph_gaps':s['by_signal']['lone_glyph_inside_previous_verse'],
            'chapters':audit['chapters'],'unresolved_chapter_claims':audit['chapter_claims']['unresolved'],
            'canonical_chapter_gaps':len(audit['canonical_chapter_gap_reviews']['canonical_missing']),
            'duplicate_refs':len(audit['duplicate_refs']),'out_of_order_refs':len(audit['out_of_order_refs']),
            'outside_canon':len(audit.get('outside_canon',[])), 'ocr_blocks':audit['metrics']['ocr_blocks'],
            'block_loss':s['zero_anchor_io_recovery']['block_loss'], 'dual_ownership':s['zero_anchor_io_recovery']['dual_ownership'],
            'recovery_evidence_sha256':hashlib.sha256(encode(recovery).encode()).hexdigest(),
            'task128':{'markers':s['compound_glyph_recovery']['applied'],
                       'refs':s['compound_glyph_recovery']['refs_added'],
                       'moves':s['compound_glyph_recovery']['blocks_moved']},
            'task131':{'markers':s['a_glyph_pixel_recovery']['markers_recovered'],
                       'refs':s['a_glyph_pixel_recovery']['refs_added'],
                       'moves':s['a_glyph_pixel_recovery']['blocks_moved']},
            'task139':s['zero_anchor_io_recovery'], 'GLUED_FRAME':'CLOSED_UNSAFE',
            'new_recovery':False,'runtime_image_reads':False}


def build(audit, baseline_commit, xml=XML, timings=None):
    start=time.perf_counter()
    if baseline_commit!=BASELINE:
        raise ValueError('Task 142 requires its frozen input commit')
    candidates=family(audit['verse_segmentation_audit']['gaps'])
    frozen=extract(candidates,xml)
    if len(frozen)!=260 or len({m['occurrence_id'] for m,f in frozen})!=260:
        raise ValueError('Expected 260 unique current occurrences')
    # These files are deliberately opened ONLY AFTER all vectors are immutable.
    historical=json.loads((DATA/'remaining_glyph_reprioritization.json').read_text())
    current_ids={m['occurrence_id'] for m,f in frozen}
    historical_ids={r['stable_occurrence_id'] for r in historical['current_inventory_summary']['rows']
                    if r['raw_ocr_form']=='a' and r['primary_root_cause']=='PROJECTED_FROM_MULTI_TOKEN' and r['source_zone']=='verse_body'}
    if current_ids!=historical_ids:
        raise ValueError('Task-140 family reconciliation failed')
    extracted=time.perf_counter()
    truth=json.loads((DATA/'projected_form_a_facsimile.json').read_text())
    rows=join_labels(frozen,truth['occurrence_reviews'])
    if Counter(group(r) for r in rows)!=Counter(positives=77,other_marker_controls=161,non_marker_controls=22):
        raise ValueError('Task-141 label split failed')
    expected={'1':1,'2':77,'3':1,'8':2,'12':1,'13':1,'20':29,'21':67,'22':19,'24':31,'25':9}
    if dict(Counter(str(r['evaluation_labels']['visible_printed_value']) for r in rows if r['evaluation_labels']['review_class']=='PRINTED_VERSE_MARKER'))!=expected:
        raise ValueError('Task-141 visible-value buckets failed')
    results=[]
    for rule in rules():
        decisions=[accepts(rule,r['runtime_features']) for r in rows]
        counts=confusion(rows,decisions)
        accepted=[r for r,d in zip(rows,decisions) if d]
        results.append({'rule':rule, **counts,
            'false_positive_ids':[r['metadata']['occurrence_id'] for r,d in zip(rows,decisions) if d and group(r)!='positives'],
            'false_negative_ids':[r['metadata']['occurrence_id'] for r,d in zip(rows,decisions) if not d and group(r)=='positives'],
            'accepted_by_visible_value':buckets(rows,decisions),
            'generalization':grouped(rows,decisions),
            'accepted_distinct_blocks':len({r['metadata']['block'] for r in accepted}),
            'accepted_books':sorted({r['metadata']['book'] for r in accepted}),
            'accepted_pages':sorted({r['metadata']['page'] for r in accepted})})
    evaluated=time.perf_counter()
    # Selection is deliberately separate from predicate evaluation. A zero-FP
    # result is necessary, never sufficient: review robustness/generalization.
    safe=[r for r in results if r['TP'] and not r['FP']]
    selection={'zero_fp_candidates':[r['rule']['name'] for r in safe]}
    result={
        'schema_version':1,
        'provenance':{'baseline_commit':baseline_commit,'task141_sha256':sha(DATA/'projected_form_a_facsimile.json'),
                      'task140_sha256':sha(DATA/'remaining_glyph_reprioritization.json'), 'source_xml_sha256':sha(xml),
                      'source_identity':truth['provenance']['source_identity'],
                      'generator':'scripts/torresamat1835/projected_form_a_visible_2_discriminator.py'},
        'family_definition':{'exact_diagnostic_form':'a','classification':'PROJECTED_FROM_MULTI_TOKEN',
            'scope':'current parser glyph-gap rows, physical source multi-token lines in task-140 verse-body scope',
            'definition_uses_ids':False,'unique_occurrences':260,'distinct_source_blocks':len({m['block'] for m,f in frozen}),
            'unit_warning':'Gap occurrences share source blocks; grouped results and distinct block counts prevent treating them as independent evidence.'},
        'ground_truth_summary':{'total':260,'positives':77,'controls':183,'other_marker_controls':161,'non_marker_controls':22,
                                'visible_value_counts':expected,'review_classes':truth['class_counts']},
        'runtime_feature_schema':SCHEMA,
        'field_separation':{'runtime_features':list(SCHEMA),'evaluation_labels':list(LABELS),
            'metadata_only':['occurrence_id','block','book','chapter','page'],
            'extraction_order':'current family -> source-only vectors -> immutable mappings for all 260 -> open task141 -> label join -> evaluation',
            'frame_boundary':'Task141 reports physical token 0. This task also locates a in first three source words; glued frames remain unsplit and are excluded by unframed_start.'},
        'occurrence_features':rows,'feature_distributions':distributions(rows),
        'candidate_rules':[r['rule'] for r in results],
        'confusion_results':results,'selection_review':selection,
        'runtime_invariants':runtime_snapshot(audit),
    }
    finalize(result)
    compact_evaluations(result)
    if timings is not None:
        timings.update(feature_extraction_seconds=extracted-start,rule_evaluation_seconds=evaluated-extracted,
                       artifact_generation_seconds=time.perf_counter()-start)
    return result


def finalize(result):
    """Reviewed selection: existing marker band + raw start + prose prefix."""
    rows=result['occurrence_features']
    results=result['confusion_results']
    chosen=next(r for r in results if r['rule']['name']=='unframed AND three_words AND marker_band')
    if chosen['FP'] or chosen['TP']!=53:
        raise ValueError('Reviewed task-142 discriminator no longer reconciles; revalidation required')
    rule=chosen['rule']
    decisions=[accepts(rule,r['runtime_features']) for r in rows]
    selected=[r for r,d in zip(rows,decisions) if d]
    near=[]
    missed=[]
    for r,d in zip(rows,decisions):
        failures=[c for c in rule['conditions'] if not accepts(
            {'name':'individual condition','conditions':[c],'rationale':[]},r['runtime_features'])]
        detail={'occurrence_id':r['metadata']['occurrence_id'], 'metadata':r['metadata'],
                'runtime_features':r['runtime_features'], 'evaluation_labels':r['evaluation_labels'],
                'failed_conditions':failures}
        if group(r)!='positives':
            # Retain ALL controls, with exact values and reasons, not a cherry-picked sample.
            detail['failed_condition_count']=len(failures)
            f=r['runtime_features']
            detail['band_margin_pixels']=None if f['band_delta'] is None else abs(f['band_delta'])-f['band_tolerance']
            near.append(detail)
        elif not d:
            missed.append(detail)
    result.update(
        selected_rule=rule,
        result_status='NARROWER_SAFE_SUBFAMILY_FOUND',
        selected_positive_subset={'occurrences':len(selected),
            'distinct_source_blocks':len({r['metadata']['block'] for r in selected}),
            'occurrence_ids':[r['metadata']['occurrence_id'] for r in selected],
            'identities_are_reporting_only':True},
        selected_confusion={k:chosen[k] for k in ('TP','FN','FP','TN')},
        accepted_by_visible_value=chosen['accepted_by_visible_value'],
        false_positive_analysis={'selected_rule_false_positives':[],
            'all_failed_rules':'Every confusion_results entry retains exact FP identities, label buckets and grouping.',
            'ablation':'Without unframed_start, values 8 and 12 pass; without the three-word prefix, other numerals and apparatus pass; without the marker band, printed 1 passes.'},
        false_negative_analysis={'population':len(missed),'occurrences':missed,
            'feature_distributions':distributions([r for r,d in zip(rows,decisions) if not d and group(r)=='positives']),
            'interpretation':'Failures include frame-prefixed OCR, single-letter words/punctuation in the prose prefix, absent band, and out-of-band source geometry. No threshold is widened to recover them.'},
        near_miss_analysis={'controls':sorted(near,key=lambda r:(r['failed_condition_count'],r['occurrence_id'])),
            'distance_definition':'Number of failed independent predicates; not a classifier or score.',
            'geometry_boundary':'The printed-1 control is 77 pixels from its calibrated band center, versus tolerance 38: rejection margin 39 pixels. No one-pixel label-tuned threshold.',
            'lexical_boundary':'Three is the existing task-141 neighboring-token window, not an optimized vocabulary or token count. Two letters excludes isolated numeral fragments and note sigla, but also legitimate single-letter Spanish words.',
            'frame_boundary':'No image segmentation or safe trimming is allowed to turn a prefixed raw token into an unframed start.'},
        overfitting_analysis={'selected_grouping':chosen['generalization'],
            'selected_books':chosen['accepted_books'],'selected_pages':chosen['accepted_pages'],
            'distinct_source_blocks':chosen['accepted_distinct_blocks'],
            'layout_boundary':'Current exact-a projected family; first physical token exactly a; three following multi-letter tokens; existing source-calibrated trusted band. Same-page anchor geometry supplies the regime, never page identity.',
            'rejected_zero_fp_single_feature':'body_height accepts only four Ps source lines; rejected as insufficient cross-book evidence, not declared safe.',
            'width_alternative':'unframed AND three_words AND digit_width has zero known FP, but the printed-1 control width ratio is 74/76, only two pixels below the one-digit boundary. Prefer the existing band with a 39-pixel control margin.',
            'capital_alternative':'unframed AND following_capital AND three_words accepts 46 positives with zero FP; selected existing marker-band rule has stronger source-layout authority and no case dependence.',
            'limitations':'In-sample diagnostic validation, not proof on unseen pages or sources. Duplicated gap occurrences are not independent physical examples. OCR corruption and limited non-marker diversity remain uncertainties.'},
        task143_recommendation={'task_id':'TORRES-1835-PROJECTED-FORM-A-VISIBLE-2-DRY-RUN-143',
            'target':'Dry-run recovery validation only for the exact selected three-feature rule',
            'reason':'53/77 diagnostic positives across multiple books and pages; zero of all 183 controls; existing band threshold and explicit morphology predicates.',
            'exact_scope':'Recompute the selected unframed_start AND next_three_are_words AND in_band rule from source in the current exact-a projected family; validate its 53 diagnostic occurrences / 47 physical blocks against all 183 controls, source-event deduplication, VerseRef effects and ownership in a separate dry run. No production recovery and no extrapolation to rejected occurrences.'})
    result['selection_review']['decision']='Select the three-feature existing-band rule; all one/two-feature zero-FP results are Ps-only body-height variants.'
    result['runtime_invariants']['ownership_unchanged']=True
    result['audit_summary']=audit_summary(result)


def compact_evaluations(data):
    """Share identical decision populations while retaining every failed rule.

    The existing diagnostic-artifact size guard stays unchanged. Each rule has
    its own four confusion counts and an explicit population reference whose
    FP/FN identities, visible-value accounting and all groupings are complete.
    """
    populations={}
    keys={}
    for entry in data['confusion_results']:
        payload={k:v for k,v in entry.items() if k not in ('rule','TP','FN','FP','TN')}
        signature=encode(payload)
        if signature not in keys:
            key=f'E{len(keys)+1:03d}'
            keys[signature]=key
            populations[key]=payload
        rule_name=entry['rule']['name']
        counts={k:entry[k] for k in ('TP','FN','FP','TN')}
        entry.clear()
        entry.update(rule_name=rule_name, **counts, evaluation_population=keys[signature])
    data['evaluation_populations']=populations
    data['evaluation_encoding']='confusion_results contains each rule and TP/FN/FP/TN; evaluation_population resolves complete shared identity lists, per-value counts and book/page/layout grouping in evaluation_populations.'


def audit_summary(data):
    """Small, read-only integration payload; no extraction or corpus scans."""
    c=data['selected_confusion']; recommendation=data['task143_recommendation']
    return {'total_candidates':260,'positives':77,'controls':183,
            'other_value_marker_controls':161,'non_marker_controls':22,
            'candidate_rule_count':len(data['candidate_rules']),'selected_rule':data['selected_rule'],
            **{'selected_rule_'+k.lower():v for k,v in c.items()},
            'accepted_by_visible_value':data['accepted_by_visible_value'],
            'result_status':data['result_status'],
            'selected_task143_target':recommendation['target'],
            'selected_task143_reason':recommendation['reason']}


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--audit',required=True);ap.add_argument('--baseline-commit',required=True)
    ap.add_argument('--xml',type=Path,default=XML);ap.add_argument('--out',type=Path,required=True)
    ns=ap.parse_args(); timings={}
    data=build(json.loads(Path(ns.audit).read_text()),ns.baseline_commit,ns.xml,timings)
    ns.out.write_text(encode(data),encoding='utf-8')
    print(json.dumps(timings,sort_keys=True))

if __name__=='__main__':
    main()
