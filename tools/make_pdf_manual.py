#!/usr/bin/env python3
"""ETCH II -- A Generative Synthesis Machine -- by Synth Centric.
Render docs/ETCH_II_Manual.md (with the cover and screen captures) to PDF.
Needs pandoc and Node.js with Playwright/Chromium.
usage: make_pdf_manual.py OUT.pdf"""
import os
import subprocess
import sys
import tempfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
OUT = os.path.abspath(sys.argv[1])
CSS = """
@page { size: A4; margin: 16mm 14mm; }
body { font-family: 'DejaVu Sans', Arial, sans-serif; font-size: 10pt; color: #16181a; line-height: 1.38; }
h1 { font-size: 24pt; letter-spacing: 5px; color: #16181a; border-bottom: 4px solid #b8683c; padding-bottom: 4px; }
h2 { font-size: 15pt; color: #8a4a26; border-bottom: 1px solid #b8683c; margin-top: 20px; page-break-after: avoid; }
h3 { font-size: 12pt; color: #16181a; page-break-after: avoid; }
table { border-collapse: collapse; margin: 8px 0; width: 100%; page-break-inside: avoid; font-size: 9pt; }
td, th { border: 1px solid #b9b3a6; padding: 3px 6px; vertical-align: top; }
th { background: #22262a; color: #ecba60; }
code, pre { font-family: 'DejaVu Sans Mono', monospace; background: #f0ece4; }
blockquote { border-left: 4px solid #b8683c; margin: 8px 0; padding: 4px 10px; background: #f6f2ea; }
.cover { text-align: center; page-break-after: always; }
.cover img { width: 150mm; }
.shots { display: grid; grid-template-columns: 1fr 1fr 1fr; gap: 6px; page-break-inside: avoid; }
.shots figure { margin: 0; text-align: center; font-size: 8pt; color: #555; }
.shots img { width: 100%; image-rendering: pixelated; border: 1px solid #22262a; }
"""


def main():
    md = os.path.join(ROOT, "docs", "ETCH_II_Manual.md")
    html = subprocess.run(["pandoc", "-f", "gfm", "-t", "html5", md], capture_output=True, text=True, check=True).stdout
    cover = os.path.join(ROOT, "release", "artwork", "ETCH_II_Cover_Small.png")
    shots = os.path.join(ROOT, "docs", "images")
    names = ["title", "live", "track", "synth", "mod", "gen", "memory", "song", "file"]
    gallery = "<h2>The screens</h2><div class='shots'>" + "".join(
        "<figure><img src='file://%s'/><figcaption>%s</figcaption></figure>" % (os.path.join(shots, "screen_%s.png" % n), n.upper())
        for n in names if os.path.exists(os.path.join(shots, "screen_%s.png" % n))) + "</div>"
    # place the gallery before section 6 (the screens)
    import re
    m = re.search(r'<h2 id="[^"]*">6\. The screens', html)
    if m:
        html = html[:m.start()] + gallery.replace("<h2>The screens</h2>", "") + html[m.start():]
    page = ("<html><head><meta charset='utf-8'><style>%s</style></head><body>"
            "<div class='cover'><img src='file://%s'/></div>%s</body></html>") % (CSS, cover, html)
    tmpdir = tempfile.mkdtemp()
    htm = os.path.join(tmpdir, "manual.html")
    open(htm, "w", encoding="utf-8").write(page)
    js = os.path.join(tmpdir, "pdf.js")
    open(js, "w").write("""
const { chromium } = require('playwright');
(async () => {
  const b = await chromium.launch();
  const pg = await b.newPage();
  await pg.goto('file://' + process.argv[2], { waitUntil: 'load' });
  await pg.pdf({ path: process.argv[3], format: 'A4', printBackground: true,
    displayHeaderFooter: true, headerTemplate: '<span></span>',
    footerTemplate: '<div style="font-size:7pt;width:100%;text-align:center;color:#777">ETCH II — A Generative Synthesis Machine — by Synth Centric — <span class="pageNumber"></span></div>',
    margin: { top: '14mm', bottom: '16mm', left: '12mm', right: '12mm' } });
  await b.close();
})();
""")
    subprocess.run(["node", js, htm, OUT], check=True)
    print("wrote", OUT)


if __name__ == "__main__":
    main()
