import { useEffect, useRef, useState } from "react";
import { useParams, useNavigate, Link } from "react-router-dom";
import { ref as dbRef, onValue } from "firebase/database";
import { ref as stRef, uploadBytes, deleteObject } from "firebase/storage";
import { api, db, storage, useValue } from "../../api.js";
import { localMidnightMs } from "../../time.js";
import { formatNumber } from "../../numberFormat.js";
import { petColorMap } from "../petColors.js";
import { usePetPhotos, coverOf, toUprightJpeg, waitForCommand } from "../components/usePetPhotos.js";

export default function PetDetail() {
  const { id } = useParams();
  const navigate = useNavigate();
  const pet = useValue(`pets/${id}`);
  const pets = useValue("pets");
  const hoppers = useValue("hoppers");
  const config = useValue("config");
  const log = useValue(`log/${id}`);
  const { photos, refresh } = usePetPhotos(id);
  const [tab, setTab] = useState("diet");

  if (pet === undefined) return <div className="page"><p className="muted">Loading…</p></div>;
  if (pet === null) return <div className="page"><p className="muted">This pet no longer exists.</p></div>;

  const color = petColorMap(pets)[id];
  const cover = coverOf(photos, pet.thumbnail_photo);
  const today = gramsToday(log, config?.timezone || "America/Toronto");

  return (
    <div className="page">
      <header className="page-head">
        <Link to="/pets" className="iconbtn" aria-label="Back to Pets">‹</Link>
      </header>

      <div className="profile">
        <div className="avatar">
          {cover
            ? <img src={cover.url} alt={pet.name} />
            : <span className="disc" style={{ background: color }}>{(pet.name || "?").slice(0, 1).toUpperCase()}</span>}
        </div>
        <h1>{pet.name}</h1>
        <span className="meta">
          {formatNumber(today)} g today · {formatNumber(pet.target_grams)} g target · {formatNumber(pet.daily_allotment)} g/day
        </span>
      </div>

      <ManualFeed id={id} name={pet.name} pets={pets} />
      <Link className="trend-link" to={`/history?pet=${encodeURIComponent(id)}`}>
        View feeding trends <span aria-hidden="true">→</span>
      </Link>

      <div className="segtabs" role="tablist">
        <button className={`segtab ${tab === "diet" ? "active" : ""}`} role="tab" aria-selected={tab === "diet"} onClick={() => setTab("diet")}>
          Diet
        </button>
        <button className={`segtab ${tab === "photos" ? "active" : ""}`} role="tab" aria-selected={tab === "photos"} onClick={() => setTab("photos")}>
          Photos {photos && <span className="chip">{photos.length}</span>}
        </button>
      </div>

      {tab === "diet" && <DietForm id={id} pet={pet} hoppers={hoppers} />}
      {tab === "photos" && (
        <Photos
          id={id}
          name={pet.name}
          photos={photos}
          refresh={refresh}
          thumbnailPhoto={pet.thumbnail_photo}
        />
      )}

      <button
        className="textbtn danger wide"
        onClick={async () => {
          if (confirm(`Remove ${pet.name}? Its intake history is kept; the Feeder drops its Gallery entries.`)) {
            await api.removePet(id);
            navigate("/pets");
          }
        }}
      >
        Remove {pet.name}
      </button>
    </div>
  );
}

function gramsToday(log, tz) {
  if (!log) return 0;
  const midnight = localMidnightMs(tz);
  let grams = 0;
  for (const row of Object.values(log)) if (row.ts >= midnight) grams += row.dispensed || 0;
  return grams;
}

