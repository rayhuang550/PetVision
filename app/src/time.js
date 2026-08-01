function safeTz(tz) {
  try {
    new Intl.DateTimeFormat("en-US", { timeZone: tz });
    return tz;
  } catch {
    return "UTC";
  }
}

function tzOffsetMs(tz, date) {
  const parts = new Intl.DateTimeFormat("en-US", {
    timeZone: tz,
    hour12: false,
    year: "numeric",
    month: "2-digit",
    day: "2-digit",
    hour: "2-digit",
    minute: "2-digit",
    second: "2-digit",
  }).formatToParts(date).reduce((values, part) => ((values[part.type] = part.value), values), {});
  const asUtc = Date.UTC(
    parts.year,
    parts.month - 1,
    parts.day,
    parts.hour === "24" ? 0 : parts.hour,
    parts.minute,
    parts.second,
  );
  return asUtc - date.getTime();
}

const localYmd = (tz, date) => new Intl.DateTimeFormat("en-CA", {
  timeZone: tz,
  year: "numeric",
  month: "2-digit",
  day: "2-digit",
}).format(date);

export function localMidnightMs(tz, now = new Date()) {
  tz = safeTz(tz);
  const ymd = localYmd(tz, now);
  const [year, month, day] = ymd.split("-").map(Number);
  const utcGuess = Date.UTC(year, month - 1, day, 0, 0, 0);
  const first = utcGuess - tzOffsetMs(tz, new Date(utcGuess));
  const second = utcGuess - tzOffsetMs(tz, new Date(first));
  if (second === first) return first;
  return localYmd(tz, new Date(second)) === ymd ? second : first;
}

export function nowMinutes(tz, now = new Date()) {
  const parts = new Intl.DateTimeFormat("en-US", {
    timeZone: safeTz(tz),
    hour12: false,
    hour: "2-digit",
    minute: "2-digit",
  }).formatToParts(now).reduce((values, part) => ((values[part.type] = part.value), values), {});
  return (Number(parts.hour) % 24) * 60 + Number(parts.minute);
}

const toMin = (hhmm) => {
  const [hour, minute] = hhmm.split(":").map(Number);
  return hour * 60 + minute;
};

export const fmtMin = (minutes) => {
  const hour = Math.floor(minutes / 60) % 24;
  const minute = minutes % 60;
  const hour12 = hour % 12 === 0 ? 12 : hour % 12;
  return `${hour12}:${String(minute).padStart(2, "0")} ${hour < 12 ? "AM" : "PM"}`;
};

export function eligibility(schedule, tz, now = new Date()) {
  if (!schedule || schedule.length === 0) return { allowed: true, label: "Allowed now" };
  const minutes = nowMinutes(tz, now);
  for (const window of schedule) {
    if (minutes >= toMin(window.start) && minutes < toMin(window.end)) {
      return { allowed: true, label: "Allowed now" };
    }
  }
  const laterToday = schedule
    .map((window) => toMin(window.start))
    .filter((start) => start > minutes)
    .sort((a, b) => a - b);
  const next = laterToday.length
    ? laterToday[0]
    : Math.min(...schedule.map((window) => toMin(window.start)));
  return { allowed: false, label: `Next window ${fmtMin(next)}` };
}

export function isFeederOnline(status, staleMs = 60_000) {
  return !!status && status.online === true && Date.now() - (status.ts || 0) < staleMs;
}
