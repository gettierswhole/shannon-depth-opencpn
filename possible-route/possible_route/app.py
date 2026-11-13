"""Windows front end for Possible route (beta)."""
import os, sys, json, threading, traceback, queue, subprocess
import tkinter as tk
from tkinter import ttk, filedialog, messagebox
from . import core, places

SETTINGS = os.path.join(core.app_dir(), "settings.json")


def load_settings():
    try:
        return json.load(open(SETTINGS))
    except Exception:
        return {}


def save_settings(s):
    json.dump(s, open(SETTINGS, "w"), indent=1)


def parse_ftin(text):
    """6'2", 6' 2, 6 2, 6.5 -> feet as a float."""
    t = text.replace('"', " ").replace("'", " ").replace("ft", " ").replace("in", " ").split()
    if not t: raise ValueError
    if len(t) == 1: return float(t[0])
    return float(t[0]) + float(t[1]) / 12


class App(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title(f"Possible route {core.VERSION} — Lough Ree")
        self.geometry("760x640")
        self.s = load_settings()
        self.q = queue.Queue()
        pad = {"padx": 8, "pady": 4}

        f = ttk.Frame(self); f.pack(fill="x", **pad)
        ttk.Label(f, text="Your CSIG chart (.mbtiles):").grid(row=0, column=0, sticky="w")
        self.chart = tk.StringVar(value=self.s.get("chart", ""))
        ttk.Entry(f, textvariable=self.chart, width=70).grid(row=1, column=0, sticky="we")
        ttk.Button(f, text="Browse…", command=self.pick_chart).grid(row=1, column=1, padx=4)
        f.columnconfigure(0, weight=1)

        g = ttk.Frame(self); g.pack(fill="x", **pad)
        ttk.Label(g, text="Draught:").grid(row=0, column=0, sticky="w")
        self.draught = tk.StringVar(value=self.s.get("draught", "3'3\""))
        ttk.Entry(g, textvariable=self.draught, width=8).grid(row=0, column=1, sticky="w")
        ttk.Label(g, text="Margin under the keel:").grid(row=0, column=2, sticky="w", padx=(16, 0))
        self.margin = tk.StringVar(value=self.s.get("margin", "2'0\""))
        ttk.Entry(g, textvariable=self.margin, width=8).grid(row=0, column=3, sticky="w")
        ttk.Label(g, text="feet and inches, e.g. 3'3\"").grid(row=0, column=4, sticky="w", padx=8)

        h = ttk.LabelFrame(self, text="Route: start, any stops, finish (top to bottom)"); h.pack(fill="x", **pad)
        self.names = [p[0] for p in places.PLACES] + ["Other (type lat, lon)…"]
        self.rows = []
        self.rowframe = ttk.Frame(h); self.rowframe.pack(fill="x")
        for v in self.s.get("route", [places.PLACES[1][0], places.PLACES[2][0], places.PLACES[3][0]]):
            self.add_row(v)
        b = ttk.Frame(h); b.pack(fill="x", pady=4)
        ttk.Button(b, text="Add stop", command=lambda: self.add_row(places.PLACES[0][0])).pack(side="left")
        ttk.Button(b, text="Remove last", command=self.remove_row).pack(side="left", padx=4)

        k = ttk.Frame(self); k.pack(fill="x", **pad)
        self.go = ttk.Button(k, text="Find a possible route", command=self.run)
        self.go.pack(side="left")
        self.open_btn = ttk.Button(k, text="Open the results folder", command=self.open_folder, state="disabled")
        self.open_btn.pack(side="left", padx=6)

        self.out = tk.Text(self, height=18, wrap="word"); self.out.pack(fill="both", expand=True, **pad)
        self.say("Possible route, not an aid to navigation. It is a suggestion worked out from the chart, "
                 "today's gauge readings and your draught and margin. Check it against the chart and what you see.\n"
                 "Your chart is read on this computer only, and what is read from it stays here.\n")
        self.after(100, self.pump)

    def add_row(self, value):
        fr = ttk.Frame(self.rowframe); fr.pack(fill="x", pady=1)
        var = tk.StringVar(value=value if value in self.names else self.names[-1])
        cb = ttk.Combobox(fr, textvariable=var, values=self.names, width=40, state="readonly"); cb.pack(side="left")
        ll = tk.StringVar(value="" if value in self.names else value)
        en = ttk.Entry(fr, textvariable=ll, width=26); en.pack(side="left", padx=6)
        self.rows.append((fr, var, ll))

    def remove_row(self):
        if len(self.rows) > 2:
            fr, _, _ = self.rows.pop(); fr.destroy()

    def pick_chart(self):
        p = filedialog.askopenfilename(title="Your CSIG Inland Waterways chart", filetypes=[("MBTiles chart", "*.mbtiles"), ("All files", "*.*")])
        if p: self.chart.set(p)

    def say(self, text):
        self.out.insert("end", text + "\n"); self.out.see("end")

    def pump(self):
        try:
            while True:
                kind, val = self.q.get_nowait()
                if kind == "log": self.say(val)
                elif kind == "done":
                    self.go.config(state="normal"); self.open_btn.config(state="normal")
                    res, files = val
                    self.say("\n" + core.summary(res))
                    self.say(f"\nSaved in {core.out_dir()}:\n  {os.path.basename(files['gpx'])} — import into OpenCPN "
                             "(Route & Mark Manager → Import GPX)\n  " + os.path.basename(files['png']) + " — the route over your chart")
                elif kind == "error":
                    self.go.config(state="normal"); self.say("\nProblem: " + val)
        except queue.Empty:
            pass
        self.after(150, self.pump)

    def points(self):
        pts = []
        lookup = {n: (la, lo) for n, la, lo in places.PLACES}
        for _, var, ll in self.rows:
            n = var.get()
            if n in lookup:
                pts.append({"name": n, "lat": lookup[n][0], "lon": lookup[n][1]})
            else:
                try:
                    la, lo = [float(x) for x in ll.get().replace(";", ",").split(",")]
                except Exception:
                    raise ValueError("For 'Other', type latitude, longitude in decimal degrees, e.g. 53.5200, -7.9800")
                if not (53.415 <= la <= 53.745 and -8.12 <= lo <= -7.80):
                    raise ValueError(f"{la}, {lo} is outside Lough Ree, the only area in this beta.")
                pts.append({"name": f"{la:.4f}, {lo:.4f}", "lat": la, "lon": lo})
        if len(pts) < 2: raise ValueError("Give a start and a finish.")
        return pts

    def run(self):
        try:
            chart = self.chart.get().strip().strip('"')
            if not os.path.isfile(chart): raise ValueError("Choose your chart file first (Browse…).")
            d, m = parse_ftin(self.draught.get()), parse_ftin(self.margin.get())
            if not (0.5 <= d <= 8): raise ValueError("Draught should be between 6\" and 8 ft.")
            if not (0 <= m <= 6): raise ValueError("Margin should be between 0 and 6 ft.")
            pts = self.points()
        except ValueError as e:
            messagebox.showwarning("Possible route", str(e)); return
        self.s.update(chart=chart, draught=self.draught.get(), margin=self.margin.get(),
                      route=[var.get() if var.get() != self.names[-1] else ll.get() for _, var, ll in self.rows])
        save_settings(self.s)
        self.go.config(state="disabled")
        self.say(f"\nPlanning {pts[0]['name']} → {pts[-1]['name']}…")

        def work():
            try:
                r = core.find(chart, pts, d, m, log=lambda t: self.q.put(("log", t)))
                self.q.put(("done", r))
            except Exception as e:
                tb = traceback.format_exc()
                with open(os.path.join(core.app_dir(), "last-error.txt"), "w") as fh: fh.write(tb)
                self.q.put(("error", f"{e}\n(details saved in {os.path.join(core.app_dir(), 'last-error.txt')})"))
        threading.Thread(target=work, daemon=True).start()

    def open_folder(self):
        p = core.out_dir()
        if sys.platform == "win32": os.startfile(p)
        else: subprocess.Popen(["xdg-open", p])


def main():
    App().mainloop()
