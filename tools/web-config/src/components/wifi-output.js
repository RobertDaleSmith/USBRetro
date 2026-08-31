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
                    <div class="form-row"><label for="rpSsid">SSID</label><input type="text" id="rpSsid" maxlength="32"></div>
                    <div class="form-row"><label for="rpPass">Password</label><input type="password" id="rpPass" maxlength="63"></div>

                    <h3>PlayStation 5</h3>
                    <div class="form-row"><label for="rpPs5Ip">PS5 IP</label><input type="text" id="rpPs5Ip" placeholder="192.168.1.107"></div>
                    <div class="form-row"><label for="rpAccount">Account ID (16 hex)</label><input type="text" id="rpAccount" placeholder="d83c2a2b2d0c3809" maxlength="16"></div>
                    <div class="form-row"><label for="rpKey">RP-Key (32 hex)</label><input type="text" id="rpKey" maxlength="32"></div>
                    <div class="form-row"><label for="rpRegist">Regist Key (32 hex)</label><input type="text" id="rpRegist" maxlength="32"></div>

                    <div class="button-row">
                        <button id="rpSaveBtn">Save to device</button>
                    </div>
                    <div id="rpStatusMsg" class="hint"></div>
                </div>
            </div>`;
        this.el.querySelector('#rpSaveBtn').addEventListener('click', () => this.save());
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
        const payload = {};
        const ssid = this.el.querySelector('#rpSsid').value.trim();
        const pass = this.el.querySelector('#rpPass').value;
        const ip = this.el.querySelector('#rpPs5Ip').value.trim();
        const acct = this.el.querySelector('#rpAccount').value.trim();
        const key = this.el.querySelector('#rpKey').value.trim();
        const regist = this.el.querySelector('#rpRegist').value.trim();

        if (ssid) { payload.wifi_ssid = ssid; payload.wifi_pass = pass; }
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
