# Companion App

An installable React app for managing pets, requesting feeds, and viewing intake history.

## Deploy

From the repository root:

```bash
npm install
cp app/.env.example app/.env
npm run deploy
```

Before deploying, copy these values from your web app's Firebase settings into `app/.env`:
`VITE_PROJECT_ID`, `VITE_APP_ID`, `VITE_API_KEY`, `VITE_AUTH_DOMAIN`, `VITE_DATABASE_URL`, and
`VITE_STORAGE_BUCKET`. The deploy command publishes the files in `app/dist/` to Firebase Hosting.

## Develop locally

```bash
npm run dev
```
