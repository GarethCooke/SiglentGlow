(function () {
    'use strict';

    const powerBtn        = document.getElementById('power-btn');
    const powerLabel      = document.getElementById('power-label');
    const colorPicker     = document.getElementById('color-picker');
    const brightnessInput = document.getElementById('brightness');
    const brightnessValue = document.getElementById('brightness-value');
    const statusEl        = document.getElementById('status');
    const ledStrip        = document.getElementById('led-strip');
    const leds            = ledStrip.querySelectorAll('.led');

    // Replies to our own slider POSTs arrive late; ignore the server's brightness for
    // this long after local input so the thumb isn't pulled back mid-drag.
    const SLIDER_HOLD_MS = 600;

    let state           = { on: false, color: '#ffffff', brightness: 128 };
    let ws              = null;
    let colorDebounce   = null;
    let lastSliderInput = 0;

    const WS_URL = Siglent.buildWsUrl(location.protocol, location.host);

    function showBrightness(brightness) {
        brightnessValue.textContent = Siglent.brightnessPercent(brightness) + '%';
        ledStrip.style.setProperty('--level', brightness / 255);
    }

    function applyState(s) {
        state                    = s;
        powerBtn.classList.toggle('on', s.on);
        powerLabel.textContent   = s.on ? 'ON' : 'OFF';
        colorPicker.value        = s.color;
        colorPicker.disabled     = !s.on;
        brightnessInput.disabled = !s.on;
        ledStrip.style.setProperty('--color-glow', s.color);
        leds.forEach((led) => led.classList.toggle('lit', s.on));
        ledStrip.classList.toggle('disabled', !s.on);
        if (Date.now() - lastSliderInput > SLIDER_HOLD_MS) {
            brightnessInput.value = s.brightness;
            showBrightness(s.brightness);
        }
    }

    function setStatus(msg) {
        statusEl.textContent = msg;
    }

    async function fetchState() {
        try {
            const res = await fetch('/api/state');
            applyState(await res.json());
            setStatus('Connected');
        } catch (_) {
            setStatus('Could not reach server');
        }
    }

    async function apiPost(path, body) {
        try {
            const res = await fetch(path, {
                method:  'POST',
                headers: { 'Content-Type': 'application/json' },
                body:    JSON.stringify(body),
            });
            applyState(await res.json());
        } catch (_) {
            setStatus('Request failed');
        }
    }

    function connectWs() {
        ws = new WebSocket(WS_URL);
        ws.onopen    = () => setStatus('Connected');
        ws.onmessage = (e) => { try { applyState(JSON.parse(e.data)); } catch (_) {} };
        ws.onclose   = () => { setStatus('Disconnected — retrying...'); setTimeout(connectWs, 3000); };
        ws.onerror   = () => ws.close();
    }

    powerBtn.addEventListener('click', () => apiPost('/api/power', { on: !state.on }));

    colorPicker.addEventListener('input', (e) => {
        clearTimeout(colorDebounce);
        colorDebounce = setTimeout(() => apiPost('/api/color', { color: e.target.value }), 200);
    });

    // At most one POST per 200 ms while dragging; the last position is always sent
    const sendBrightness = Siglent.throttle(
        (brightness) => apiPost('/api/brightness', { brightness: brightness }), 200);

    brightnessInput.addEventListener('input', () => {
        const brightness = Number(brightnessInput.value);
        lastSliderInput  = Date.now();
        showBrightness(brightness);
        sendBrightness(brightness);
    });

    fetchState();
    connectWs();
}());
