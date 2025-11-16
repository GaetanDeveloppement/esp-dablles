import { defineConfig } from "vite";
import { svelte } from "@sveltejs/vite-plugin-svelte";

export default defineConfig({
  plugins: [svelte()],
  server: {
    port: 5173,
    // Optional: proxy API calls during development
    proxy: {
      "/api": {
        target: "http://192.168.1.14",
        changeOrigin: true,
      },
    },
  },
});
