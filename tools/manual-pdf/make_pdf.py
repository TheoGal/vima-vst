# Builds "Vima Manual.pdf" from MANUAL.md: Markdown -> HTML -> Chromium print (A4).
# Usage: python3 tools/manual-pdf/make_pdf.py . "docs/Vima Manual.pdf"
# Needs: pip install markdown playwright pillow; playwright's Chromium.
# The PDF layout only lives here (MANUAL.md stays plain GitHub Markdown).
import re, sys, pathlib, tempfile, markdown
from playwright.sync_api import sync_playwright

repo = pathlib.Path(sys.argv[1]).resolve()
out = pathlib.Path(sys.argv[2]).resolve()
md = (repo / "MANUAL.md").read_text(encoding="utf-8")

html_body = markdown.markdown(md, extensions=["tables", "attr_list", "md_in_html", "sane_lists"])
# bare web addresses in the text (not already in a tag) become links
html_body = re.sub(r'(?<![="\'>/])(https?://[^\s<>()"]+)', r'<a href="\1">\1</a>', html_body)

# Table of contents from the h2 headings (anchors added here)
toc = []
def add_id(m):
    title = re.sub("<[^>]+>", "", m.group(1))
    slug = re.sub(r"[^a-z0-9]+", "-", title.lower()).strip("-")
    toc.append((slug, title))
    return f'<h2 id="{slug}">{m.group(1)}</h2>'
html_body = re.sub(r"<h2>(.*?)</h2>", add_id, html_body)

# Cover: everything before the first h2 (logo, title, screenshot)
first = html_body.find("<h2")
cover, rest = html_body[:first], html_body[first:]
toc_html = '<nav class="toc"><h2 class="toc-title">Contents</h2><ol>' + "".join(
    f'<li><a href="#{s}">{t}</a></li>' for s, t in toc) + "</ol></nav>"

