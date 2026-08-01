import { useEffect, useMemo, useState } from "react";
import { useSearchParams } from "react-router-dom";
import { api, useValue } from "../../api.js";
import { formatNumber } from "../../numberFormat.js";
import { petColorMap } from "../petColors.js";
import { intakeStats, localDateKey, trendLabel } from "../historyStats.js";

export default function History() {
  const pets = useValue("pets");
  const logs = useValue("log");
  const config = useValue("config");
  const [search, setSearch] = useSearchParams();
  const [days, setDays] = useState(7);

  const entries = pets ? Object.entries(pets) : [];
  const requested = search.get("pet");
  const selectedId = entries.some(([id]) => id === requested) ? requested : entries[0]?.[0] || null;
  const colors = petColorMap(pets);
  const tz = config?.timezone || "America/Toronto";

  useEffect(() => {
    if (selectedId && requested !== selectedId) setSearch({ pet: selectedId }, { replace: true });
  }, [requested, selectedId, setSearch]);

  const selectedPet = selectedId ? pets?.[selectedId] : null;
  const selectedStats = useMemo(
    () => selectedId ? intakeStats(logs?.[selectedId], tz, days) : null,
    [selectedId, logs, tz, days],
  );

  function choosePet(id) {
    setSearch({ pet: id }, { replace: true });
  }

  return (
    <div className="page">
      <header className="page-head">
        <h1>Feeding trends</h1>
        <div className="range-toggle" aria-label="History range">
          {[7, 30].map((n) => (
            <button key={n} className={days === n ? "active" : ""} onClick={() => setDays(n)}>
              {n}D
            </button>
          ))}
        </div>
      </header>

      {(pets === undefined || logs === undefined) && <p className="muted">Loading…</p>}
      {pets === null && <p className="muted">No pets yet. The Intake Log fills in as meals are dispensed.</p>}

      {entries.length > 0 && (
        <>
          <p className="sect">Across your pets</p>
          <div className="pet-comparison">
            {entries.map(([id, pet]) => {
              const stats = intakeStats(logs?.[id], tz, days);
              return (
                <button
                  key={id}
                  className={`comparison-card ${selectedId === id ? "active" : ""}`}
                  onClick={() => choosePet(id)}
                  aria-pressed={selectedId === id}
                >
                  <span className="comparison-name">
                    <i style={{ background: colors[id] }} />
                    {pet.name}
                  </span>
                  <strong>{formatNumber(stats.avgDispensed)} g/day</strong>
                  <span>{formatNumber(stats.mealsPerDay)} meals/day</span>
                  <small>{trendLabel(stats)}</small>
                </button>
              );
            })}
          </div>
        </>
      )}

      {selectedPet && selectedStats && (
        <PetTrends
          id={selectedId}
          pet={selectedPet}
          stats={selectedStats}
          tz={tz}
          color={colors[selectedId]}
        />
      )}
    </div>
  );
}

function PetTrends({ id, pet, stats, tz, color }) {
  return (
    <>
      <div className="trend-heading">
        <div>
          <p className="sect">Observed intake</p>
          <h2>{pet.name}</h2>
        </div>
        <span>{stats.days} days</span>
      </div>

      <DailyChart daily={stats.daily} color={color} />

      <div className="metric-grid">
        <Metric value={`${formatNumber(stats.avgDispensed)} g`} label="Avg dispensed / day" />
        <Metric value={formatNumber(stats.mealsPerDay)} label="Meals / day" />
        <Metric value={`${formatNumber(stats.averageMeal)} g`} label="Avg meal" />
        <Metric
          value={stats.eatenRate == null ? "N/A" : `${formatNumber(stats.eatenRate)}%`}
          label="Eaten / dispensed"
        />
      </div>
      {stats.totals.unsettledMeals > 0 && (
        <p className="metric-note">
          {stats.totals.unsettledMeals} unsettled {stats.totals.unsettledMeals === 1 ? "meal is" : "meals are"} excluded.
        </p>
      )}

      <p className="sect">Intake Log</p>
      <LogRows id={id} name={pet.name} rows={stats.rows} tz={tz} />
    </>
  );
}

