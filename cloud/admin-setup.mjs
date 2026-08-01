import { applicationDefault, deleteApp, initializeApp } from "firebase-admin/app";
import { getAuth } from "firebase-admin/auth";

const PROJECT_ID = process.env.PETVISION_PROJECT_ID;
if (!PROJECT_ID) {
  console.error("Set PETVISION_PROJECT_ID (and GOOGLE_APPLICATION_CREDENTIALS to a service-account key).");
  process.exit(1);
}

const OWNER_EMAIL = process.env.PETVISION_OWNER_EMAIL || "owner@example.com";
const DEVICE_EMAIL = process.env.PETVISION_DEVICE_EMAIL || "feeder@example.com";
const OWNER_PASSWORD = process.env.PETVISION_OWNER_PASSWORD;
const DEVICE_PASSWORD = process.env.PETVISION_DEVICE_PASSWORD;
if (!OWNER_PASSWORD || !DEVICE_PASSWORD) {
  console.error("Set PETVISION_OWNER_PASSWORD and PETVISION_DEVICE_PASSWORD. These are real credentials; choose strong ones.");
  process.exit(1);
}

const app = initializeApp({ credential: applicationDefault(), projectId: PROJECT_ID });
const auth = getAuth(app);

async function ensureUser(email, password, claims) {
  let user;
  try {
    user = await auth.createUser({ email, password, emailVerified: true });
  } catch (e) {
    if (e.code !== "auth/email-already-exists") throw e;
    user = await auth.getUserByEmail(email);
    await auth.updateUser(user.uid, { password });
  }
  await auth.setCustomUserClaims(user.uid, claims);
  return user.uid;
}

async function main() {
  await ensureUser(OWNER_EMAIL, OWNER_PASSWORD, { owner: true });
  await ensureUser(DEVICE_EMAIL, DEVICE_PASSWORD, { device: true });
  console.log(`Setup complete for ${PROJECT_ID}: ${OWNER_EMAIL} (owner) + ${DEVICE_EMAIL} (device).`);
  await deleteApp(app);
}

main().catch((e) => {
  console.error(e);
  process.exit(1);
});
