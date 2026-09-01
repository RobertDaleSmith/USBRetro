/**
 * WiFi / PS Remote Play Output Page (usb2wifi app).
 *
 * Provisions the Remote Play output: WiFi SSID/password (station mode), the PS5
 * IP, and the credentials from `remote-play-lab/rp.py` (PSN Account ID + RP-Key
 * + Registration Key). Talks to the firmware's remoteplay OutputInterface via
 * the generic OUTPUT.NATIVE.GET/SET (type == "remoteplay").
 *
 * Account ID / RP-Key / Regist Key are entered as hex; rp.py's profile stores
 * the account id as base64 and the keys as hex — paste them from
 * ~/.pyremoteplay/.profile.json (RP-Key, RegistKey) and the account id from
 * `rp.py list` (id=...), base64-decoded to 8 bytes -> 16 hex chars.
 */
export class WifiOutputCard {
    constructor(el, protocol, log) {
        this.el = el;
        this.protocol = protocol;
        this.log = log;
        this.available = false;
    }

    isAvailable() { return this.available; }
    async load() { return this.refresh(); }

    render() {
        this.el.innerHTML = `
            <div class="card">
                <h2>PS Remote Play (WiFi)</h2>
                <div class="card-content">
                    <p class="hint">
                        Drive a PS5 over WiFi via PS Remote Play. Pair once on a PC with
                        <code>remote-play-lab/rp.py</code>, then paste the resulting WiFi + PSN
                        credentials here. The adapter connects to your network and, once the
                        session engine is enabled, forwards your controller to the console.
                    </p>

                    <div class="device-info">
                        <div class="row"><span class="label">WiFi</span><span class="value" id="rpWifiState">—</span></div>
                        <div class="row"><span class="label">Adapter IP</span><span class="value" id="rpIp">—</span></div>
                        <div class="row"><span class="label">Session</span><span class="value" id="rpSession">—</span></div>
                    </div>

                    <h3>WiFi network</h3>
                    <div class="button-row">
                        <button id="rpWifiScanBtn" title="Scan for networks">Scan for networks</button>
                    </div>
                    <div id="rpAps" class="rp-ap-list"></div>
                    <div class="form-row"><label for="rpSsid">SSID</label><input type="text" id="rpSsid" maxlength="32"></div>
                    <div class="form-row"><label for="rpPass">Password</label><input type="password" id="rpPass" maxlength="63"></div>
                    <div class="button-row">
                        <button id="rpWifiConnectBtn">Connect</button>
                        <span id="rpWifiMsg" class="hint"></span>
                    </div>

                    <style>
                        .rp-ap-list { display:flex; flex-direction:column; gap:2px; margin:6px 0; }
                        .rp-ap-row { display:flex; justify-content:space-between; align-items:center;
                            padding:6px 10px; border:1px solid var(--border,#333); border-radius:6px;
                            cursor:pointer; }
                        .rp-ap-row:hover { background:var(--hover,rgba(128,128,128,0.12)); }
                        .rp-ap-row.selected { border-color:var(--accent,#4a9eff); background:rgba(74,158,255,0.12); }
                        .rp-ap-row .rp-ap-sig { opacity:0.7; font-variant-numeric:tabular-nums; }
                    </style>

                    <h3>PlayStation 5</h3>
                    <div class="form-row">
                        <label for="rpPs5Ip">PS5 IP</label>
                        <input type="text" id="rpPs5Ip" placeholder="192.168.1.107">
                        <button id="rpScanBtn" title="Find consoles on your network">Scan</button>
                    </div>
                    <div id="rpHosts" class="rp-ap-list"></div>
                    <div class="form-row"><label for="rpAccount">Account ID (16 hex)</label><input type="text" id="rpAccount" placeholder="d83c2a2b2d0c3809" maxlength="16"></div>
                    <div class="form-row"><label for="rpKey">RP-Key (32 hex)</label><input type="text" id="rpKey" maxlength="32"></div>
                    <div class="form-row"><label for="rpRegist">Regist Key (32 hex)</label><input type="text" id="rpRegist" maxlength="32"></div>

                    <div class="button-row">
                        <button id="rpSaveBtn">Save PS5 credentials</button>
                    </div>
                    <div id="rpStatusMsg" class="hint"></div>
                </div>
            </div>`;
        this.el.querySelector('#rpSaveBtn').addEventListener('click', () => this.save());
        this.el.querySelector('#rpScanBtn').addEventListener('click', () => this.scan());
        this.el.querySelector('#rpWifiScanBtn').addEventListener('click', () => this.wifiScan());
        this.el.querySelector('#rpWifiConnectBtn').addEventListener('click', () => this.connectWifi());
    }

