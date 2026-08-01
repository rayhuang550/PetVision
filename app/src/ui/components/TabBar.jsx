import { NavLink } from "react-router-dom";
import { BowlIcon, PawIcon, LedgerIcon, GearIcon } from "../../components/TabIcons.jsx";

const tabs = [
  { to: "/", label: "Home", Icon: BowlIcon, end: true },
  { to: "/pets", label: "Pets", Icon: PawIcon },
  { to: "/history", label: "History", Icon: LedgerIcon },
];

export default function TabBar() {
  return (
    <nav className="navwrap" aria-label="Main">
      <div className="navpill">
        {tabs.map(({ to, label, Icon, end }) => (
          <NavLink key={to} to={to} end={end} aria-label={label} title={label}>
            <Icon />
          </NavLink>
        ))}
      </div>
      <NavLink to="/settings" className="navcircle" aria-label="Settings" title="Settings">
        <GearIcon />
      </NavLink>
    </nav>
  );
}
