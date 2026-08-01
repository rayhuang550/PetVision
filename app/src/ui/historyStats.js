import { formatNumber } from "../numberFormat.js";

function safeTz(tz) {
  try {
    new Intl.DateTimeFormat("en-US", { timeZone: tz });
    return tz;
  } catch {
    return "UTC";
  }
}

export function localDateKey(tz, ts) {
  const parts = new Intl.DateTimeFormat("en-US", {
    timeZone: safeTz(tz),
    year: "numeric",
    month: "2-digit",
    day: "2-digit",
  }).formatToParts(new Date(ts)).reduce((out, part) => {
    out[part.type] = part.value;
    return out;
  }, {});
  return `${parts.year}-${parts.month}-${parts.day}`;
}

function shiftDateKey(key, deltaDays) {
  const [year, month, day] = key.split("-").map(Number);
  const shifted = new Date(Date.UTC(year, month - 1, day + deltaDays));
  return shifted.toISOString().slice(0, 10);
}

function dateKeysEndingAt(endKey, days) {
  return Array.from({ length: days }, (_, index) => shiftDateKey(endKey, index - days + 1));
}

function summarize(log, keys, tz) {
  const allowed = new Set(keys);
  const byDay = new Map(keys.map((key) => [key, {
    key,
    dispensed: 0,
    eaten: 0,
    meals: 0,
    settledMeals: 0,
    unsettledMeals: 0,
    settledDispensed: 0,
  }]));
  const rows = [];

  for (const [id, row] of Object.entries(log || {})) {
    const key = row?.ts == null ? null : localDateKey(tz, row.ts);
    if (!key || !allowed.has(key)) continue;
    const day = byDay.get(key);
    const dispensed = Number(row.dispensed) || 0;
    day.dispensed += dispensed;
    day.meals += 1;
    if (row.eaten == null) {
      day.unsettledMeals += 1;
    } else {
      day.eaten += Number(row.eaten) || 0;
      day.settledDispensed += dispensed;
      day.settledMeals += 1;
    }
    rows.push([id, row]);
  }

  const daily = [...byDay.values()];
  const totals = daily.reduce((out, day) => ({
    dispensed: out.dispensed + day.dispensed,
    eaten: out.eaten + day.eaten,
    meals: out.meals + day.meals,
    settledMeals: out.settledMeals + day.settledMeals,
    unsettledMeals: out.unsettledMeals + day.unsettledMeals,
    settledDispensed: out.settledDispensed + day.settledDispensed,
  }), { dispensed: 0, eaten: 0, meals: 0, settledMeals: 0, unsettledMeals: 0, settledDispensed: 0 });

  return { daily, totals, rows: rows.sort((a, b) => b[1].ts - a[1].ts) };
}

export function intakeStats(log, tz, days = 7, now = Date.now()) {
  const periodDays = days === 30 ? 30 : 7;
  const today = localDateKey(tz, now);
  const currentKeys = dateKeysEndingAt(today, periodDays);
  const priorEnd = shiftDateKey(currentKeys[0], -1);
  const priorKeys = dateKeysEndingAt(priorEnd, periodDays);

  const validTz = safeTz(tz);
  const current = summarize(log, currentKeys, validTz);
  const prior = summarize(log, priorKeys, validTz);
  const avgDispensed = current.totals.dispensed / periodDays;
  const priorAvgDispensed = prior.totals.dispensed / periodDays;

  let changePct = null;
  if (priorAvgDispensed > 0) {
    changePct = ((avgDispensed - priorAvgDispensed) / priorAvgDispensed) * 100;
  }

  return {
    days: periodDays,
    daily: current.daily,
    rows: current.rows,
    totals: current.totals,
    avgDispensed,
    priorAvgDispensed,
    changePct,
    mealsPerDay: current.totals.meals / periodDays,
    averageMeal: current.totals.meals ? current.totals.dispensed / current.totals.meals : 0,
    eatenRate: current.totals.settledDispensed
      ? (current.totals.eaten / current.totals.settledDispensed) * 100
      : null,
  };
}

export function trendLabel(stats) {
  if (stats.priorAvgDispensed === 0) {
    return stats.avgDispensed > 0 ? "New activity vs prior period" : "No change vs prior period";
  }
  if (Math.abs(stats.changePct) < 0.005) return "No change vs prior period";
  return `${stats.changePct > 0 ? "+" : ""}${formatNumber(stats.changePct)}% vs prior period`;
}
