#!/usr/bin/env python3
import argparse, hashlib, json, subprocess
from collections import Counter, defaultdict
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).parent))
from source_ocr import read_pages

# Facsimile observations, retained as diagnostic evidence rather than parser rules.
REVIEWS=[
 {'review_id':'batch-135-p0276l0033','batch':'batch-135','exact_ocr_form':'a','scan_page':276,'pdf_page':277,'block_id':'p0276l0033','raw_ocr':'a Vanidad de vanidades, dijo el Eccle-','bbox':[1766,1642,2980,1713],'visible_printed_content':'2 Vanidad de vanidades, dijo el Eclesiastés','classification':'PRINTED_DIGIT','observed_printed_digit':'2','confidence':'high','rationale':'Visible numeral at the Spanish verse start in the verified facsimile.','structural_effect':'none_diagnostic_only'},
 {'review_id':'batch-135-p0296l0015','batch':'batch-135','exact_ocr_form':'y','scan_page':296,'pdf_page':297,'block_id':'p0296l0015','raw_ocr':'y bebidas para reir y banquetear; pacs','bbox':[1793,512,3056,590],'visible_printed_content':'y bebidas para reir y banquetear','classification':'ORDINARY_TEXT','observed_printed_digit':None,'confidence':'high','rationale':'Visible Spanish conjunction in the verified facsimile.','structural_effect':'none_diagnostic_only'},
 {'review_id':'batch-135-p0071l0060','batch':'batch-135','exact_ocr_form':'4','scan_page':71,'pdf_page':72,'block_id':'p0071l0060','raw_ocr':'4acob.','bbox':[1699,2749,1897,2813],'visible_printed_content':'Jacob','classification':'ORDINARY_TEXT','observed_printed_digit':None,'confidence':'high','rationale':'Visible final word Jacob in Spanish verse body; OCR leading 4 is not a marker.','structural_effect':'none_diagnostic_only'},
 {'review_id':'batch-135-p0071l0065','batch':'batch-135','exact_ocr_form':'8','scan_page':71,'pdf_page':72,'block_id':'p0071l0065','raw_ocr':'8aloM>.','bbox':[2240,3377,2451,3433],'visible_printed_content':'SALMO XLVI','classification':'OTHER','observed_printed_digit':None,'confidence':'medium','rationale':'Printed Psalm heading, not a verse marker occurrence.','structural_effect':'none_diagnostic_only'},
 {'review_id':'batch-135-p0094l0047-v18','batch':'batch-135','exact_ocr_form':'40','scan_page':94,'pdf_page':95,'block_id':'p0094l0047','raw_ocr':'40i>dia.','bbox':[1811,2350,2028,2408],'visible_printed_content':'unreadable OCR/print correspondence','classification':'UNREADABLE','observed_printed_digit':None,'confidence':'high','rationale':'The isolated OCR token cannot be matched to a legible standalone printed glyph.','structural_effect':'none_diagnostic_only'},
 {'review_id':'batch-135-p0094l0047-v19','batch':'batch-135','exact_ocr_form':'40','scan_page':94,'pdf_page':95,'block_id':'p0094l0047','raw_ocr':'40i>dia.','bbox':[1811,2350,2028,2408],'visible_printed_content':'unreadable OCR/print correspondence','classification':'UNREADABLE','observed_printed_digit':None,'confidence':'high','rationale':'Same physical source line, distinct diagnostic gap occurrence.','structural_effect':'none_diagnostic_only'},
 {'review_id':'batch-135-p0094l0047-v20','batch':'batch-135','exact_ocr_form':'40','scan_page':94,'pdf_page':95,'block_id':'p0094l0047','raw_ocr':'40i>dia.','bbox':[1811,2350,2028,2408],'visible_printed_content':'unreadable OCR/print correspondence','classification':'UNREADABLE','observed_printed_digit':None,'confidence':'high','rationale':'Same physical source line, distinct diagnostic gap occurrence.','structural_effect':'none_diagnostic_only'},
 {'review_id':'batch-135-p0108l0048','batch':'batch-135','exact_ocr_form':'4','scan_page':108,'pdf_page':109,'block_id':'p0108l0048','raw_ocr':'4alosos^','bbox':[1781,1256,2076,1325],'visible_printed_content':'Latin-column text','classification':'LATIN','observed_printed_digit':None,'confidence':'medium','rationale':'Source line is in the Latin parallel column, not a Spanish verse marker.','structural_effect':'none_diagnostic_only'},
]

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('--inventory',required=True); ap.add_argument('--out',required=True); ap.add_argument('--pdf',required=True); ap.add_argument('--xml',required=True); ap.add_argument('--reviews',required=True); ap.add_argument('--audit',required=True)
    ns=ap.parse_args(); inv=json.loads(Path(ns.inventory).read_text()); rows=[r for r in inv['rows'] if r['primary_root_cause']=='standalone_glyph_candidate']
    wanted={(int(r['source_block'][1:5]),int(r['source_block'][6:])) for r in rows if r.get('source_block')}
    lines={}
    for page in read_pages(ns.xml):
        for index, line in enumerate(page.lines):
            if (page.scan_page,index) in wanted: lines[(page.scan_page,index)]=(line.raw_text,list(line.bbox))
    for r in rows:
        if not r.get('source_block'): r['projection_class']='OTHER_UNKNOWN'; r['raw_ocr_line']=''; r['token_count']=0; continue
        pg,ix=int(r['source_block'][1:5]),int(r['source_block'][6:]); raw,bbox=lines.get((pg,ix),('',[])); r['raw_ocr_line']=raw; r['token_count']=len(raw.split()); r['source_bbox']=bbox
        r['projection_class']='TRUE_SINGLE_TOKEN_LINE' if r['token_count']==1 else ('PROJECTED_TOKEN_FROM_MULTI_TOKEN_LINE' if r['token_count']>1 else 'OTHER_UNKNOWN')
    t128=json.loads(Path(ns.audit).read_text())['verse_segmentation_audit']['compound_glyph_recovery']
    handled={x['block_id']:x for x in t128['markers']}
    rejected={x['block_id']:x.get('reason','historical_rejection') for x in t128['geometry_rejected_detail']}
    for x in t128['canon_rejected_detail']:
        for bid in x.get('block_ids',[]): rejected[bid]=x['reason']
    for r in rows:
        parts=r['raw_ocr_line'].split(); pair=' '.join(parts[:2])
        r['task128_overlap']=('TASK128_HANDLED_EXACT' if r.get('source_block') in handled else ('TASK128_REJECTED_EXACT' if r.get('source_block') in rejected else ('TASK128_NOT_APPLICABLE' if r['projection_class']=='PROJECTED_TOKEN_FROM_MULTI_TOKEN_LINE' else 'OTHER_UNRESOLVED')))
    by=defaultdict(list)
    for r in rows: by[r['raw_ocr_form']].append(r)
    forms=[]
    for form, rs in sorted(by.items()):
        rr=[x for x in REVIEWS if x['exact_ocr_form']==form]; pos=[x for x in rr if x['classification']=='PRINTED_DIGIT']; neg=[x for x in rr if x['classification']=='ORDINARY_TEXT']
        forms.append({'exact_ocr_form':form,'unicode_codepoints':[f'U+{ord(c):04X}' for c in form],'population':len(rs),'projection_counts':dict(Counter(r['projection_class'] for r in rs)),'gap_count':len(rs),'unique_blocks':len({r['source_block'] for r in rs if r['source_block']}),'books':sorted({r['book'] for r in rs}),'chapters':sorted({f"{r['book']}.{r['chapter']}" for r in rs}),'scan_pages':sorted({r['scan_page'] for r in rs if r['scan_page'] is not None}),'columns':dict(sorted(Counter(r['column'] for r in rs).items())),'prior_reviewed':0,'batch_135_reviewed':len(rr),'marker_positives':len(pos),'ordinary_text_negatives':len(neg),'other_negatives':0,'unreadable':0,'printed_value_distribution':dict(Counter(x['observed_printed_digit'] for x in pos)),'conflicts':[],'unreviewed':len(rs)-len(rr),'status':'UNSAFE_WITH_CURRENT_EVIDENCE' if form=='y' else 'NEEDS_MORE_REVIEW'})
    commit=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip()
    pdf=Path(ns.pdf); sha=hashlib.sha256(pdf.read_bytes()).hexdigest()
    old=json.loads(Path(ns.reviews).read_text()); prior=old.get('reviews',[])
    block_ids={r['source_block'] for r in rows}; matches=[x for x in prior if x.get('block_id') in block_ids or x.get('target_block') in block_ids]
    projection=dict(Counter(r['projection_class'] for r in rows)); overlap=dict(Counter(r['task128_overlap'] for r in rows if r['projection_class']=='PROJECTED_TOKEN_FROM_MULTI_TOKEN_LINE')); reasons=dict(Counter(rejected.get(r.get('source_block')) for r in rows if r.get('source_block') in rejected))
    singles=[r for r in rows if r['projection_class']=='TRUE_SINGLE_TOKEN_LINE']
    out={'schema_version':3,'provenance':{'edition':'Torres Amat 1832-1835','witness':'ia-lasagradabiblia01unkngoog','pdf':str(pdf),'source_sha256':sha,'expected_source_sha256':'cb9cf759ff77d0a7822efeba5bf62734544cee9681736bd00085a1a0b2384346','pdf_pages':652,'baseline_commit':commit,'offline':True},'standalone_candidate_definition':'Task-134 rows with lone_glyph signal; refined only diagnostically by source-line token count. GLUED_FRAME excluded. The historical name denotes projected candidate token, not necessarily a one-token OCR line.','projection_taxonomy':projection,'all_true_single_occurrences':singles,'task128_linkage_method':'Exact source block_id against task-128 marker/rejection detail; grammar-only similarity is not exact linkage.','task128_overlap':overlap,'task128_rejection_reasons':reasons,'inventory_accounting':{'standalone_population':len(rows),'glyph_gaps':inv['glyph_gap_total'],'candidate_lines':len(rows),'unique_blocks':len({r['source_block'] for r in rows if r['source_block']}),'exact_form_count':len(forms)},'prior_reviews':{'batches':[124,127,128,129,130,131,132,133],'records_examined':len(prior),'exact_block_matches':len(matches),'reused':0,'contextual_nonreusable':len(matches),'unmatched_current':len(rows)-len({x.get('block_id') for x in matches})},'batch_135_reviews':REVIEWS,'forms':forms,'statuses':{'EVIDENCE_READY':[],'NEEDS_MORE_REVIEW':[f['exact_ocr_form'] for f in forms if f['status']=='NEEDS_MORE_REVIEW'],'UNSAFE_WITH_CURRENT_EVIDENCE':['y']},'structural_findings':{'descriptive_only':True},'pixel_findings':{'performed':False},'task_136_target':'task-128 rejected-projection occurrence audit','task_136_reason':'Exact block linkage distinguishes historical task-128 evidence from grammar-only resemblance; remaining rejected/not-applicable projections require bounded audit.','rules':{'expected_verse_used':False,'previous_next_verse_used':False,'runtime_changed':False}}
    Path(ns.out).write_text(json.dumps(out,ensure_ascii=False,indent=2,sort_keys=True)+'\n')
    print(json.dumps({'standalone_population':len(rows),'forms':len(forms),'sha256':sha},sort_keys=True))
if __name__=='__main__': main()