function ManualFeed({ id, name, pets }) {
  const [cmdKey, setCmdKey] = useState(null);
  const [outcome, setOutcome] = useState(null);
  const [busy, setBusy] = useState(false);

  useEffect(() => {
    if (!cmdKey) return;
    return onValue(dbRef(db, `commands/${cmdKey}`), (snap) => {
      const cmd = snap.val();
      if (cmd && cmd.status && cmd.status !== "pending") {
        setOutcome({
          status: cmd.status,
          reason: cmd.reason,
          grams: cmd.grams,
          blockingPet: cmd.blocking_pet,
        });
        setBusy(false);
      }
    });
  }, [cmdKey]);

  async function feed() {
    setBusy(true);
    setOutcome(null);
    try {
      const key = await api.createFeed(id);
      setCmdKey(key);
    } catch (e) {
      setBusy(false);
      console.error(e);
    }
  }
  async function dismiss() {
    if (cmdKey) await api.clearCommand(cmdKey);
    setCmdKey(null);
    setOutcome(null);
    setBusy(false);
  }

  let label = `Feed ${name}`;
  if (cmdKey && !outcome) label = "Feeding… (waiting for Feeder)";

  return (
    <section>
      <button className="pill fill wide" onClick={feed} disabled={busy || (!!cmdKey && !outcome)}>{label}</button>
      {outcome && (
        <div className={`outcome ${outcome.status}`}>
          {outcome.status === "done" && <span>● Fed {formatNumber(outcome.grams, "?")} g</span>}
          {outcome.status === "held" && <span>● Held: {reasonText(outcome, pets)}</span>}
          {outcome.status === "failed" && <span>● Failed: {reasonText(outcome, pets)}</span>}
          <button className="textbtn" onClick={dismiss}>Dismiss</button>
        </div>
      )}
    </section>
  );
}