function Metric({ value, label }) {
  return (
    <div className="metric">
      <strong>{value}</strong>
      <span>{label}</span>
    </div>
  );
}

function DailyChart({ daily, color }) {
  const max = Math.max(1, ...daily.flatMap((day) => [day.dispensed, day.eaten]));

  return (
    <div className="chart-shell">
      <div className="chart-legend">
        <span><i style={{ background: color }} />Dispensed</span>
        <span><i className="eaten-key" />Eaten</span>
      </div>
      <div className={`daily-chart ${daily.length > 7 ? "wide" : ""}`}>
        {daily.map((day) => {
          const dispensedHeight = (day.dispensed / max) * 100;
          const eatenHeight = (day.eaten / max) * 100;
          const label = `${formatDay(day.key, true)}: ${formatNumber(day.dispensed)} g dispensed, ${formatNumber(day.eaten)} g eaten${day.unsettledMeals ? `, ${day.unsettledMeals} unsettled` : ""}`;
          return (
            <div className="chart-day" key={day.key} aria-label={label} title={label}>
              <div className="bar-pair">
                <i className="bar dispensed" style={{ height: `${dispensedHeight}%`, background: color }} />
                <i className="bar eaten" style={{ height: `${eatenHeight}%` }} />
              </div>
              <span>{formatChartDay(day.key, daily.length <= 7)}</span>
            </div>
          );
        })}
      </div>
    </div>
  );
}

function LogRows({ id, name, rows, tz }) {
  async function remove(key, row) {
    const when = formatWhen(row.ts, tz, true);
    if (!window.confirm(`Delete this ${name} entry (${when}, ${formatNumber(row.dispensed)} g)?`)) return;
    await api.removeLogEntry(id, key);
  }

  if (rows.length === 0) return <p className="muted small">No meals logged in this period.</p>;

  let previousDay = null;
  return (
    <div className="log-days">
      {rows.map(([key, row]) => {
        const day = localDateKey(tz, row.ts);
        const showDay = day !== previousDay;
        previousDay = day;
        return (
          <div key={key}>
            {showDay && <p className="log-day">{formatDay(day, true)}</p>}
            <div className="log-row">
              <span className="grow">
                {formatWhen(row.ts, tz)}
                <span className="sub">
                  {row.eaten == null ? "Eaten N/A · unsettled" : `Eaten ${formatNumber(row.eaten)} g`}
                </span>
              </span>
              <span className="end">{formatNumber(row.dispensed)} g</span>
              <button className="del" onClick={() => remove(key, row)} aria-label={`Delete ${formatWhen(row.ts, tz)} entry`}>✕</button>
            </div>
          </div>
        );
      })}
    </div>
  );
}

function dateFromKey(key) {
  const [year, month, day] = key.split("-").map(Number);
  return new Date(Date.UTC(year, month - 1, day));
}

function formatDay(key, weekday) {
  return dateFromKey(key).toLocaleDateString(undefined, {
    month: "short",
    day: "numeric",
    ...(weekday ? { weekday: "short" } : {}),
    timeZone: "UTC",
  });
}

function formatChartDay(key, weekly) {
  return dateFromKey(key).toLocaleDateString(undefined, weekly
    ? { weekday: "short", timeZone: "UTC" }
    : { day: "numeric", timeZone: "UTC" });
}

function formatWhen(ts, tz, full = false) {
  try {
    new Intl.DateTimeFormat("en-US", { timeZone: tz });
  } catch {
    tz = "UTC";
  }
  return new Date(ts).toLocaleString(undefined, full
    ? { dateStyle: "medium", timeStyle: "short", timeZone: tz }
    : { hour: "numeric", minute: "2-digit", timeZone: tz });
}