    async connectWifi() {
        const ssid = this.el.querySelector('#rpSsid').value.trim();
        const pass = this.el.querySelector('#rpPass').value;
        if (!ssid) { this.#wifiMsg('Pick a network or type an SSID', 'error'); return; }
        const btn = this.el.querySelector('#rpWifiConnectBtn');
        btn.disabled = true; btn.textContent = 'Connecting…';
        try {
            await this.protocol.sendCommand('OUTPUT.NATIVE.SET', { wifi_ssid: ssid, wifi_pass: pass });
            this.#wifiMsg(`Connecting to "${ssid}"…`, 'success');
            // Poll the WiFi state until it settles.
            for (let i = 0; i < 12; i++) {
                await new Promise(r => setTimeout(r, 1000));
                await this.refresh();
                const st = this.el.querySelector('#rpWifiState')?.textContent || '';
                if (st.startsWith('connected')) { this.#wifiMsg('Connected ✓', 'success'); break; }
                if (st.startsWith('failed'))    { this.#wifiMsg('Connection failed — check password', 'error'); break; }
            }
        } catch (e) {
            this.#wifiMsg(`Connect failed: ${e.message}`, 'error');
        }
        btn.disabled = false; btn.textContent = 'Connect';
    }

    #wifiMsg(text, kind) {
        const e = this.el.querySelector('#rpWifiMsg');
        if (e) { e.textContent = text; e.className = 'hint ' + (kind === 'error' ? 'error' : 'success'); }
    }

    async wifiScan() {
        const btn = this.el.querySelector('#rpWifiScanBtn');
        btn.disabled = true; btn.textContent = 'Scanning…';
        try {
            await this.protocol.sendCommand('OUTPUT.NATIVE.SET', { wifiscan: 1 });
            for (let i = 0; i < 5; i++) {
                await new Promise(r => setTimeout(r, 700));
                await this.refresh();
            }
        } catch (e) {
            this.#msg(`WiFi scan failed: ${e.message}`, 'error');
        }
        btn.disabled = false; btn.textContent = 'Scan';
    }

    #signalBars(rssi) {
        // rssi ~ -30 (strong) .. -90 (weak)
        const n = rssi >= -55 ? 4 : rssi >= -65 ? 3 : rssi >= -75 ? 2 : 1;
        return '▂▄▆█'.slice(0, n) + '·'.repeat(4 - n);
    }

    renderAps(aps) {
        const box = this.el.querySelector('#rpAps');
        if (!box) return;
        if (!aps || !aps.length) { box.innerHTML = ''; return; }
        const cur = this.el.querySelector('#rpSsid')?.value || '';
        box.innerHTML = aps.map(a => {
            const s = (a.ssid || '').replace(/"/g, '&quot;');
            const sel = a.ssid === cur ? ' selected' : '';
            return `<div class="rp-ap-row${sel}" data-ssid="${s}">
                <span class="rp-ap-name">${a.secure ? '🔒 ' : ''}${a.ssid || '(hidden)'}</span>
                <span class="rp-ap-sig">${this.#signalBars(a.rssi)}</span>
            </div>`;
        }).join('');
        box.querySelectorAll('.rp-ap-row').forEach(row => row.addEventListener('click', () => {
            const ssid = row.getAttribute('data-ssid');
            const inp = this.el.querySelector('#rpSsid');
            if (inp) inp.value = ssid;
            box.querySelectorAll('.rp-ap-row').forEach(r => r.classList.remove('selected'));
            row.classList.add('selected');
            this.el.querySelector('#rpPass')?.focus();
        }));
    }

    async scan() {
        const btn = this.el.querySelector('#rpScanBtn');
        btn.disabled = true; btn.textContent = 'Scanning…';
        try {
            await this.protocol.sendCommand('OUTPUT.NATIVE.SET', { scan: 1 });
            // Poll for results while the ~3s device-side scan runs.
            for (let i = 0; i < 5; i++) {
                await new Promise(r => setTimeout(r, 800));
                await this.refresh();
            }
        } catch (e) {
            this.#msg(`Scan failed: ${e.message}`, 'error');
        }
        btn.disabled = false; btn.textContent = 'Scan';
    }

    renderHosts(hosts) {
        const box = this.el.querySelector('#rpHosts');
        if (!box) return;
        if (!hosts || !hosts.length) { box.innerHTML = ''; return; }
        const cur = this.el.querySelector('#rpPs5Ip')?.value || '';
        box.innerHTML = hosts.map(h => {
            const sel = h.ip === cur ? ' selected' : '';
            return `<div class="rp-ap-row${sel}" data-ip="${h.ip}">
                <span class="rp-ap-name">${h.ps5 ? '🎮' : '🎮'} ${h.name} <span class="rp-ap-sig">${h.ps5 ? 'PS5' : 'PS4'} · ${h.ready ? 'ready' : 'standby'}</span></span>
                <span class="rp-ap-sig">${h.ip}</span>
            </div>`;
        }).join('');
        box.querySelectorAll('.rp-ap-row').forEach(row => row.addEventListener('click', () => {
            const ip = row.getAttribute('data-ip');
            const ipInput = this.el.querySelector('#rpPs5Ip');
            if (ipInput) ipInput.value = ip;
            box.querySelectorAll('.rp-ap-row').forEach(r => r.classList.remove('selected'));
            row.classList.add('selected');
        }));
    }

    async refresh() {
        try {
            const r = await this.protocol.sendCommand('OUTPUT.NATIVE.GET');
            if (!r || r.type !== 'remoteplay') { this.available = false; return; }
            this.available = true;
            if (!this.el.querySelector('#rpSaveBtn')) this.render();
            const set = (id, v) => { const e = this.el.querySelector(id); if (e) e.textContent = v; };
            set('#rpWifiState', `${r.wifi_state || '—'}${r.wifi_ssid ? ' (' + r.wifi_ssid + ')' : ''}`);
            set('#rpIp', r.ip || '—');
            set('#rpSession', r.session || '—');
            this.renderHosts(r.hosts);
            this.renderAps(r.aps);
            const ssid = this.el.querySelector('#rpSsid');
            if (ssid && !ssid.value && r.wifi_ssid) ssid.value = r.wifi_ssid;
            const ip = this.el.querySelector('#rpPs5Ip');
            if (ip && !ip.value && r.ps5_ip) ip.value = r.ps5_ip;
        } catch (e) {
            this.available = false;
        }
    }

    #hexOk(s, n) { return typeof s === 'string' && new RegExp(`^[0-9a-fA-F]{${n}}$`).test(s); }

    async save() {
        // WiFi is handled by the Connect button; Save stores the PS5 credentials.
        const payload = {};
        const ip = this.el.querySelector('#rpPs5Ip').value.trim();
        const acct = this.el.querySelector('#rpAccount').value.trim();
        const key = this.el.querySelector('#rpKey').value.trim();
        const regist = this.el.querySelector('#rpRegist').value.trim();

        if (ip) payload.ps5_ip = ip;
        if (acct) {
            if (!this.#hexOk(acct, 16)) return this.#msg('Account ID must be 16 hex chars', 'error');
            payload.account_id = acct;
        }
        if (key) {
            if (!this.#hexOk(key, 32)) return this.#msg('RP-Key must be 32 hex chars', 'error');
            payload.rp_key = key;
            if (regist) {
                if (!this.#hexOk(regist, 32)) return this.#msg('Regist Key must be 32 hex chars', 'error');
                payload.regist_key = regist;
            }
        }
        if (Object.keys(payload).length === 0) return this.#msg('Nothing to save', 'error');

        try {
            const r = await this.protocol.sendCommand('OUTPUT.NATIVE.SET', payload);
            this.#msg(`Saved (${r && r.status ? r.status : 'ok'})`, 'success');
            this.log && this.log('Remote Play config saved', 'success');
            setTimeout(() => this.refresh(), 500);
        } catch (e) {
            this.#msg(`Save failed: ${e.message}`, 'error');
        }
    }

    #msg(text, kind) {
        const e = this.el.querySelector('#rpStatusMsg');
        if (e) { e.textContent = text; e.className = 'hint ' + (kind === 'error' ? 'error' : 'success'); }
    }
}
