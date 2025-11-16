// ESP32 API base URL - change this to your ESP32's IP
const API_BASE = "http://192.168.1.14";

export const api = {
  // Get motor status
  async getStatus() {
    const response = await fetch(`${API_BASE}/api/status`);
    return await response.json();
  },

  // Rotate motor
  async rotate(direction, steps, speed) {
    const response = await fetch(`${API_BASE}/api/rotate`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ direction, steps, speed }),
    });
    return await response.json();
  },

  // Stop motor
  async stop() {
    const response = await fetch(`${API_BASE}/api/stop`, {
      method: "POST",
    });
    return await response.json();
  },

  // Reset position
  async reset() {
    const response = await fetch(`${API_BASE}/api/reset`, {
      method: "POST",
    });
    return await response.json();
  },
};
