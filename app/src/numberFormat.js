const oneDecimalFormatter = new Intl.NumberFormat(undefined, {
  minimumFractionDigits: 0,
  maximumFractionDigits: 1,
});

export function formatNumber(value, fallback = "N/A") {
  const number = finiteNumber(value);
  if (number === null) return fallback;

  return oneDecimalFormatter.format(normalizeZero(number));
}

export function formatProgressNumber(value, fallback = "N/A") {
  const number = finiteNumber(value);
  if (number === null) return fallback;

  const rounded = Math.round((normalizeZero(number) + Number.EPSILON) * 10) / 10;
  return rounded === 0 ? "0" : rounded.toFixed(1);
}

function finiteNumber(value) {
  if (value === null || value === undefined || value === "") return null;
  const number = Number(value);
  return Number.isFinite(number) ? number : null;
}

function normalizeZero(number) {
  return Object.is(number, -0) ? 0 : number;
}
