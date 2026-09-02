/**
 * WiFi / PS Remote Play Output Page (usb2wifi app).
 *
 * Provisions the Remote Play output: WiFi (scan + connect, station mode), PSN
 * sign-in, and the target PS5. Talks to the firmware's remoteplay
 * OutputInterface via the generic OUTPUT.NATIVE.GET/SET (type == "remoteplay").
 *
 * PSN sign-in is on-device: this page opens Sony's OAuth login in a new tab,
 * the user pastes back the post-login redirect URL, and the firmware does the
 * HTTPS token exchange itself ({psn_code}) — deriving the account id on-chip.
 * (The browser can't: Sony's endpoint blocks cross-origin fetches via CORS +
 * anti-bot edge.) RP-Key / Regist Key (from device pairing) are still manual
 * under "Advanced" until on-device registration lands.
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
                <h2>WiFi</h2>
                <div class="card-content">
                    <p class="hint">
                        Connect the adapter to your network. This is the adapter's base WiFi link —
                        used by PS Remote Play below, and by other WiFi features.
                    </p>

                    <div class="device-info">
                        <div class="row"><span class="label">Status</span><span class="value" id="rpWifiState">—</span></div>
                        <div class="row"><span class="label">Adapter IP</span><span class="value" id="rpIp">—</span></div>
                    </div>

                    <div class="button-row">
                        <button id="rpWifiScanBtn" title="Scan for networks">Scan for networks</button>
                        <button id="rpWifiOtherBtn" class="secondary" title="Join a hidden or unlisted network">Other network…</button>
                    </div>
                    <div id="rpAps" class="rp-ap-list"></div>

                    <!-- Connect dialog: opened by clicking an AP (SSID prefilled) or "Other network…" -->
                    <div id="rpWifiModal" class="rp-modal" hidden>
                        <div class="rp-modal-box">
                            <h3 id="rpWifiModalTitle" style="margin-top:0">Connect to WiFi</h3>
                            <div class="form-row"><label for="rpSsid">Network</label><input type="text" id="rpSsid" maxlength="32" placeholder="Network name"></div>
                            <div class="form-row"><label for="rpPass">Password</label><input type="password" id="rpPass" maxlength="63" placeholder="Leave blank if open"></div>
                            <div id="rpWifiMsg" class="hint"></div>
                            <div class="button-row" style="justify-content:flex-end">
                                <button id="rpWifiCancelBtn" class="secondary">Cancel</button>
                                <button id="rpWifiConnectBtn">Connect</button>
                            </div>
                        </div>
                    </div>

                    <style>
                        .rp-ap-list { display:flex; flex-direction:column; gap:4px; margin:8px 0; }
                        .rp-ap-row { display:flex; justify-content:space-between; align-items:center; gap:12px;
                            padding:9px 12px; border:1px solid var(--border,#333); border-radius:8px;
                            cursor:pointer; }
                        .rp-ap-row:hover { background:var(--hover,rgba(128,128,128,0.12)); }
                        .rp-ap-row.connected { border-color:#3cbe6e; background:rgba(60,190,110,0.08); }
                        .rp-ap-name { display:flex; align-items:center; gap:7px; min-width:0; }
                        .rp-ap-name .txt { overflow:hidden; text-overflow:ellipsis; white-space:nowrap; }
                        .rp-ap-right { display:flex; align-items:center; gap:10px; }
                        .rp-ap-check { color:#3cbe6e; }
                        /* signal strength: 4 rising bars, filled by level */
                        .rp-sig { display:inline-flex; align-items:flex-end; gap:2px; height:14px; }
                        .rp-sig i { width:3px; border-radius:1px; background:currentColor; opacity:0.22; }
                        .rp-sig i:nth-child(1){height:5px} .rp-sig i:nth-child(2){height:8px}
                        .rp-sig i:nth-child(3){height:11px} .rp-sig i:nth-child(4){height:14px}
                        .rp-sig.l1 i:nth-child(-n+1), .rp-sig.l2 i:nth-child(-n+2),
                        .rp-sig.l3 i:nth-child(-n+3), .rp-sig.l4 i:nth-child(-n+4) { opacity:1; }
                        .rp-modal { position:fixed; inset:0; background:rgba(0,0,0,0.5); display:flex;
                            align-items:center; justify-content:center; z-index:1000; }
                        .rp-modal[hidden] { display:none; }
                        .rp-modal-box { background:var(--card-bg,#1e1e1e); border:1px solid var(--border,#333);
                            border-radius:12px; padding:20px; width:min(90vw,380px);
                            box-shadow:0 10px 40px rgba(0,0,0,0.4); }
                        .rp-ap-row .rp-ap-sig { opacity:0.7; font-variant-numeric:tabular-nums; }
                    </style>
                </div>
            </div>

            <div class="card">
                <h2>PlayStation Remote Play</h2>
                <div class="card-content">
                    <p class="hint">
                        Play a PS5 over your network. Needs WiFi connected (above) plus a PSN
                        sign-in and a one-time console pairing — all on-device, no PC.
                    </p>

                    <div class="device-info">
                        <div class="row"><span class="label">PSN account</span><span class="value" id="rpAccountState">—</span></div>
                        <div class="row"><span class="label">Session</span><span class="value" id="rpSession">—</span></div>
                    </div>

                    <h3>PlayStation account</h3>
                    <p class="hint">
                        Sign in with your PSN account so the adapter can link to your console.
                        Click <b>Sign in</b>, log in on Sony's page, then copy the URL of the
                        blank/redirect page you land on and paste it below. The adapter completes
                        the sign-in on-device — nothing is entered by hand.
                    </p>
                    <div class="button-row">
                        <button id="rpSignInBtn" title="Open Sony's login in a new tab">Sign in to PlayStation</button>
                        <button id="rpUnlinkBtn" class="secondary" title="Clear saved PSN account + keys">Unlink</button>
                    </div>
                    <div class="form-row">
                        <label for="rpRedirect">Redirect URL</label>
                        <input type="text" id="rpRedirect" placeholder="https://remoteplay.dl.playstation.net/remoteplay/redirect?code=…">
                        <button id="rpCompleteBtn">Complete</button>
                    </div>
                    <div id="rpOauthMsg" class="hint"></div>

                    <h3>PlayStations</h3>
                    <div class="button-row">
                        <button id="rpScanBtn" title="Find consoles on your network">Scan for consoles</button>
                        <span id="rpScanMsg" class="hint"></span>
                    </div>
                    <div id="rpConsoles" class="rp-console-list"></div>
                    <div id="rpPairMsg" class="hint"></div>

                    <style>
                        .rp-console-list { display:flex; flex-direction:column; gap:6px; margin:8px 0; }
                        .rp-console { border:1px solid var(--border,#333); border-radius:8px; overflow:hidden; }
                        .rp-console-row { display:flex; align-items:center; gap:10px; padding:10px 12px; }
                        .rp-console-ico { font-size:20px; }
                        .rp-console-text { flex:1 1 auto; min-width:0; }
                        .rp-console-name { font-weight:600; }
                        .rp-console-sub { opacity:0.65; font-size:0.85em; font-variant-numeric:tabular-nums; }
                        .rp-badge { font-size:0.8em; padding:3px 9px; border-radius:999px; white-space:nowrap; }
                        .rp-badge.linked { background:rgba(60,190,110,0.18); color:#3cbe6e; }
                        .rp-badge.standby { background:rgba(128,128,128,0.16); opacity:0.8; }
                        .rp-link-panel { border-top:1px solid var(--border,#333); padding:10px 12px;
                            background:rgba(74,158,255,0.06); }
                        .rp-link-panel .form-row { margin:6px 0; }
                        .rp-empty { opacity:0.6; padding:10px 2px; }
                    </style>

                    <h3>Remote Play</h3>
                    <p class="hint">
                        Start a live session to the selected, paired console. Your controller
                        (plugged into the adapter) drives the PS5. <b>Heads-up:</b> while Remote Play
                        is active the console hands its video to the adapter, so your TV may go
                        blank — that's normal. Stop to hand it back.
                    </p>
                    <div class="button-row">
                        <button id="rpStreamBtn">Start Remote Play</button>
                        <span id="rpStreamMsg" class="hint"></span>
                    </div>

                    <details class="rp-advanced">
                        <summary>Advanced</summary>
                        <p class="hint">Manually target a console by IP (if discovery can't find it), or
                            paste paired keys from <code>remote-play-lab/rp.py</code>.</p>
                        <div class="form-row"><label for="rpPs5Ip">Console IP</label><input type="text" id="rpPs5Ip" placeholder="192.168.1.107"></div>
                        <div class="form-row"><label for="rpAccount">Account ID (16 hex)</label><input type="text" id="rpAccount" placeholder="d83c2a2b2d0c3809" maxlength="16"></div>
                        <div class="form-row"><label for="rpKey">RP-Key (32 hex)</label><input type="text" id="rpKey" maxlength="32"></div>
                        <div class="form-row"><label for="rpRegist">Regist Key (32 hex)</label><input type="text" id="rpRegist" maxlength="32"></div>
                        <div class="button-row">
                            <button id="rpSaveBtn">Save credentials</button>
                        </div>
                    </details>
                    <div id="rpStatusMsg" class="hint"></div>
                </div>
            </div>`;
        this.el.querySelector('#rpSaveBtn').addEventListener('click', () => this.save());
        this.el.querySelector('#rpScanBtn').addEventListener('click', () => this.scan());
        this.el.querySelector('#rpWifiScanBtn').addEventListener('click', () => this.wifiScan());
        this.el.querySelector('#rpWifiConnectBtn').addEventListener('click', () => this.connectWifi());
        this.el.querySelector('#rpWifiOtherBtn').addEventListener('click', () => this.openWifiModal('', true, 'Other network'));
        this.el.querySelector('#rpWifiCancelBtn').addEventListener('click', () => this.closeWifiModal());
        this.el.querySelector('#rpWifiModal').addEventListener('click', (e) => { if (e.target.id === 'rpWifiModal') this.closeWifiModal(); });
        this.el.querySelector('#rpSignInBtn').addEventListener('click', () => this.signIn());
        this.el.querySelector('#rpCompleteBtn').addEventListener('click', () => this.completeSignIn());
        this.el.querySelector('#rpUnlinkBtn').addEventListener('click', () => this.unlink());
        this.el.querySelector('#rpStreamBtn').addEventListener('click', () => this.toggleStream());
    }

    async toggleStream() {
        const on = !this._streaming;
        const btn = this.el.querySelector('#rpStreamBtn');
        btn.disabled = true;
        try {
            await this.protocol.sendCommand('OUTPUT.NATIVE.SET', { stream: on ? 1 : 0 });
            this.#streamMsg(on ? 'Starting… (TV may blank — that\'s Remote Play)' : 'Stopping…', 'success');
            setTimeout(() => this.refresh(), 400);
        } catch (e) {
            this.#streamMsg(`Failed: ${e.message}`, 'error');
        }
        btn.disabled = false;
    }

    #streamMsg(text, kind) {
        const e = this.el.querySelector('#rpStreamMsg');
        if (e) { e.textContent = text; e.className = 'hint ' + (kind === 'error' ? 'error' : 'success'); }
    }

    #pairMsg(text, kind) {
        const e = this.el.querySelector('#rpPairMsg');
        if (e) { e.textContent = text; e.className = 'hint ' + (kind === 'error' ? 'error' : 'success'); }
    }

    async unlink() {
        if (!confirm('Clear the saved PSN account and pairing keys from this adapter?')) return;
        try {
            await this.protocol.sendCommand('OUTPUT.NATIVE.SET', { rp_reset: 1 });
            this.#oauthMsg('Unlinked — sign in again to relink.', 'success');
            setTimeout(() => this.refresh(), 300);
        } catch (e) {
            this.#oauthMsg(`Unlink failed: ${e.message}`, 'error');
        }
    }

    // Sony OAuth authorize URL — mirrors mouthpad-utility RPOAuth.swift. The full
    // 4-scope set is required; single-scope psn:clientapp returns "Something went
    // wrong." after login.
    #loginURL() {
        const clientId = 'ba495a24-818c-472b-b12d-ff231c1b5745';
        const redirect = 'https://remoteplay.dl.playstation.net/remoteplay/redirect';
        const scope = [
            'psn:clientapp',
            'referenceDataService:countryConfig.read',
            'pushNotification:webSocket.desktop.connect',
            'sessionManager:remotePlaySession.system.update',
        ].join(' ');
        const q = new URLSearchParams({
            service_entity: 'urn:service-entity:psn',
            response_type: 'code',
            client_id: clientId,
            redirect_uri: redirect,
            scope,
            request_locale: 'en_US',
            ui: 'pr',
            service_logo: 'ps',
            layout_type: 'popup',
            smcid: 'remoteplay',
            prompt: 'always',
            PlatformPrivacyWs1: 'minimal',
        });
        return 'https://auth.api.sonyentertainmentnetwork.com/2.0/oauth/authorize?' + q.toString();
    }

    signIn() {
        window.open(this.#loginURL(), '_blank', 'noopener');
        this.#oauthMsg('Log in on the new tab, then paste the redirect URL you land on below.', 'success');
        this.el.querySelector('#rpRedirect')?.focus();
    }

    // Pull the ?code=… out of a pasted redirect URL (or accept a bare code).
    #extractCode(s) {
        s = (s || '').trim();
        if (!s) return null;
        try {
            const u = new URL(s);
            const code = u.searchParams.get('code');
            if (code) return code;
        } catch { /* not a URL — maybe a bare code */ }
        const m = s.match(/[?&]code=([^&\s]+)/);
        if (m) return decodeURIComponent(m[1]);
        if (/^[A-Za-z0-9._-]{8,}$/.test(s)) return s;   // looks like a bare code
        return null;
    }

    async completeSignIn() {
        const raw = this.el.querySelector('#rpRedirect').value;
        const code = this.#extractCode(raw);
        if (!code) { this.#oauthMsg('Paste the full redirect URL (it contains ?code=…)', 'error'); return; }
        const btn = this.el.querySelector('#rpCompleteBtn');
        btn.disabled = true; btn.textContent = 'Signing in…';
        try {
            const r = await this.protocol.sendCommand('OUTPUT.NATIVE.SET', { psn_code: code });
            if (r && r.status && r.status !== 'signing-in') {
                this.#oauthMsg(`Could not start: ${r.error || r.status}`, 'error');
            } else {
                this.#oauthMsg('Exchanging token on device…', 'success');
                // Poll oauth state until it settles (device does the HTTPS exchange).
                for (let i = 0; i < 20; i++) {
                    await new Promise(r => setTimeout(r, 1000));
                    await this.refresh();
                    const st = this._oauth || '';
                    if (st === 'done') { this.#oauthMsg('Signed in ✓', 'success'); this.el.querySelector('#rpRedirect').value = ''; break; }
                    if (st === 'error') { this.#oauthMsg(`Sign-in failed: ${this._oauthError || 'unknown error'}`, 'error'); break; }
                }
            }
        } catch (e) {
            this.#oauthMsg(`Sign-in failed: ${e.message}`, 'error');
        }
        btn.disabled = false; btn.textContent = 'Complete';
    }

    #oauthMsg(text, kind) {
        const e = this.el.querySelector('#rpOauthMsg');
        if (e) { e.textContent = text; e.className = 'hint ' + (kind === 'error' ? 'error' : 'success'); }
    }

    openWifiModal(ssid, editable, title) {
        const m = this.el.querySelector('#rpWifiModal');
        const ssidInput = this.el.querySelector('#rpSsid');
        this.el.querySelector('#rpWifiModalTitle').textContent = title || (ssid ? `Connect to “${ssid}”` : 'Connect to WiFi');
        ssidInput.value = ssid || '';
        ssidInput.readOnly = !editable;
        this.el.querySelector('#rpPass').value = '';
        this.#wifiMsg('', '');
        m.hidden = false;
        (editable ? ssidInput : this.el.querySelector('#rpPass')).focus();
    }

    closeWifiModal() {
        const m = this.el.querySelector('#rpWifiModal');
        if (m) m.hidden = true;
    }

    async connectWifi() {
        const ssid = this.el.querySelector('#rpSsid').value.trim();
        const pass = this.el.querySelector('#rpPass').value;
        if (!ssid) { this.#wifiMsg('Enter a network name', 'error'); return; }
        const btn = this.el.querySelector('#rpWifiConnectBtn');
        btn.disabled = true; btn.textContent = 'Connecting…';
        try {
            await this.protocol.sendCommand('OUTPUT.NATIVE.SET', { wifi_ssid: ssid, wifi_pass: pass });
            this.#wifiMsg(`Connecting to “${ssid}”…`, 'success');
            let done = false;
            for (let i = 0; i < 12; i++) {
                await new Promise(r => setTimeout(r, 1000));
                await this.refresh();
                const st = this._wifiState || '';
                if (st.startsWith('connected')) { done = true; this.closeWifiModal(); break; }
                if (st.startsWith('failed'))    { this.#wifiMsg('Connection failed — check the password', 'error'); break; }
            }
            if (!done && !(this._wifiState || '').startsWith('failed')) this.#wifiMsg('Still connecting… you can close this.', 'success');
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

    // RSSI (~ -30 strong .. -90 weak) -> 1..4 bars, rendered as 4 rising bars.
    #sigBars(rssi) {
        const l = rssi >= -55 ? 4 : rssi >= -67 ? 3 : rssi >= -78 ? 2 : 1;
        return `<span class="rp-sig l${l}" title="${rssi} dBm"><i></i><i></i><i></i><i></i></span>`;
    }

    renderAps(aps) {
        const box = this.el.querySelector('#rpAps');
        if (!box) return;
        if (!aps || !aps.length) { box.innerHTML = ''; return; }
        const connected = this._wifiState && this._wifiState.startsWith('connected') ? (this._wifiSsid || '') : '';
        box.innerHTML = aps.map(a => {
            const s = this.#esc(a.ssid || '');
            const isConn = a.ssid && a.ssid === connected;
            return `<div class="rp-ap-row${isConn ? ' connected' : ''}" data-ssid="${s}" data-secure="${a.secure ? 1 : 0}">
                <span class="rp-ap-name">
                    <span>${a.secure ? '🔒' : '🔓'}</span>
                    <span class="txt">${s || '(hidden)'}</span>
                </span>
                <span class="rp-ap-right">
                    ${isConn ? '<span class="rp-ap-check">✓ Connected</span>' : ''}
                    ${this.#sigBars(a.rssi)}
                </span>
            </div>`;
        }).join('');
        box.querySelectorAll('.rp-ap-row').forEach(row => row.addEventListener('click', () => {
            const ssid = row.getAttribute('data-ssid');
            if (ssid === connected) return;   // already on this one
            this.openWifiModal(ssid, false, `Connect to “${ssid}”`);
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

    #esc(s) { return (s || '').replace(/[&<>"]/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c])); }

    // One clean row per discovered console, with linked/not-linked state and an
    // inline Link → PIN flow. `this._expandedIp` is the console whose link panel
    // is open; refresh() freezes the list while a panel is open so typing/focus
    // isn't clobbered by polling.
    renderConsoles(hosts) {
        const box = this.el.querySelector('#rpConsoles');
        if (!box) return;
        hosts = hosts || [];
        const linkedIp = this._haveReg ? (this._linkedIp || '') : '';
        if (!hosts.length) {
            box.innerHTML = `<div class="rp-empty">No consoles found yet. Make sure your PS5 is on
                (or in rest mode with Remote Play enabled) and on this network, then Scan.</div>`;
            return;
        }
        box.innerHTML = hosts.map(h => {
            const linked = h.ip === linkedIp;
            const type = h.ps5 ? 'PS5' : 'PS4';
            const action = linked
                ? `<span class="rp-badge linked">Linked ✓</span>`
                : `<button class="rp-link-btn" data-ip="${h.ip}">Link</button>`;
            const panel = (this._expandedIp === h.ip && !linked)
                ? `<div class="rp-link-panel">
                       <p class="hint" style="margin-top:0">On the console: <b>Settings → System →
                         Remote Play → Link Device</b>, then enter the 8-digit PIN it shows.</p>
                       <div class="form-row">
                         <input type="text" class="rp-pin" inputmode="numeric" maxlength="8" placeholder="12345678" value="${this._pinDraft || ''}">
                         <button class="rp-pair-btn" data-ip="${h.ip}">Pair</button>
                         <button class="rp-cancel-btn secondary">Cancel</button>
                       </div>
                     </div>`
                : '';
            return `<div class="rp-console" data-ip="${h.ip}">
                <div class="rp-console-row">
                    <span class="rp-console-ico">🎮</span>
                    <div class="rp-console-text">
                        <div class="rp-console-name">${this.#esc(h.name)}</div>
                        <div class="rp-console-sub">${type} · ${h.ready ? 'ready' : 'rest mode'} · ${h.ip}</div>
                    </div>
                    ${action}
                </div>
                ${panel}
            </div>`;
        }).join('');

        box.querySelectorAll('.rp-link-btn').forEach(b => b.addEventListener('click', () => {
            this._expandedIp = b.getAttribute('data-ip');
            this._pinDraft = '';
            this.renderConsoles(this._hosts);
            this.el.querySelector('.rp-console[data-ip="' + this._expandedIp + '"] .rp-pin')?.focus();
        }));
        box.querySelectorAll('.rp-cancel-btn').forEach(b => b.addEventListener('click', () => {
            this._expandedIp = null; this._pinDraft = '';
            this.#pairMsg('', '');
            this.renderConsoles(this._hosts);
        }));
        box.querySelectorAll('.rp-pin').forEach(inp => inp.addEventListener('input', () => {
            this._pinDraft = inp.value;
        }));
        box.querySelectorAll('.rp-pair-btn').forEach(b => b.addEventListener('click', () => {
            this.pair(b.getAttribute('data-ip'), (this._pinDraft || '').trim());
        }));
    }

    async pair(ip, pin) {
        if (!ip) { this.#pairMsg('No console selected', 'error'); return; }
        if (!/^\d{8}$/.test(pin)) { this.#pairMsg('Enter the 8-digit PIN from the console', 'error'); return; }
        this._pairing = true;
        this.#pairMsg('Registering with console…', 'success');
        try {
            await this.protocol.sendCommand('OUTPUT.NATIVE.SET', { ps5_ip: ip });
            const r = await this.protocol.sendCommand('OUTPUT.NATIVE.SET', { pair_pin: pin });
            if (r && r.status && r.status !== 'pairing') {
                this.#pairMsg(`Couldn't start: ${r.status}`, 'error');
            } else {
                for (let i = 0; i < 15; i++) {
                    await new Promise(res => setTimeout(res, 1000));
                    await this.refresh();
                    const st = this._regist || '';
                    if (st === 'paired') { this.#pairMsg('Paired ✓', 'success'); this._expandedIp = null; this._pinDraft = ''; break; }
                    if (st === 'error') { this.#pairMsg(`Pairing failed: ${this._registError || 'unknown'}`, 'error'); break; }
                    if (st === 'unavailable') { this.#pairMsg('This firmware build has pairing stubbed — flash the latest usb2wifi.', 'error'); break; }
                }
            }
        } catch (e) {
            this.#pairMsg(`Pair failed: ${e.message}`, 'error');
        }
        this._pairing = false;
        this.renderConsoles(this._hosts);
    }

    async refresh() {
        try {
            const r = await this.protocol.sendCommand('OUTPUT.NATIVE.GET');
            if (!r || r.type !== 'remoteplay') { this.available = false; return; }
            this.available = true;
            if (!this.el.querySelector('#rpSaveBtn')) this.render();
            const set = (id, v) => { const e = this.el.querySelector(id); if (e) e.textContent = v; };
            this._wifiState = r.wifi_state || '';
            this._wifiSsid = r.wifi_ssid || '';
            set('#rpWifiState', `${r.wifi_state || '—'}${r.wifi_ssid ? ' (' + r.wifi_ssid + ')' : ''}`);
            set('#rpIp', r.ip || '—');
            const sessionLabel = {
                'engine-not-built': r.have_registration
                    ? 'paired ✓ — streaming engine not built yet'
                    : 'not available yet (streaming engine not built)',
                'idle': r.have_registration ? 'paired ✓ (idle)' : 'idle — needs pairing (Link PIN)',
                'connecting': 'connecting…',
                'ready': 'connected ✓',
                'error': 'error',
            }[r.session] || (r.session || '—');
            set('#rpSession', sessionLabel);
            this._oauth = r.oauth || '';
            this._oauthError = r.oauth_error || '';
            this._regist = r.regist || '';
            this._registError = r.regist_error || '';
            this._streaming = !!r.streaming;
            const sbtn = this.el.querySelector('#rpStreamBtn');
            if (sbtn) {
                sbtn.textContent = this._streaming ? 'Stop Remote Play' : 'Start Remote Play';
                sbtn.classList.toggle('secondary', this._streaming);
                sbtn.disabled = !r.have_registration;
            }
            this._haveReg = !!r.have_registration;
            this._linkedIp = r.ps5_ip || '';
            this._hosts = r.hosts || [];
            const acctStr = r.have_account
                ? (r.psn_online_id ? `signed in as ${r.psn_online_id}` : 'signed in ✓')
                : (r.oauth && r.oauth !== 'idle' && r.oauth !== 'done'
                    ? r.oauth + '…'
                    : 'not signed in');
            set('#rpAccountState', acctStr);
            // Freeze the console list while a link panel is open / pairing so the
            // PIN input and focus aren't clobbered by polling.
            if (!this._expandedIp && !this._pairing) this.renderConsoles(this._hosts);
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