function DietForm({ id, pet, hoppers }) {
  const [draft, setDraft] = useState(pet);
  const [saved, setSaved] = useState(false);
  useEffect(() => setDraft(pet), [pet]);

  const set = (k, v) => { setDraft((d) => ({ ...d, [k]: v })); setSaved(false); };
  const setWindow = (i, k, v) => {
    const schedule = [...(draft.schedule || [])];
    schedule[i] = { ...schedule[i], [k]: v };
    set("schedule", schedule);
  };
  const addWindow = () => set("schedule", [...(draft.schedule || []), { start: "07:00", end: "09:00" }]);
  const removeWindow = (i) => set("schedule", (draft.schedule || []).filter((_, j) => j !== i));

  async function save() {
    await api.updatePet(id, {
      name: draft.name,
      hopper: draft.hopper,
      target_grams: Number(draft.target_grams),
      daily_allotment: Number(draft.daily_allotment),
      schedule: (draft.schedule || []).map((w) => ({ start: w.start, end: w.end })),
    });
    setSaved(true);
  }

  const warnTarget = Number(draft.target_grams) > Number(draft.daily_allotment);

  const numOk = (v) => v !== "" && v != null && Number.isFinite(Number(v)) && Number(v) >= 0;
  const toMin = (hhmm) => { const [h, m] = String(hhmm).split(":").map(Number); return h * 60 + m; };
  const numbersValid = numOk(draft.target_grams) && numOk(draft.daily_allotment);
  const scheduleValid = (draft.schedule || []).every((w) => toMin(w.end) > toMin(w.start));
  const invalidMsg = !numbersValid
    ? "⚠ Target/Allotment must be numbers ≥ 0"
    : !scheduleValid
    ? "⚠ Schedule window end must be after start"
    : null;

  return (
    <section>
      <label>Name
        <input value={draft.name ?? ""} onChange={(e) => set("name", e.target.value)} />
      </label>
      <label>Hopper
        <select value={draft.hopper ?? ""} onChange={(e) => set("hopper", e.target.value)}>
          {hoppers && Object.entries(hoppers).map(([hid, h]) => (
            <option key={hid} value={hid}>Hopper {hid} · {h.food}</option>
          ))}
        </select>
      </label>
      <div className="two">
        <label>Target (g)
          <input type="number" value={draft.target_grams ?? ""} onChange={(e) => set("target_grams", e.target.value)} />
        </label>
        <label>Allotment (g/day)
          <input type="number" value={draft.daily_allotment ?? ""} onChange={(e) => set("daily_allotment", e.target.value)} />
        </label>
      </div>
      {warnTarget && <p className="warn">⚠ Target exceeds the daily allotment. A full portion can't be dispensed.</p>}

      <div className="sched">
        <span>Schedule (allowed windows)</span>
        {(draft.schedule || []).length === 0 && <p className="muted small">No windows. Feeding is always allowed.</p>}
        {(draft.schedule || []).map((w, i) => (
          <div className="window" key={i}>
            <input type="time" value={w.start} onChange={(e) => setWindow(i, "start", e.target.value)} />
            <span>–</span>
            <input type="time" value={w.end} onChange={(e) => setWindow(i, "end", e.target.value)} />
            <button className="textbtn" onClick={() => removeWindow(i)}>✕</button>
          </div>
        ))}
        <button className="linkbtn" onClick={addWindow}>+ add window</button>
      </div>

      {invalidMsg && <p className="warn">{invalidMsg}</p>}
      <button className="pill fill wide" onClick={save} disabled={!!invalidMsg}>{saved ? "Saved ✓" : "Save diet"}</button>
    </section>
  );
}

function Photos({ id, name, photos, refresh, thumbnailPhoto }) {
  const [busy, setBusy] = useState(false);
  const [activePath, setActivePath] = useState(null);

  async function add(e) {
    const files = [...e.target.files];
    if (!files.length) return;
    setBusy(true);
    try {
      const paths = [];
      for (const f of files) {
        const path = `enroll/${id}/${Date.now()}-${crypto.randomUUID()}.jpg`;
        const upright = await toUprightJpeg(f);
        await uploadBytes(stRef(storage, path), upright, { contentType: "image/jpeg" });
        paths.push(path);
      }
      const key = await api.createEnroll(id, paths);
      await refresh();
      const resolved = await waitForCommand(key);
      if (resolved) await api.clearCommand(key);
      await refresh();
    } finally {
      setBusy(false);
      e.target.value = "";
    }
  }
  async function remove(path) {
    if (path === thumbnailPhoto) await api.setPetThumbnail(id, null);
    await deleteObject(stRef(storage, path)).catch(() => {});
    await api.createUnenroll(id, [path]);
    if (activePath === path) setActivePath(null);
    await refresh();
  }

  async function chooseThumbnail(path) {
    await api.setPetThumbnail(id, path);
  }

  if (photos === null) return <section><p className="muted">Loading…</p></section>;

  const embedded = photos.filter((p) => p.petsDetected !== null);
  const detTitle = (n) => n === null ? "processing…" : n === 1 ? "1 pet detected" : n === 0 ? "no pet detected" : `${n} pets detected`;
  const detClass = (n) => n === null ? "det-pending" : n === 1 ? "det-ok" : "det-warn";
  const active = photos.find((p) => p.path === activePath) || null;

  return (
    <section>
      {photos.length === 0 && <p className="warn">⚠ 0 photos. An empty Gallery never matches.</p>}
      {photos.length > 0 && embedded.length === 0 && <p className="warn">⚠ No photo has a confirmed pet yet.</p>}
      <label className="photo-add">
        <span>{busy ? "Adding photos…" : "+ Add photos"}</span>
        <input type="file" accept="image/*" multiple hidden disabled={busy} onChange={add} />
      </label>
      <div className="photos" aria-label={`${name}'s reference photos`}>
        {photos.map((p) => (
          <div className={`photo ${detClass(p.petsDetected)}`} key={p.path} title={detTitle(p.petsDetected)}>
            <button
              className="photo-open"
              onClick={() => p.url && setActivePath(p.path)}
              aria-label={`Expand ${p.name}`}
              disabled={!p.url}
            >
              {p.url ? <img src={p.url} alt="" /> : <div className="ph" />}
            </button>
            {p.path === thumbnailPhoto && <span className="cover-badge">Home thumbnail</span>}
            <button className="x" onClick={() => remove(p.path)} aria-label={`Delete ${p.name}`}>✕</button>
          </div>
        ))}
      </div>
      <p className="muted small">
        Photos are cropped to the detected Pet. <span className="det-ok-text">Green</span> = one Pet detected;
        <span className="det-warn-text"> red</span> = none or several (consider removing).
      </p>
      {active && (
        <PhotoLightbox
          photo={active}
          petName={name}
          selected={active.path === thumbnailPhoto}
          detectionLabel={detTitle(active.petsDetected)}
          onClose={() => setActivePath(null)}
          onChoose={() => chooseThumbnail(active.path)}
          onRemove={() => remove(active.path)}
        />
      )}
    </section>
  );
}

function PhotoLightbox({ photo, petName, selected, detectionLabel, onClose, onChoose, onRemove }) {
  const closeRef = useRef(null);
  const panelRef = useRef(null);

  useEffect(() => {
    const oldOverflow = document.body.style.overflow;
    document.body.style.overflow = "hidden";
    closeRef.current?.focus();
    const onKey = (e) => {
      if (e.key === "Escape") onClose();
      if (e.key === "Tab") {
        const focusable = [...panelRef.current.querySelectorAll("button:not(:disabled)")];
        if (!focusable.length) return;
        const first = focusable[0], last = focusable.at(-1);
        if (e.shiftKey && document.activeElement === first) {
          e.preventDefault();
          last.focus();
        } else if (!e.shiftKey && document.activeElement === last) {
          e.preventDefault();
          first.focus();
        }
      }
    };
    document.addEventListener("keydown", onKey);
    return () => {
      document.body.style.overflow = oldOverflow;
      document.removeEventListener("keydown", onKey);
    };
  }, [onClose]);

  return (
    <div className="lightbox" role="dialog" aria-modal="true" aria-label={`${petName} reference photo`}>
      <button className="lightbox-backdrop" onClick={onClose} aria-label="Close expanded photo" />
      <div className="lightbox-panel" ref={panelRef}>
        <div className="lightbox-head">
          <div>
            <strong>{petName}</strong>
            <span>{detectionLabel}</span>
          </div>
          <button ref={closeRef} className="lightbox-close" onClick={onClose} aria-label="Close">✕</button>
        </div>
        <div className="lightbox-image">
          <img src={photo.url} alt={`${petName} reference`} />
        </div>
        <div className="lightbox-actions">
          <button className={`pill ${selected ? "ghost" : "fill"}`} onClick={onChoose} disabled={selected}>
            {selected ? "Home thumbnail ✓" : "Use as home thumbnail"}
          </button>
          <button className="textbtn danger" onClick={onRemove}>Delete photo</button>
        </div>
      </div>
    </div>
  );
}

function reasonText(outcome, pets) {
  const { reason, grams, blockingPet } = outcome;
  const blockingName = blockingPet ? pets?.[blockingPet]?.name : null;
  if (reason === "bowl_not_empty") {
    const amount = grams != null ? `${formatNumber(grams)} g of ` : "";
    return blockingName
      ? `${amount}${blockingName}'s food is in the bowl`
      : `${amount}food with an unknown owner is in the bowl`;
  }
  if (reason === "portion_in_bowl") {
    const amount = grams != null ? `${formatNumber(grams)} g` : "the portion";
    return `${amount} is already in the bowl`;
  }
  const map = {
    allotment_reached: "daily limit reached",
    hardware_not_ready: "the Feeder is not commissioned yet",
    vision_host_unreachable: "Vision Host unreachable",
    outside_schedule: "outside the allowed schedule",
    offline: "Feeder couldn't verify today's total",
  };
  return map[reason] || reason || "no reason given";
}
