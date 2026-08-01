# Cloud Backend

Firebase Realtime Database, Authentication, Storage, security rules, and Hosting for PetVision.

## Set up and deploy

Cloud Storage for Firebase requires the Blaze plan.

```bash
cd ..
npm install
cp .firebaserc.example .firebaserc
npx firebase login
```

Create a Firebase project and register a web app. Enable Email/Password sign-in and Realtime
Database, then create the default Storage bucket. Use the
`your-project-id.firebasestorage.app` bucket name shown in the Firebase console. Create one owner
account and one device account:

```bash
cd cloud
GOOGLE_APPLICATION_CREDENTIALS=/path/to/serviceAccount.json \
PETVISION_PROJECT_ID=your-project \
PETVISION_OWNER_EMAIL=owner@example.com PETVISION_DEVICE_EMAIL=feeder@example.com \
PETVISION_OWNER_PASSWORD=... PETVISION_DEVICE_PASSWORD=... \
npm run setup
cd ..
npm run deploy
```

Copy the database URL and storage bucket name from the Firebase console into the app and firmware
settings.
