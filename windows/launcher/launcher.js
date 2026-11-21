/* Grand Theft Auto V — Five Star Port Windows Launcher JS */

(function () {
  'use strict';

  // State Management
  const state = {
    currentScreen: 'play',
    currentMode: 'story',
    sandboxMap: 0,
    activeSettingsTab: 'graphics',
    activePreset: 4, // Quality (recommended)
    filesReady: false,
    gameFolderPath: 'C:\\Games\\Grand Theft Auto V',
    problems: [],
    settings: {
      resolution: 0,      // 0: Native (1080p), 1: 720p, 2: 900p, 3: 1440p, 4: 4K
      fullscreen: 0,      // 0: On, 1: Off
      framerate: 0,       // 0: Unlocked, 1: 30 FPS, 2: 60 FPS, 3: 120 FPS
      showfps: 0,         // 0: Off, 1: On
      vsync: 0,           // 0: On, 1: Off
      memorycap: 0,       // 0: Automatic, 1: 4 GB, 2: 6 GB, 3: 8 GB
      textures: 0,        // 0: Preset (High), 1: Normal, 2: Very High
      shaders: 0,         // 0: Preset (High), 1: Normal, 2: Very High
      shadows: 0,         // 0: Preset (Normal), 1: Low, 2: High
      reflections: 0,     // 0: Preset (High), 1: Normal, 2: Very High
      drawdistance: 0,    // 0: Preset (70%), 1: 40%, 2: 100%
      grass: 0,           // 0: Preset (High), 1: Normal, 2: Very High
      language: 0,        // 0: English (US), 1: Français, 2: Deutsch, 3: Español, 4: Italiano, 5: Русский
      mods_enabled: 0,    // 0: On, 1: Off
      selfradio_mode: 0   // 0: Radio Station, 1: Sequential, 2: Shuffle
    }
  };

  const settingOptions = {
    resolution: ['Native (1080p)', '720p (1280x720)', '900p (1600x900)', '1440p (2560x1440)', '4K (3840x2160)'],
    fullscreen: ['On', 'Off'],
    framerate: ['Unlocked', '30 FPS', '60 FPS', '120 FPS'],
    showfps: ['Off', 'On'],
    vsync: ['On', 'Off'],
    memorycap: ['Automatic', '4 GB', '6 GB', '8 GB'],
    textures: ['Preset (High)', 'Normal', 'Very High'],
    shaders: ['Preset (High)', 'Normal', 'Very High'],
    shadows: ['Preset (Normal)', 'Low', 'High'],
    reflections: ['Preset (High)', 'Normal', 'Very High'],
    drawdistance: ['Preset (70%)', '40%', '100%'],
    grass: ['Preset (High)', 'Normal', 'Very High'],
    language: ['English (US)', 'Français', 'Deutsch', 'Español', 'Italiano', 'Русский'],
    mods_enabled: ['On', 'Off'],
    selfradio_mode: ['Radio Station', 'Sequential', 'Shuffle']
  };

  const modeData = {
    story: {
      title: 'STORY MODE',
      desc: 'The campaign with missions. Continues your latest save, or starts from the Prologue.',
      hasMap: false
    },
    sandbox: {
      title: 'SANDBOX',
      desc: 'Free roam in Los Santos with cheats, vehicle spawning, and full map exploration.',
      hasMap: true
    },
    performance: {
      title: 'PERFORMANCE',
      desc: 'Optimized high-framerate mode with balanced rendering for smooth 60+ FPS gameplay.',
      hasMap: false
    },
    mods: {
      title: 'MODS & PATCHES',
      desc: 'Load custom ASI scripts, vehicle models, and community modifications.',
      hasMap: false
    },
    radio: {
      title: 'SELF RADIO',
      desc: 'Your own custom MP3 and audio tracks broadcast on the in-game Self Radio station.',
      hasMap: false
    }
  };

  // Bridge Communication
  function sendNativeMessage(type, payload) {
    const msg = JSON.stringify({ type: type, payload: payload || {} });
    if (window.chrome && window.chrome.webview && window.chrome.webview.postMessage) {
      window.chrome.webview.postMessage(msg);
    } else {
      console.log('[NativeBridge mock send]', type, payload);
    }
  }

  // Receive message from C++
  if (window.chrome && window.chrome.webview && window.chrome.webview.addEventListener) {
    window.chrome.webview.addEventListener('message', function (event) {
      try {
        const data = typeof event.data === 'string' ? JSON.parse(event.data) : event.data;
        handleNativeMessage(data.type, data.payload);
      } catch (e) {
        console.error('Failed to parse message from native:', e);
      }
    });
  }

  function handleNativeMessage(type, payload) {
    console.log('[NativeBridge received]', type, payload);
    switch (type) {
      case 'file_status':
        updateFileStatus(payload);
        break;
      case 'sys_info':
        updateSystemInfo(payload);
        break;
      case 'settings_loaded':
        if (payload) {
          Object.assign(state.settings, payload);
          renderAllSettings();
        }
        break;
    }
  }

  // UI Navigation
  function switchScreen(screenName) {
    state.currentScreen = screenName;
    document.querySelectorAll('.screen').forEach(el => el.classList.remove('active'));
    document.querySelectorAll('.nav-tab').forEach(el => el.classList.remove('active'));

    const bgStart = document.getElementById('bg-start');
    const bgSettings = document.getElementById('bg-settings');

    if (screenName === 'play') {
      document.getElementById('screen-play').classList.add('active');
      document.getElementById('nav-play').classList.add('active');
      bgStart.classList.add('active');
      bgSettings.classList.remove('active');
    } else if (screenName === 'settings') {
      document.getElementById('screen-settings').classList.add('active');
      document.getElementById('nav-settings').classList.add('active');
      bgStart.classList.remove('active');
      bgSettings.classList.add('active');
    } else if (screenName === 'gamefiles') {
      document.getElementById('screen-gamefiles').classList.add('active');
      bgStart.classList.remove('active');
      bgSettings.classList.add('active');
    } else if (screenName === 'online') {
      // Jump to Settings -> Online tab
      switchScreen('settings');
      switchSettingsTab('online');
    }
  }

  function switchSettingsTab(tabName) {
    state.activeSettingsTab = tabName;
    document.querySelectorAll('.sidebar-item').forEach(item => {
      item.classList.toggle('active', item.dataset.tab === tabName);
    });
    document.querySelectorAll('.settings-tab-pane').forEach(pane => {
      pane.classList.toggle('active', pane.id === 'tab-' + tabName);
    });
  }

  // Mode Selection
  function selectMode(modeKey) {
    state.currentMode = modeKey;
    document.querySelectorAll('.mode-card').forEach(card => {
      card.classList.toggle('active', card.dataset.mode === modeKey);
    });

    const info = modeData[modeKey] || modeData.story;
    document.getElementById('hero-title').textContent = info.title;
    document.getElementById('hero-desc').textContent = info.desc;

    const mapRow = document.getElementById('sandbox-map-row');
    mapRow.classList.toggle('hidden', !info.hasMap);
  }

  // Settings Controls
  function cycleSetting(key, direction) {
    const opts = settingOptions[key];
    if (!opts) return;
    let idx = state.settings[key] || 0;
    idx = (idx + direction + opts.length) % opts.length;
    state.settings[key] = idx;
    renderSetting(key);
    sendNativeMessage('save_settings', state.settings);
  }

  function renderSetting(key) {
    const row = document.querySelector(`.setting-row[data-setting="${key}"]`);
    if (!row) return;
    const valEl = row.querySelector('.carousel-val');
    if (valEl && settingOptions[key]) {
      valEl.textContent = settingOptions[key][state.settings[key] || 0];
    }
  }

  function renderAllSettings() {
    for (const key in settingOptions) {
      renderSetting(key);
    }
  }

  // Presets
  function selectPreset(presetIndex) {
    state.activePreset = presetIndex;
    document.querySelectorAll('.preset-row').forEach(row => {
      const idx = parseInt(row.dataset.preset, 10);
      const isActive = idx === presetIndex;
      row.classList.toggle('active', isActive);
      row.querySelector('.check-mark').textContent = isActive ? '✓' : '';
    });
    sendNativeMessage('preset_changed', { preset: presetIndex });
  }

  // File Status & Diagnostics
  function updateFileStatus(status) {
    // status: { ready: bool, folder: string, missingCount: int, missingBytes: int, corruptCount: int, problems: [] }
    state.filesReady = status.ready;
    state.gameFolderPath = status.folder || state.gameFolderPath;
    state.problems = status.problems || [];

    const playBtn = document.getElementById('btn-play');
    const banner = document.getElementById('file-status-banner');
    const folderDisplay = document.getElementById('settings-folder-display');
    const folderText = document.getElementById('game-folder-text');
    const diagBox = document.getElementById('diag-box');

    folderDisplay.textContent = state.gameFolderPath;
    folderText.textContent = state.gameFolderPath;

    if (status.ready) {
      playBtn.classList.remove('disabled');
      banner.className = 'file-status-banner ready';
      banner.querySelector('.banner-icon').textContent = '✓';
      document.getElementById('banner-headline').textContent = 'Game files ready';
      document.getElementById('banner-detail').textContent = 'All 21 GB assets verified. Click PLAY to launch.';

      diagBox.className = 'diag-alert-box ready';
      diagBox.querySelector('.diag-icon').textContent = '✓';
      document.getElementById('diag-title').textContent = 'ALL GAME FILES VERIFIED';
      document.getElementById('diag-desc').textContent = 'Your GTA V installation is complete and ready to run.';
      document.getElementById('diag-missing-stat').textContent = '0 files missing';
      document.getElementById('diag-corrupt-stat').textContent = '0 files corrupted';
      document.getElementById('btn-show-problems').style.display = 'none';
    } else {
      playBtn.classList.remove('disabled'); // Allow click so it explains or launches
      banner.className = 'file-status-banner warning';
      banner.querySelector('.banner-icon').textContent = '⚠';
      document.getElementById('banner-headline').textContent = status.missingCount > 0 ? 'Some game files are missing or damaged' : 'Game folder requires verification';
      document.getElementById('banner-detail').textContent = "Found your game folder, but it can't be used yet. Click to see why and how to fix it.";

      diagBox.className = 'diag-alert-box warning';
      diagBox.querySelector('.diag-icon').textContent = '⚠';
      document.getElementById('diag-title').textContent = 'SOME GAME FILES ARE DAMAGED';
      document.getElementById('diag-desc').textContent = 'Missing or mismatched archives were detected in your game folder.';
      document.getElementById('diag-missing-stat').textContent = (status.missingCount || 0) + ' files missing';
      document.getElementById('diag-corrupt-stat').textContent = (status.corruptCount || 0) + ' files corrupted';
      document.getElementById('btn-show-problems').style.display = 'block';
    }
  }

  function updateSystemInfo(info) {
    if (info.gpu) document.getElementById('dev-gpu').textContent = info.gpu;
    if (info.cpu) document.getElementById('dev-cpu').textContent = info.cpu;
    if (info.ram) document.getElementById('dev-ram').textContent = info.ram;
  }

  function showProblemsModal() {
    const modal = document.getElementById('modal-problems');
    const list = document.getElementById('problem-items-list');
    list.innerHTML = '';

    if (!state.problems || state.problems.length === 0) {
      const li = document.createElement('li');
      li.className = 'problem-item';
      li.innerHTML = '<div class="item-path">common/RadioLog.csv</div><div class="item-desc">Missing archive or payload file (expected 1.4 MB)</div>';
      list.appendChild(li);

      const li2 = document.createElement('li');
      li2.className = 'problem-item';
      li2.innerHTML = '<div class="item-path">common/data/CloudKeyframes.xml</div><div class="item-desc">File size mismatch: 0 KB (expected 48.2 KB)</div>';
      list.appendChild(li2);
    } else {
      state.problems.forEach(p => {
        const li = document.createElement('li');
        li.className = 'problem-item';
        li.innerHTML = `<div class="item-path">${p.path}</div><div class="item-desc">${p.reason || 'Missing or corrupted file'}</div>`;
        list.appendChild(li);
      });
    }

    modal.classList.remove('hidden');
  }

  function hideProblemsModal() {
    document.getElementById('modal-problems').classList.add('hidden');
  }

  // Keyboard Navigation
  window.addEventListener('keydown', function (e) {
    if (e.key === 'Escape') {
      if (!document.getElementById('modal-problems').classList.contains('hidden')) {
        hideProblemsModal();
      } else if (state.currentScreen !== 'play') {
        switchScreen('play');
      }
    } else if (e.key === 'Enter' && state.currentScreen === 'play') {
      onPlayClicked();
    } else if (state.currentScreen === 'play') {
      const modeKeys = ['story', 'sandbox', 'performance', 'mods', 'radio'];
      const curIdx = modeKeys.indexOf(state.currentMode);
      if (e.key === 'ArrowRight') {
        const nextIdx = (curIdx + 1) % modeKeys.length;
        selectMode(modeKeys[nextIdx]);
      } else if (e.key === 'ArrowLeft') {
        const prevIdx = (curIdx - 1 + modeKeys.length) % modeKeys.length;
        selectMode(modeKeys[prevIdx]);
      }
    }
  });

  function onPlayClicked() {
    sendNativeMessage('play', {
      mode: state.currentMode,
      sandboxMap: state.sandboxMap,
      preset: state.activePreset,
      settings: state.settings
    });
  }

  // DOM Event Listeners Setup
  document.addEventListener('DOMContentLoaded', function () {
    // Top Tabs
    document.getElementById('nav-play').addEventListener('click', () => switchScreen('play'));
    document.getElementById('nav-settings').addEventListener('click', () => switchScreen('settings'));
    document.getElementById('nav-online').addEventListener('click', () => switchScreen('online'));

    // External links
    document.querySelectorAll('.external-link').forEach(el => {
      el.addEventListener('click', (e) => {
        const url = el.dataset.url;
        if (url) {
          sendNativeMessage('open_url', { url: url });
        }
      });
    });

    // Play CTA & Banner
    document.getElementById('btn-play').addEventListener('click', onPlayClicked);
    document.getElementById('file-status-banner').addEventListener('click', () => switchScreen('gamefiles'));

    // Mode Cards
    document.querySelectorAll('.mode-card').forEach(card => {
      card.addEventListener('click', () => selectMode(card.dataset.mode));
    });

    // Sandbox Map Chips
    document.querySelectorAll('.map-chip').forEach(chip => {
      chip.addEventListener('click', () => {
        document.querySelectorAll('.map-chip').forEach(c => c.classList.remove('active'));
        chip.classList.add('active');
        state.sandboxMap = parseInt(chip.dataset.map, 10);
      });
    });

    // Settings Sidebar
    document.querySelectorAll('.sidebar-item').forEach(item => {
      item.addEventListener('click', () => switchSettingsTab(item.dataset.tab));
    });

    // Presets
    document.querySelectorAll('.preset-row').forEach(row => {
      row.addEventListener('click', () => selectPreset(parseInt(row.dataset.preset, 10)));
    });

    // Carousels
    document.querySelectorAll('.setting-row').forEach(row => {
      const key = row.dataset.setting;
      if (!key) return;
      const prevBtn = row.querySelector('.carousel-arrow.prev');
      const nextBtn = row.querySelector('.carousel-arrow.next');
      if (prevBtn) prevBtn.addEventListener('click', () => cycleSetting(key, -1));
      if (nextBtn) nextBtn.addEventListener('click', () => cycleSetting(key, 1));
      const valEl = row.querySelector('.carousel-val');
      if (valEl) valEl.addEventListener('click', () => cycleSetting(key, 1));
    });

    // Settings Back
    document.getElementById('btn-settings-back').addEventListener('click', () => switchScreen('play'));
    document.getElementById('btn-files-back').addEventListener('click', () => switchScreen('play'));

    // Game files shortcuts from Settings
    document.getElementById('btn-settings-browse').addEventListener('click', () => sendNativeMessage('browse_folder'));
    document.getElementById('btn-settings-verify').addEventListener('click', () => sendNativeMessage('verify_files'));
    document.getElementById('btn-settings-open-mgr').addEventListener('click', () => switchScreen('gamefiles'));

    // Special folder openers
    document.getElementById('btn-open-saves').addEventListener('click', () => sendNativeMessage('open_folder', { folder: 'saves' }));
    document.getElementById('btn-open-mods').addEventListener('click', () => sendNativeMessage('open_folder', { folder: 'mods' }));
    document.getElementById('btn-open-music').addEventListener('click', () => sendNativeMessage('open_folder', { folder: 'music' }));

    // Game Files Screen
    document.getElementById('btn-browse-folder').addEventListener('click', () => sendNativeMessage('browse_folder'));
    document.getElementById('btn-files-recheck').addEventListener('click', () => sendNativeMessage('verify_files'));
    document.getElementById('btn-show-problems').addEventListener('click', showProblemsModal);
    document.getElementById('btn-close-problem-modal').addEventListener('click', hideProblemsModal);
    document.getElementById('btn-dismiss-problems').addEventListener('click', hideProblemsModal);

    // Initial render
    renderAllSettings();
    selectMode('story');

    // Notify native host that UI is ready
    sendNativeMessage('ready');
  });

})();
