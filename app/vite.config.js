import { defineConfig, loadEnv } from "vite";
import react from "@vitejs/plugin-react";

const REQUIRED = [
  "VITE_PROJECT_ID",
  "VITE_APP_ID",
  "VITE_API_KEY",
  "VITE_AUTH_DOMAIN",
  "VITE_DATABASE_URL",
  "VITE_STORAGE_BUCKET",
];

function requireFirebaseConfig(mode) {
  return {
    name: "require-firebase-config",
    buildStart() {
      const env = loadEnv(mode, process.cwd(), "VITE_");
      const missing = REQUIRED.filter((key) => !env[key]);
      if (missing.length) {
        this.error(
          `Missing Firebase configuration: ${missing.join(", ")}. ` +
            "Copy app/.env.example to app/.env and fill it in.",
        );
      }
    },
  };
}

export default defineConfig(({ mode }) => ({
  plugins: [react(), requireFirebaseConfig(mode)],
}));
