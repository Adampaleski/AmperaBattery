# python3 scripts/test_wiring_crossings.py — wiring aid drawing standard: no unrelated wires overlapping; crossings drawn as hops, junctions as dots
import sys, pathlib
from playwright.sync_api import sync_playwright
U=(pathlib.Path(__file__).resolve().parent.parent/'tools/wiring.html').as_uri()
JS='''()=>{const out={};PAGES.forEach(pg=>{const N={};pg.nodes.forEach(n=>N[n[0]]=n);const cx=n=>[n[2]+n[4]/2,n[3]+n[5]/2];let ov=0,lst=[];
const S=document.createElementNS('http://www.w3.org/2000/svg','svg');document.body.appendChild(S);draw(pg,S,1);const W0=S._W;S.remove();const W=W0||pg.wires.map(([a,b])=>{const A=N[a],B=N[b];if(!A||!B)return null;let[x1,y1]=cx(A),[x2,y2]=cx(B);return {a,b,pts:Math.abs(x2-x1)>Math.abs(y2-y1)?[[A[2]+(x2>x1?A[4]:0),y1],[(x1+x2)/2,y1],[(x1+x2)/2,y2],[B[2]+(x2>x1?0:B[4]),y2]]:[[x1,A[3]+(y2>y1?A[5]:0)],[x1,(y1+y2)/2],[x2,(y1+y2)/2],[x2,B[3]+(y2>y1?0:B[5])]]}}).filter(Boolean);
for(let i=0;i<W.length;i++)for(let j=i+1;j<W.length;j++){const wi=W[i],wj=W[j];const share=[wi.a,wi.b].some(x=>x===wj.a||x===wj.b);if(share)continue;
 for(let k=0;k<3;k++)for(let m=0;m<3;m++){const p=wi.pts[k],q=wi.pts[k+1],r=wj.pts[m],s=wj.pts[m+1];
  if(p[1]===q[1]&&r[1]===s[1]&&p[1]===r[1]&&Math.min(Math.max(p[0],q[0]),Math.max(r[0],s[0]))-Math.max(Math.min(p[0],q[0]),Math.min(r[0],s[0]))>1){ov++;lst.push(wi.a+'-'+wi.b+' / '+wj.a+'-'+wj.b)}
  if(p[0]===q[0]&&r[0]===s[0]&&p[0]===r[0]&&Math.min(Math.max(p[1],q[1]),Math.max(r[1],s[1]))-Math.max(Math.min(p[1],q[1]),Math.min(r[1],s[1]))>1){ov++;lst.push(wi.a+'-'+wi.b+' | '+wj.a+'-'+wj.b)}}}
out[pg.id]=[ov,lst.slice(0,6)]});return out}'''
with sync_playwright() as p:
    b=p.chromium.launch(executable_path='/usr/bin/google-chrome',args=['--no-sandbox']);pg=b.new_page()
    errs=[];pg.on('pageerror',lambda e:errs.append(str(e)));pg.goto(U);pg.wait_for_timeout(400)
    r=pg.evaluate(JS);bad=sum(v[0] for v in r.values())
    hops=pg.evaluate("(()=>{let h=0;PAGES.forEach(p=>{const S=document.createElementNS('http://www.w3.org/2000/svg','svg');document.body.appendChild(S);draw(p,S,1);h+=+S.dataset.hops;S.remove()});return h})()")
    arcs=pg.evaluate("document.querySelector('main svg').innerHTML.includes(' A6 6 ')")
    for k,v in r.items():
        if v[0]: print('FAIL overlap',k,v)
    print(('PASS' if not bad else 'FAIL'),'wiring: no unrelated wires overlap on any of',len(r),'pages')
    print(('PASS' if hops>0 and arcs else 'FAIL'),'wiring: crossings drawn as hop arcs (',hops,'hops)')
    print(('PASS' if not errs else 'FAIL'),'wiring: no page errors',errs[:2])
    # Build 3: install guide, parts panel, critical toggle
    pg.evaluate("localStorage.removeItem('teryx-wiring-crit')");pg.reload();pg.wait_for_timeout(400)
    nc=pg.locator('nav a').count();pg.click('#crit');pg.wait_for_timeout(200);na=pg.locator('nav a').count();pg.click('#crit')
    t1=nc<na and na==len(r);print(('PASS' if t1 else 'FAIL'),'wiring: Critical-only is the default (',nc,'of',na,'pages) and toggles to all')
    miss=[]
    for pid in [x for x in r if x!='overview']:
        pg.evaluate(f"location.hash='{pid}'");pg.wait_for_timeout(150)
        if pg.locator('ol.steps li').count()<3 or 'Wire (from' not in pg.inner_text('ol.steps'): miss.append(pid)
    t2=not miss;print(('PASS' if t2 else 'FAIL'),'wiring: every sub-page has an install guide (mount / wire / torque / test)',miss)
    pg.evaluate("location.hash='precharge'");pg.wait_for_timeout(200);pg.locator('.n[data-id="evp"]').click();pg.wait_for_timeout(200)
    t3='8.8' in pg.inner_text('#pp') and pg.locator('#pp.on').count()==1;print(('PASS' if t3 else 'FAIL'),'wiring: tapping EV200 opens its cut sheet with the sourced torque')
    pg.click('#pp button');pg.wait_for_timeout(300);pg.click('#partsb');pg.wait_for_timeout(200);t4=pg.locator('#pp .box').count()==pg.evaluate('CUTSHEETS.length') and 'NO DATASHEET FOUND' in pg.inner_text('#pp')
    print(('PASS' if t4 else 'FAIL'),'wiring: Parts panel lists every cut sheet and flags the missing ones')
    sys.exit(1 if bad or not hops or errs or not (t1 and t2 and t3 and t4) else 0)
