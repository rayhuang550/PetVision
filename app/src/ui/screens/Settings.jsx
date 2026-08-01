import { useEffect, useState } from "react";
import { useValue } from "../../api.js";
import { useAuth } from "../../auth.jsx";
import { api } from "../../api.js";

export default function Settings() {
  const config = useValue("config");
  const hoppers = useValue("hoppers");
  const { logout } = useAuth();

  const [tz, setTz] = useState("");
  useEffect(() => { if (config) setTz(config.timezone || ""); }, [config]);

  const tzValid = !tz || isValidTz(tz);

  return (
    <div className="page">
      <header className="page-head"><h1>Settings</h1></header>

      <p className="sect">Home</p>
      <label>Timezone (IANA)
        <input value={tz} onChange={(e) => setTz(e.target.value)} placeholder="America/Toronto" onBlur={() => tz && tzValid && api.setTimezone(tz)} />
      </label>
      {!tzValid && <p className="warn">⚠ Not a recognized IANA timezone. Changes were not saved.</p>}

      <p className="sect">Hopper foods</p>
      {hoppers && Object.entries(hoppers).map(([id, h]) => (
        <HopperRow key={id} id={id} food={h.food} />
      ))}

      <p className="sect">Account</p>
      <div className="group">
        <button className="row danger" onClick={logout}>Sign out</button>
      </div>
    </div>
  );
}

function isValidTz(tz) {
  try { new Intl.DateTimeFormat("en-US", { timeZone: tz }); return true; }
  catch { return false; }
}

function HopperRow({ id, food }) {
  const [val, setVal] = useState(food);
  useEffect(() => setVal(food), [food]);
  return (
    <label>Hopper {id}
      <input value={val} onChange={(e) => setVal(e.target.value)} onBlur={() => val !== food && api.setHopperFood(id, val)} />
    </label>
  );
}
