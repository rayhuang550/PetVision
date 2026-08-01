import { useState } from "react";
import { useAuth } from "../../auth.jsx";

function describeAuthError(err) {
  const code = err?.code || "unknown";
  if (code === "auth/network-request-failed") {
    return "Can't reach the sign-in server. Check your connection and try again.";
  }
  if (code === "auth/invalid-credential" || code === "auth/wrong-password" || code === "auth/user-not-found") {
    return "Sign-in failed. Check the email and password.";
  }
  return `Sign-in failed (${code}).`;
}

export default function Login() {
  const { login } = useAuth();
  const [email, setEmail] = useState("");
  const [password, setPassword] = useState("");
  const [error, setError] = useState(null);
  const [busy, setBusy] = useState(false);

  async function submit(e) {
    e.preventDefault();
    setBusy(true);
    setError(null);
    try {
      await login(email.trim(), password);
    } catch (err) {
      setError(describeAuthError(err));
      console.error(err);
    } finally {
      setBusy(false);
    }
  }

  return (
    <div className="login">
      <div className="login-card">
        <span className="brand">PetVision</span>
        <h1 className="hero">Know who's<br />eating.</h1>
        <form onSubmit={submit}>
          <label>Email
            <input type="email" value={email} onChange={(e) => setEmail(e.target.value)} autoComplete="username" />
          </label>
          <label>Password
            <input type="password" value={password} onChange={(e) => setPassword(e.target.value)} autoComplete="current-password" />
          </label>
          {error && <p className="error">{error}</p>}
          <button className="pill fill wide" disabled={busy}>{busy ? "Signing in…" : "Sign in"}</button>
        </form>
      </div>
    </div>
  );
}