fonts = repo / "Resources" / "fonts"
css = f"""
@font-face {{ font-family: Plex; src: url('{(fonts/'IBMPlexSans-Regular.ttf').as_uri()}'); font-weight: 400; }}
@font-face {{ font-family: Plex; src: url('{(fonts/'IBMPlexSans-SemiBold.ttf').as_uri()}'); font-weight: 600; }}
@font-face {{ font-family: Plex; src: url('{(fonts/'IBMPlexSans-Bold.ttf').as_uri()}'); font-weight: 700; }}
@font-face {{ font-family: VMono; src: url('{(fonts/'VIMAMono-Regular.ttf').as_uri()}'); }}
@page {{ size: A4; margin: 18mm 17mm 20mm 17mm; }}
html {{ -webkit-print-color-adjust: exact; print-color-adjust: exact; }}
body {{ margin: 0; font-family: Plex, sans-serif; font-size: 10pt; line-height: 1.5; color: #1d232b; }}
.cover {{ height: 255mm; display: flex; flex-direction: column; justify-content: center; text-align: center; }}
.cover p {{ margin: 0; }}
.cover h1 {{ font-size: 30pt; font-weight: 700; letter-spacing: 0.04em; margin: 8mm 0 12mm; }}
.cover img[alt="Vima Screenshot"] {{ width: 100%; }}
.cover .sub {{ margin-top: 10mm; color: #6b7480; font-size: 10pt; letter-spacing: .12em; text-transform: uppercase; }}
h2 {{ break-before: page; font-size: 18pt; font-weight: 700; margin: 0 0 4mm; padding-bottom: 2mm; border-bottom: 2px solid #00954D; }}
h3 {{ font-size: 12.5pt; font-weight: 600; margin: 6mm 0 2mm; color: #11151a; }}
h2, h3 {{ break-after: avoid; }}
h2#who-am-i-disclaimer, h2#transport {{ break-before: auto; margin-top: 9mm; }}   /* share the page before */
.table-box {{ break-inside: avoid; }}   /* tables stay whole (the step values table moves to the next page) */
.page-break {{ break-before: page; }}
.repo {{ margin-top: 3mm; padding: 3.5mm 6mm; border: 1px solid #c9d0d9; border-radius: 6px; background: #f1f6f3; text-align: center; break-inside: avoid; }}
.repo .label {{ font-size: 8.5pt; letter-spacing: .12em; text-transform: uppercase; color: #6b7480; }}
.repo a {{ display: block; margin-top: 1.5mm; font-size: 12pt; font-weight: 600; color: #00804a; }}
p, li {{ orphans: 3; widows: 3; }}
a {{ color: #00804a; text-decoration: none; font-weight: 600; }}
code {{ font-family: VMono, monospace; font-size: 8.8pt; background: #eef1f5; padding: 0 3px; border-radius: 3px; }}
img {{ max-width: 100%; height: auto; }}
p[align="center"] {{ text-align: center; break-inside: avoid; margin: 4mm 0; }}
.cover img[alt="Vima Screenshot"] {{ max-height: none; width: 100%; }}
/* captions: a box attached under the image (its width set to the image's by attach.js) */
p[align="center"] em {{ display: block; margin: 0 auto; box-sizing: border-box; padding: 1.6mm 3mm; font-size: 8.5pt; font-style: normal;
  color: #4a525e; background: #eef1f5; border: 1px solid #c9d0d9; border-top: none; border-radius: 0 0 5px 5px; }}
p[align="center"].captioned img {{ display: block; margin: 0 auto; }}
p[align="center"].captioned br {{ display: none; }}
img[align="left"] {{ float: left; margin: 1mm 6mm 3mm 0; }}
br[clear] {{ clear: both; }}
.keep {{ break-inside: avoid; overflow: hidden; }}
p.label {{ break-after: avoid; }}   /* "note mode:" style labels stay with what follows (marked by attach.js) */
blockquote {{ margin: 4mm 0; padding: 3mm 5mm; background: #f1f6f3; border-left: 3px solid #00954D; border-radius: 0 4px 4px 0; break-inside: avoid; }}
blockquote p {{ margin: 1.5mm 0; }}
table {{ border-collapse: collapse; width: 100%; margin: 3mm 0; font-size: 8.8pt; break-inside: auto; }}
th, td {{ border: 1px solid #c9d0d9; padding: 0.9mm 2.4mm; text-align: left; vertical-align: top; }}
th {{ background: #eef1f5; font-weight: 600; }}
tr {{ break-inside: avoid; }}
ul, ol {{ padding-left: 6mm; }}
li {{ margin: 0.8mm 0; }}
.toc ol {{ list-style: none; padding: 0; columns: 1; font-size: 11pt; }}
.toc li {{ margin: 2.2mm 0; border-bottom: 1px solid #d6dde6; padding-bottom: 1.2mm; }}
.toc a {{ color: #1d232b; font-weight: 400; }}
"""
page = f"""<!doctype html><html><head><meta charset="utf-8"><style>{css}</style></head><body>
<section class="cover">{cover}<div class="sub">8-track MIDI step sequencer - VST3</div></section>
{toc_html}
{rest}
<div class="repo"><div class="label">Vima on GitHub: downloads, source and updates</div><a href="https://github.com/TheoGal/vima-vst">github.com/TheoGal/vima-vst</a></div>
</body></html>"""
work = pathlib.Path(tempfile.mkdtemp(prefix="vima-manual-"))
tmp = work / "manual.html"
# Images: flattened copies (alpha onto white, no ICC profile). Transparency in a PDF makes every
# page a transparency group that viewers like Acrobat redraw slowly while scrolling.
from PIL import Image
flat = work / "images"
flat.mkdir(exist_ok=True)
for f in (repo / "docs" / "images").glob("*.png"):
    im = Image.open(f)
    if im.mode in ("RGBA", "LA", "P"):
        im = im.convert("RGBA")
        bg = Image.new("RGB", im.size, (255, 255, 255))
        bg.paste(im, mask=im.split()[3])
        im = bg
    else:
        im = im.convert("RGB")
    im.save(flat / f.name, optimize=True)
page = page.replace('src="docs/images/', f'src="{flat.as_uri()}/')
tmp.write_text(page, encoding="utf-8")

footer = """<div style="font-family: sans-serif; font-size: 7.5pt; color: #8a94a3; width: 100%; padding: 0 17mm; display: flex; justify-content: space-between;">
<span>Vima User Manual</span><span><span class="pageNumber"></span> / <span class="totalPages"></span></span></div>"""
with sync_playwright() as p:
    b = p.chromium.launch()
    # lay the page out at the printed text width, so measured image sizes match the PDF
    pg = b.new_page(viewport={"width": round((210 - 34) / 25.4 * 96), "height": 1100})
    pg.goto(tmp.as_uri())
    pg.wait_for_load_state("networkidle")
    pg.evaluate((pathlib.Path(__file__).parent / "attach.js").read_text())   # attach each caption to its image
    pg.pdf(path=str(out), format="A4", print_background=True, display_header_footer=True,
           header_template="<div></div>", footer_template=footer,
           margin={"top": "18mm", "bottom": "20mm", "left": "17mm", "right": "17mm"})
    b.close()
print("ok", out)
