const { contextBridge, ipcRenderer, shell } = require('electron');

const pkg = require('./package.json');

contextBridge.exposeInMainWorld('api', {
  saveFile: (data) => ipcRenderer.invoke('save-file', data),
  openExternal: (url) => ipcRenderer.invoke('open-external', url),
  onUpdateAvailable: (callback) => ipcRenderer.on('update-available', (_, data) => callback(data)),
  platform: process.platform,
  version: pkg.version,
});
