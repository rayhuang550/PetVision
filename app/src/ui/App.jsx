import { useEffect } from "react";
import { Routes, Route, Navigate } from "react-router-dom";
import { useAuth } from "../auth.jsx";
import { api } from "../api.js";
import Login from "./components/Login.jsx";
import TabBar from "./components/TabBar.jsx";
import Home from "./screens/Home.jsx";
import Pets from "./screens/Pets.jsx";
import PetDetail from "./screens/PetDetail.jsx";
import History from "./screens/History.jsx";
import Settings from "./screens/Settings.jsx";

export default function App() {
  const { user } = useAuth();

  useEffect(() => {
    if (user) api.clearTerminalCommands().catch(console.error);
  }, [user]);

  if (user === undefined) return <div className="splash">Loading…</div>;
  if (user === null) return <Login />;

  return (
    <div className="app">
      <main className="screen">
        <Routes>
          <Route path="/" element={<Home />} />
          <Route path="/pets" element={<Pets />} />
          <Route path="/pets/:id" element={<PetDetail />} />
          <Route path="/history" element={<History />} />
          <Route path="/settings" element={<Settings />} />
          <Route path="*" element={<Navigate to="/" replace />} />
        </Routes>
      </main>
      <TabBar />
    </div>
  );
}
