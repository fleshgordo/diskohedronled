#!/usr/bin/env python3
"""Markdown (with ```mermaid diagrams) -> PDF, rendered by headless Google Chrome.

    python3 tools/md2pdf.py WIRING.md                 -> docs/WIRING.pdf
    python3 tools/md2pdf.py WIRING.md -o wiring.pdf --landscape

Needs Google Chrome and `pip install websockets`. marked.js and mermaid.js are
downloaded once into tools/.cache/.
"""
import argparse
import asyncio
import base64
import json
import subprocess
import tempfile
import time
import urllib.request
from pathlib import Path

CHROME = '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome'
TOOLS = Path(__file__).resolve().parent
CACHE = TOOLS / '.cache'
LIBS = {
    'marked.min.js': 'https://cdn.jsdelivr.net/npm/marked@12.0.2/marked.min.js',
    'mermaid.min.js': 'https://cdn.jsdelivr.net/npm/mermaid@11.4.1/dist/mermaid.min.js',
}

CSS = """
body { font: 11pt/1.45 -apple-system, 'Helvetica Neue', Arial, sans-serif; color: #222; margin: 0; }
h1 { font-size: 20pt; border-bottom: 2px solid #ffb347; padding-bottom: 4px; }
h2 { font-size: 15pt; margin-top: 1.6em; break-after: avoid; }
h3 { font-size: 12.5pt; break-after: avoid; }
table { border-collapse: collapse; margin: 0.8em 0; break-inside: avoid; }
th, td { border: 1px solid #ccc; padding: 4px 8px; text-align: left; vertical-align: top; }
th { background: #f4f4f4; }
code { background: #f4f4f4; padding: 1px 4px; border-radius: 3px; font-size: 0.92em; }
pre { background: #f4f4f4; padding: 8px 10px; border-radius: 4px; overflow-wrap: anywhere; white-space: pre-wrap; }
pre code { background: none; padding: 0; }
.mermaid { text-align: center; margin: 1em 0; break-inside: avoid; }
/* mermaid sets max-width to the natural size: small diagrams stay small, big ones shrink */
.mermaid svg { height: auto; max-height: 240mm; }
img { max-width: 100%; }
"""

PAGE = """<!doctype html><html><head><meta charset="utf-8"><style>{css}</style>
<script>{marked}</script><script>{mermaid}</script></head>
<body><main id="doc"></main><script>
(async () => {{
  try {{
    const md = {md};
    const doc = document.getElementById('doc');
    doc.innerHTML = marked.parse(md);
    const nodes = [];
    for (const code of doc.querySelectorAll('pre > code.language-mermaid')) {{
      const div = document.createElement('div');
      div.className = 'mermaid';
      div.textContent = code.textContent;
      code.parentElement.replaceWith(div);
      nodes.push(div);
    }}
    mermaid.initialize({{ startOnLoad: false, theme: 'default', flowchart: {{ htmlLabels: true }} }});
    await mermaid.run({{ nodes }});
    window.__done = 'ok:' + nodes.length;
  }} catch (e) {{
    window.__done = 'error: ' + (e.message || e);
  }}
}})();
</script></body></html>"""


def lib(name):
    path = CACHE / name
    if not path.exists():
        CACHE.mkdir(exist_ok=True)
        print('downloading', LIBS[name])
        urllib.request.urlretrieve(LIBS[name], path)
    return path.read_text(encoding='utf-8').replace('</script>', '<\\/script>')


async def print_pdf(html_path, out, landscape):
    from websockets.asyncio.client import connect
    profile = tempfile.mkdtemp()
    chrome = subprocess.Popen([CHROME, '--headless=new', '--disable-gpu', '--remote-debugging-port=9334',
                               f'--user-data-dir={profile}', '--allow-file-access-from-files', 'about:blank'],
                              stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        for _ in range(300):
            try:
                tabs = json.load(urllib.request.urlopen('http://127.0.0.1:9334/json'))
                break
            except OSError:
                time.sleep(0.1)
        url = next(t['webSocketDebuggerUrl'] for t in tabs if t['type'] == 'page')
        async with connect(url, max_size=None) as ws:
            n = 0

            async def cmd(method, **params):
                nonlocal n
                n += 1
                await ws.send(json.dumps({'id': n, 'method': method, 'params': params}))
                while True:
                    msg = json.loads(await ws.recv())
                    if msg.get('id') == n:
                        if 'error' in msg:
                            raise RuntimeError(msg['error'])
                        return msg['result']

            await cmd('Page.enable')
            await cmd('Page.navigate', url=html_path.as_uri())
            status = None
            for _ in range(300):
                r = await cmd('Runtime.evaluate', expression='window.__done || null', returnByValue=True)
                status = r['result'].get('value')
                if status:
                    break
                await asyncio.sleep(0.1)
            if not status or not status.startswith('ok'):
                raise SystemExit(f'rendering failed: {status or "timeout"}')
            r = await cmd('Page.printToPDF', printBackground=True, landscape=landscape,
                          paperWidth=8.27, paperHeight=11.69, marginTop=0.6, marginBottom=0.6,
                          marginLeft=0.6, marginRight=0.6, displayHeaderFooter=True,
                          headerTemplate='<span></span>',
                          footerTemplate='<div style="font-size:8px;width:100%;text-align:center;color:#888">'
                                         '<span class="title"></span> – <span class="pageNumber"></span>/'
                                         '<span class="totalPages"></span></div>')
            out.write_bytes(base64.b64decode(r['data']))
            return status
    finally:
        chrome.terminate()


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('markdown')
    ap.add_argument('-o', '--out', help='default: docs/<name>.pdf next to the markdown file')
    ap.add_argument('--landscape', action='store_true')
    args = ap.parse_args()

    src = Path(args.markdown).resolve()
    out = Path(args.out).resolve() if args.out else src.parent / 'docs' / (src.stem + '.pdf')
    out.parent.mkdir(exist_ok=True)
    html = PAGE.format(css=CSS, marked=lib('marked.min.js'), mermaid=lib('mermaid.min.js'),
                       md=json.dumps(src.read_text(encoding='utf-8')).replace('</', '<\\/'))
    # written next to the markdown so relative image paths keep working
    tmp = src.parent / f'.{src.stem}.md2pdf.html'
    tmp.write_text(html.replace('<head>', f'<head><title>{src.stem}</title>', 1), encoding='utf-8')
    try:
        status = asyncio.run(print_pdf(tmp, out, args.landscape))
    finally:
        tmp.unlink(missing_ok=True)
    print(f'wrote {out} ({status.split(":")[1]} diagrams)')


if __name__ == '__main__':
    main()
