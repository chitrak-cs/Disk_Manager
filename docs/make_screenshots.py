#!/usr/bin/env python3
"""Run the real ./build/sysmon and render its ANSI output as SVG 'terminal screenshots'.

Usage: python3 docs/make_screenshots.py
"""
import html, re, socket, subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "docs" / "images"
BIN = ROOT / "build" / "sysmon"

CW, LH, PAD, TOP = 8.4, 19, 18, 44          # char width, line height, padding, title bar
FG, BG = "#d4d4d4", "#1e1e1e"
COLORS = {"32": "#4ec9b0", "33": "#e5c07b", "31": "#f44747"}
ANSI = re.compile(r"\x1b\[([0-9;]*)m")

def run(args, host_alias="my-laptop"):
    out = subprocess.run([str(BIN), *args], capture_output=True, text=True, check=True).stdout
    return out.replace(socket.gethostname(), host_alias)   # keep the real hostname out of the README

def esc(t):
    """Escape for SVG; use NBSP so column alignment survives (SVG collapses spaces)."""
    return html.escape(t).replace(" ", "\u00a0")

def spans(line):
    """Yield (text, colour) pairs, plus bold for section headers."""
    pos, col = 0, FG
    for m in ANSI.finditer(line):
        if m.start() > pos: yield line[pos:m.start()], col
        col = FG if m.group(1) in ("0", "") else COLORS.get(m.group(1), col)
        pos = m.end()
    if pos < len(line): yield line[pos:], col

def render(cmd, text, name):
    lines = text.rstrip("\n").split("\n")
    width = max(len(ANSI.sub("", l)) for l in [f"$ {cmd}"] + lines)
    w, h = int(width * CW + 2 * PAD), int(TOP + (len(lines) + 1) * LH + PAD)
    o = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" viewBox="0 0 {w} {h}" role="img" aria-label="{html.escape(cmd)}">',
         f'<rect width="{w}" height="{h}" rx="10" fill="{BG}"/>',
         f'<rect width="{w}" height="30" rx="10" fill="#323233"/><rect y="20" width="{w}" height="10" fill="#323233"/>',
         '<circle cx="18" cy="15" r="6" fill="#ff5f56"/><circle cx="38" cy="15" r="6" fill="#ffbd2e"/><circle cx="58" cy="15" r="6" fill="#27c93f"/>',
         f'<text x="{w/2}" y="20" fill="#9d9d9d" font-family="sans-serif" font-size="12" text-anchor="middle">sysmon</text>',
         '<g font-family="\'DejaVu Sans Mono\',Menlo,Consolas,monospace" font-size="14">']
    y = TOP + LH
    o.append(f'<text x="{PAD}" y="{y}"><tspan fill="#6a9955">$ </tspan><tspan fill="#ffffff">{html.escape(cmd)}</tspan></text>')
    for l in lines:
        y += LH
        if l.startswith("== "):
            o.append(f'<text x="{PAD}" y="{y}" fill="#569cd6" font-weight="bold">{esc(l)}</text>'); continue
        parts = "".join(f'<tspan fill="{c}">{esc(t)}</tspan>' for t, c in spans(l))
        o.append(f'<text x="{PAD}" y="{y}" fill="{FG}">{parts}</text>')
    o.append("</g></svg>")
    (OUT / name).write_text("\n".join(o))
    print("wrote", name)

SHOTS = [
    ("sysmon",            ["--color"],                  "full-report.svg"),
    ("sysmon --disk",     ["--color", "--disk"],        "disk.svg"),
    ("sysmon --ram",      ["--color", "--ram"],         "ram.svg"),
    ("sysmon --gpu",      ["--color", "--gpu"],         "gpu.svg"),
    ("sysmon --help",     ["--help"],                   "help.svg"),
]
if __name__ == "__main__":
    OUT.mkdir(parents=True, exist_ok=True)
    for cmd, args, name in SHOTS:
        render(cmd, run(args), name)
    # JSON: pretty-print the real output so it fits a screenshot
    import json
    data = json.loads(run(["--json", "--disk", "--ram"]))
    render("sysmon --json --disk --ram", json.dumps(data, indent=2), "json.svg")
