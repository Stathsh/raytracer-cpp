const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('api', {
  saveFile: (data) => ipcRenderer.invoke('save-file', data),
  platform: process.platform,
});
