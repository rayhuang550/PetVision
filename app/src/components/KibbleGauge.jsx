import { useEffect, useRef } from "react";
import { formatNumber, formatProgressNumber } from "../numberFormat.js";

const DOTS = 10;

export default function KibbleGauge({ dispensed, allotment, color }) {
  let filled = 0;
  if (allotment > 0) {
    filled = Math.min(DOTS, Math.round((dispensed / allotment) * DOTS));
    if (dispensed > 0 && filled === 0) filled = 1;
  }

  const prevFilled = useRef(null);
  useEffect(() => { prevFilled.current = filled; }, [filled]);
  const animateFrom = prevFilled.current === null ? filled : prevFilled.current;

  return (
    <div
      className="dots"
      role="img"
      aria-label={`${formatProgressNumber(dispensed)} of ${formatNumber(allotment)} grams dispensed today`}
    >
      {Array.from({ length: DOTS }, (_, i) => (
        <i
          key={i}
          className={i < filled ? (i >= animateFrom ? "dot live" : "dot") : "dot empty"}
          style={i < filled ? { background: color } : undefined}
        />
      ))}
    </div>
  );
}
