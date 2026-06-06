import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

export default defineConfig({
  plugins: [react()],
  server: {
    allowedHosts: ['app.scc316.us', 'localhost', '127.0.0.1'],
  },
});
