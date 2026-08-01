import { useState } from "react";
import { Link } from "react-router-dom";
import { api, useValue } from "../../api.js";
import { localMidnightMs, eligibility, isFeederOnline } from "../../time.js";
import { petColorMap } from "../petColors.js";
import DayStrip from "../../components/DayStrip.jsx";
import KibbleGauge from "../../components/KibbleGauge.jsx";
import { formatNumber, formatProgressNumber } from "../../numberFormat.js";
import PetPhoto from "../components/PetPhoto.jsx";

export default function Home() {
  const status = useValue("status/feeder");
  const config = useValue("config");
  const pets = useValue("pets");
  const logs = useValue("log");

  const tz = config?.timezone || "America/Toronto";
  const online = isFeederOnline(status);
  const petEntries = pets ? Object.entries(pets) : [];
  const colors = petColorMap(pets);

  return (
    <div className="page">
      <header className="page-head">
        <span className="brand">PetVision</span>
        <span className={`presence ${online ? "ok" : "bad"}`}>
          <i />{online ? "Feeder online" : "Feeder offline"}
        </span>
      </header>

      {pets === undefined || logs === undefined
        ? <h1 className="hero dim">…</h1>
        : <Headline online={online} pets={pets} logs={logs} tz={tz} />}

      {status?.hold?.reason === "bowl_not_empty" && (
        <BowlConflict hold={status.hold} pets={pets} />
      )}
      {online && status?.ready === false && (
        <section className="bowl-conflict hardware-readiness" aria-live="polite">
          <div className="bowl-conflict-mark" aria-hidden="true">i</div>
          <div>
            <strong>Hardware setup is required.</strong>
            <p>
              The Feeder needs to be commissioned over its serial console before it can
              dispense.
            </p>
          </div>
        </section>
      )}

      <p className="sect">Today</p>
      <DayStrip pets={pets} logs={logs} tz={tz} colors={colors} />

      <section className="feedcards">
        {pets === null && <p className="muted">No pets yet. Add one under Pets.</p>}
        {petEntries.map(([id, pet]) => (
          <PetCard key={id} id={id} pet={pet} log={logs?.[id]} tz={tz} online={online} color={colors[id]} />
        ))}
      </section>

    </div>
  );
}

function BowlConflict({ hold, pets }) {
  const [clearing, setClearing] = useState(false);
  const blockingName = hold.pet ? pets?.[hold.pet]?.name : null;
  const grams = Number.isFinite(Number(hold.grams)) ? `${formatNumber(hold.grams)} g` : "Food";
  const detail = blockingName
    ? `${grams} of ${blockingName}'s food is still in the bowl.`
    : `${grams} is still in the bowl and its owner is unknown.`;

  async function affirmClear() {
    setClearing(true);
    try {
      await api.clearFeederHold();
    } finally {
      setClearing(false);
    }
  }

  return (
    <section className="bowl-conflict" aria-live="polite">
      <div className="bowl-conflict-mark" aria-hidden="true">!</div>
      <div>
        <strong>Clear the bowl before the next Pet.</strong>
        <p>{detail} The Feeder will not mix diets.</p>
      </div>
      <button className="pill ghost small" onClick={affirmClear} disabled={clearing}>
        {clearing ? "Clearing…" : "Bowl is clear"}
      </button>
    </section>
  );
}

function Headline({ online, pets, logs, tz }) {
  const dateLine = new Date().toLocaleDateString(undefined, { weekday: "long", month: "long", day: "numeric" });

  let line;
  if (!online) line = "Feeder offline.";
  else if (!pets) line = "No pets enrolled.";
  else {
    const midnight = localMidnightMs(tz);
    let meals = 0;
    for (const id of Object.keys(pets)) {
      for (const row of Object.values(logs?.[id] || {})) if (row.ts >= midnight) meals++;
    }
    line = meals === 0 ? "No meals yet today." : meals === 1 ? "1 meal dispensed today." : `${meals} meals dispensed today.`;
  }

  return (
    <h1 className="hero">
      {line}
      <br />
      <span className="dim">{dateLine}</span>
    </h1>
  );
}

function PetCard({ id, pet, log, tz, online, color }) {
  const { grams: today, lastFed } = summarizeDay(log, tz);
  const elig = eligibility(pet.schedule, tz);

  const fedLine = lastFed ? `fed ${relTime(lastFed.ts)} · ${formatNumber(lastFed.dispensed)} g` : "no meals logged yet";
  return (
    <Link to={`/pets/${id}`} className="feedcard">
      <div className="tile-img home-photo">
        <PetPhoto id={id} name={pet.name} color={color} thumbnailPhoto={pet.thumbnail_photo} />
      </div>
      <div className="feedcard-head">
        <span className="pet-name">{pet.name}</span>
        <span className="grams"><b>{formatProgressNumber(today)}</b> <span>/ {formatNumber(pet.daily_allotment)} g</span></span>
      </div>
      <div className="feedcard-gauge">
        <KibbleGauge dispensed={today} allotment={pet.daily_allotment} color={color} />
      </div>
      <p className="status">
        {!online
          ? "Feeding paused: Feeder offline"
          : <>{elig.allowed && <span className="yes">✓ </span>}{elig.label} · {fedLine}</>}
      </p>
    </Link>
  );
}

function summarizeDay(log, tz) {
  if (!log) return { grams: 0, lastFed: null };
  const midnight = localMidnightMs(tz);
  let grams = 0, lastFed = null;
  for (const row of Object.values(log)) {
    if (row.ts >= midnight) grams += row.dispensed || 0;
    if (!lastFed || row.ts > lastFed.ts) lastFed = row;
  }
  return { grams, lastFed };
}

function relTime(ts) {
  const mins = Math.round((Date.now() - ts) / 60000);
  if (mins < 1) return "just now";
  if (mins < 60) return `${mins}m ago`;
  const h = Math.round(mins / 60);
  if (h < 24) return `${h}h ago`;
  return `${Math.round(h / 24)}d ago`;
}
