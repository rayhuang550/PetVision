import { Link, useNavigate } from "react-router-dom";
import { api, useValue } from "../../api.js";
import { formatNumber } from "../../numberFormat.js";
import { petColorMap } from "../petColors.js";
import PetPhoto from "../components/PetPhoto.jsx";

export default function Pets() {
  const pets = useValue("pets");
  const hoppers = useValue("hoppers");
  const navigate = useNavigate();
  const colors = petColorMap(pets);

  async function addPet() {
    const firstHopper = hoppers ? Object.keys(hoppers)[0] : null;
    const id = await api.createPet({
      name: "New pet",
      hopper: firstHopper,
      target_grams: 20,
      daily_allotment: 60,
      schedule: [],
    });
    navigate(`/pets/${id}`);
  }

  const entries = pets ? Object.entries(pets) : [];

  return (
    <div className="page">
      <header className="page-head">
        <h1>Pets</h1>
        <button className="pill fill small" onClick={addPet}>Add pet</button>
      </header>

      {pets === undefined && <p className="muted">Loading…</p>}
      {pets === null && <p className="muted">No pets yet. Add one so the Feeder can identify it.</p>}

      <div className="petgrid">
        {entries.map(([id, pet]) => (
          <Link to={`/pets/${id}`} className="pettile" key={id}>
            <div className="tile-img">
              <PetPhoto id={id} name={pet.name} color={colors[id]} thumbnailPhoto={pet.thumbnail_photo} />
            </div>
            <span className="pet-name">{pet.name}</span>
            <span className="meta">
              {pet.hopper ? `Hopper ${pet.hopper}` : "No hopper"} · {formatNumber(pet.daily_allotment)} g/day
            </span>
          </Link>
        ))}
      </div>
    </div>
  );
}
