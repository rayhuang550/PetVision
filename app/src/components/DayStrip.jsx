import { useEffect, useRef, useState } from "react";
import { localMidnightMs, nowMinutes } from "../time.js";

const DAY_MS = 24 * 60 * 60 * 1000;
const toMin = (hhmm) => { const [h, m] = String(hhmm).split(":").map(Number); return h * 60 + m; };

export default function DayStrip({ pets, logs, tz, colors }) {
  const [, setTick] = useState(0);
  useEffect(() => {
    const t = setInterval(() => setTick((n) => n + 1), 60_000);
    return () => clearInterval(t);
  }, []);

  const midnight = localMidnightMs(tz);
  const petIds = pets ? Object.keys(pets) : [];

  const events = [];
  if (logs) {
    for (const id of petIds) {
      for (const [key, row] of Object.entries(logs[id] || {})) {
        if (row.ts >= midnight) {
          events.push({ key: `${id}/${key}`, ts: row.ts, pct: ((row.ts - midnight) / DAY_MS) * 100, color: colors[id] });
        }
      }
    }
  }
  events.sort((a, b) => a.ts - b.ts);

  const windows = petIds.flatMap((id) =>
    (pets[id].schedule || []).map((w, i) => ({
      key: `${id}/${i}`,
      left: (toMin(w.start) / 1440) * 100,
      width: ((toMin(w.end) - toMin(w.start)) / 1440) * 100,
    })),
  );

  const mountMs = useRef(Date.now());

  return (
    <>
      <div className="strip" aria-label="Today's feed events and schedule windows">
        <div className="strip-track" />
        {windows.map((w) => (
          <div className="strip-window" key={w.key} style={{ left: `${w.left}%`, width: `${w.width}%` }} />
        ))}
        {events.map((e) => (
          <i
            key={e.key}
            className={e.ts > mountMs.current ? "strip-dot live" : "strip-dot"}
            style={{ left: `${e.pct}%`, background: e.color }}
          />
        ))}
        <div className="strip-now" style={{ left: `${(nowMinutes(tz) / 1440) * 100}%` }} />
      </div>
      <div className="strip-hours"><span>0</span><span>6</span><span>12</span><span>18</span><span>24</span></div>
      <div className="legend">
        {petIds.map((id) => (
          <span key={id}><i style={{ background: colors[id] }} />{pets[id].name}</span>
        ))}
      </div>
    </>
  );
}
