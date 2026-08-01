const PALETTE = ["#C98A3B", "#6F7D4E", "#A34E68"];

export function petColorMap(pets) {
  const map = {};
  if (pets) Object.keys(pets).forEach((id, i) => { map[id] = PALETTE[i % PALETTE.length]; });
  return map;
}
