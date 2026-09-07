/** Input/Output Stream Test Card — Per-Player, Per-Source */

const STREAM_BUTTONS = [
    { bit: 0, label: 'B1' }, { bit: 1, label: 'B2' },
    { bit: 2, label: 'B3' }, { bit: 3, label: 'B4' },
    { bit: 4, label: 'L1' }, { bit: 5, label: 'R1' },
    { bit: 6, label: 'L2' }, { bit: 7, label: 'R2' },
    { bit: 8, label: 'S1' }, { bit: 9, label: 'S2' },
    { bit: 10, label: 'L3' }, { bit: 11, label: 'R3' },
    { bit: 20, label: 'L4' }, { bit: 21, label: 'R4' },
    { bit: 24, label: 'L5' }, { bit: 25, label: 'R5' },
    { bit: 12, label: 'DU' }, { bit: 13, label: 'DD' },
    { bit: 14, label: 'DL' }, { bit: 15, label: 'DR' },
    { bit: 16, label: 'A1' }, { bit: 17, label: 'A2' },
    { bit: 18, label: 'A3' }, { bit: 19, label: 'A4' },
];

const AXES = ['LX', 'LY', 'RX', 'RY', 'L2', 'R2'];

function renderButtons(id) {
    return `<div class="buttons" id="${id}">${
        STREAM_BUTTONS.map(b => `<span class="btn" data-bit="${b.bit}">${b.label}</span>`).join('')
    }</div>`;
}

function renderAxes(prefix) {
    return `<div class="axes">${AXES.map((name, i) => {
        const val = i < 4 ? 128 : 0;
        const pct = i < 4 ? '50%' : '0%';
        return `<div class="axis">
            <div>${name}: <span id="${prefix}Axis${name}">${val}</span></div>
            <div class="axis-bar" id="${prefix}Bar${i}" data-axis="${i}"><div class="axis-bar-fill" id="${prefix}Axis${name}Bar" style="width:${pct}"></div></div>
        </div>`;
    }).join('')}</div>`;
}

// Rest value per axis: sticks center at 128, triggers release to 0 (RZ centered).
function axisNeutral(idx) { return (idx === 4 || idx === 5) ? 0 : 128; }

export class InputTestCard {
    constructor(container, protocol, log) {
        this.protocol = protocol;
        this.log = log;
        this.el = container;
        this.streaming = false;
        this.players = {};  // keyed by player index
        this.pendingUpdates = {};  // buffered display updates keyed by prefix
        this.rafScheduled = false;
        // Manual inject (INPUT.INJECT): held button bitmask driven by mouse clicks
        // on the inject pad. Sends are coalesced/serialized so rapid presses only
        // ever transmit the latest mask and frames never interleave.
        this.injectMask = 0;
        this.injectSending = false;
        this.injectDirty = false;
        // Injected analog axes [LX,LY,RX,RY,L2,R2,RZ]; only sent while
        // injectAnalogActive (a drag is engaged), else axes go back to the real
        // controller (INPUT.INJECT "analog":false).
        this.injectAnalog = [128, 128, 128, 128, 0, 0, 128];
        this.injectAnalogActive = false;
    }

    render() {
        this.el.innerHTML = `
            <div class="card">
                <div class="card-header">
                    <h2>Input Test</h2>
                    <div style="display: flex; gap: 8px; align-items: center;">
                        <button id="injectClearBtn" class="btn-sm secondary" title="Release all injected buttons">Release All</button>
                        <button id="rumbleBtn" class="btn-sm secondary" title="Test rumble">Rumble</button>
                        <button id="streamBtn" class="btn-sm secondary">Start Stream</button>
                    </div>
                </div>
                <div id="playerGroups" class="player-groups">
                    <p class="hint" id="streamHint">Start streaming to see connected controllers. Click the Output buttons to drive input over serial (INPUT.INJECT).</p>
                </div>
            </div>`;

        this.el.querySelector('#streamBtn').addEventListener('click', () => this.toggleStreaming());
        this.el.querySelector('#rumbleBtn').addEventListener('click', () => this.testRumble());
        this.el.querySelector('#injectClearBtn').addEventListener('click', () => this.injectClear());
    }

    // Make an Output (Merged) button row clickable to inject: pointer-down presses a
    // bit, pointer-up/leave/cancel releases it. Pointer (not click) gives real
    // press-and-hold so combos work. The pressed visual is optimistic; when streaming
    // the round-tripped output confirms it via updateDisplay.
    wireOutputInject(prefix) {
        const row = this.el.querySelector(`#${prefix}Btns`);
        if (!row || row.dataset.injectWired) return;
        row.dataset.injectWired = '1';
        row.style.cursor = 'pointer';
        row.style.touchAction = 'none';
        row.style.userSelect = 'none';
        row.querySelectorAll('.btn').forEach(btn => {
            const bit = parseInt(btn.dataset.bit);
            const press = (e) => { e.preventDefault(); btn.classList.add('pressed'); this.injectSet(this.injectMask | (1 << bit)); };
            const release = () => {
                if (!(this.injectMask & (1 << bit))) return;  // not injecting this bit
                btn.classList.remove('pressed');  // optimistic (stream confirms when live)
                this.injectSet(this.injectMask & ~(1 << bit));
            };
            btn.addEventListener('pointerdown', press);
            btn.addEventListener('pointerup', release);
            btn.addEventListener('pointerleave', release);
            btn.addEventListener('pointercancel', release);
        });
        this.wireAxisDrag(prefix);
    }

    // Make the Output axis bars click-and-draggable: drag left/right to set the
    // axis (0..255), release to spring back to rest (stick=center, trigger=0).
    // Drives INPUT.INJECT "analog"; when every axis is at rest the axes are handed
    // back to the real controller.
    wireAxisDrag(prefix) {
        for (let i = 0; i < 6; i++) {
            const bar = this.el.querySelector(`#${prefix}Bar${i}`);
            if (!bar || bar.dataset.injectWired) continue;
            bar.dataset.injectWired = '1';
            bar.style.cursor = 'ew-resize';
            bar.style.touchAction = 'none';
            const valueAt = (e) => {
                const r = bar.getBoundingClientRect();
                let v = Math.round((e.clientX - r.left) / r.width * 255);
                return Math.max(0, Math.min(255, v));
            };
            const setAxis = (v) => {
                if (this.injectAnalog[i] === v && this.injectAnalogActive) return;
                this.injectAnalog[i] = v;
                this.injectAnalogActive = true;
                this.updateAxisVisual(prefix, i, v);
                this.injectDirty = true;
                if (!this.injectSending) this.flushInject();
            };
            bar.addEventListener('pointerdown', (e) => { e.preventDefault(); bar.setPointerCapture(e.pointerId); setAxis(valueAt(e)); });
            bar.addEventListener('pointermove', (e) => { if (bar.hasPointerCapture(e.pointerId)) setAxis(valueAt(e)); });
            const end = (e) => {
                if (!bar.hasPointerCapture?.(e.pointerId)) return;
                bar.releasePointerCapture(e.pointerId);
                this.injectAnalog[i] = axisNeutral(i);
                this.updateAxisVisual(prefix, i, this.injectAnalog[i]);
                // If every axis is back at rest, release analog to the real controller.
                const allRest = this.injectAnalog.every((v, idx) => v === axisNeutral(idx));
                if (allRest) this.injectAnalogActive = false;
                this.injectDirty = true;
                if (!this.injectSending) this.flushInject();
            };
            bar.addEventListener('pointerup', end);
            bar.addEventListener('pointercancel', end);
        }
    }

    updateAxisVisual(prefix, idx, v) {
        const name = AXES[idx];
        const el = document.getElementById(`${prefix}Axis${name}`);
        const bar = document.getElementById(`${prefix}Axis${name}Bar`);
        if (el) el.textContent = v;
        if (bar) bar.style.width = (v / 255 * 100) + '%';
    }

    injectClear() {
        // Clear optimistic pressed state on the clickable Output rows (stream, if
        // live, will immediately repaint real controller state).
        this.el.querySelectorAll('.buttons[data-inject-wired] .btn.pressed')
            .forEach(b => b.classList.remove('pressed'));
        this.injectAnalog = [128, 128, 128, 128, 0, 0, 128];
        this.injectAnalogActive = false;
        this.injectSet(0);
        this.injectDirty = true;
        if (!this.injectSending) this.flushInject();
    }

    injectSet(mask) {
        mask = mask >>> 0;  // keep unsigned (L5/R5 live at bits 24/25)
        if (mask === this.injectMask) return;
        this.injectMask = mask;
        this.injectDirty = true;
        if (!this.injectSending) this.flushInject();
    }

    async flushInject() {
        this.injectSending = true;
        try {
            while (this.injectDirty) {
                this.injectDirty = false;
                // analog: array holds the axes while dragging; false hands them back
                // to the real controller when no drag is engaged.
                const args = {
                    buttons: this.injectMask,
                    analog: this.injectAnalogActive ? this.injectAnalog.slice() : false,
                };
                try {
                    await this.protocol.sendCommand('INPUT.INJECT', args);
                } catch (e) {
                    this.log(`Inject failed: ${e.message}`, 'error');
                    break;
                }
            }
        } finally {
            this.injectSending = false;
        }
    }

    handleEvent(event) {
        if (event.type === 'input') {
            const player = event.player !== undefined ? event.player : 0;
            const addr = event.addr !== undefined ? event.addr : 0;
            this.ensurePlayerGroup(player);
            this.ensureInputSource(player, addr, event.name || '', event.src || '');
            this.scheduleUpdate(`p${player}a${addr}`, event.buttons, event.axes);
        } else if (event.type === 'output') {
            const player = event.player !== undefined ? event.player : 0;
            this.ensurePlayerGroup(player);
            this.scheduleUpdate(`p${player}out`, event.buttons, event.axes);
        } else if (event.type === 'connect') {
            this.log(`Controller connected: ${event.name} (${event.vid}:${event.pid})`);
        } else if (event.type === 'disconnect') {
            this.log(`Controller disconnected: port ${event.port}`);
            this.removePlayerGroup(event.port);
        }
    }

    scheduleUpdate(prefix, buttons, axes) {
        this.pendingUpdates[prefix] = { buttons, axes };
        if (!this.rafScheduled) {
            this.rafScheduled = true;
            requestAnimationFrame(() => {
                for (const [pfx, data] of Object.entries(this.pendingUpdates)) {
                    this.updateDisplay(pfx, data.buttons, data.axes);
                }
                this.pendingUpdates = {};
                this.rafScheduled = false;
            });
        }
    }

    ensurePlayerGroup(player) {
        if (this.players[player]) return;

        // Hide hint
        const hint = this.el.querySelector('#streamHint');
        if (hint) hint.style.display = 'none';

        const container = this.el.querySelector('#playerGroups');
        const group = document.createElement('div');
        group.className = 'player-group';
        group.id = `playerGroup${player}`;
        group.innerHTML = `
            <div class="player-header">Player ${player + 1}</div>
            <div class="player-inputs" id="playerInputs${player}"></div>
            <div class="player-output">
                <div class="source-label">Output (Merged)</div>
                <div class="input-display compact">
                    ${renderButtons(`p${player}outBtns`)}
                    ${renderAxes(`p${player}out`)}
                </div>
            </div>`;
        container.appendChild(group);

        this.players[player] = { sources: {} };

        // The Output (Merged) row doubles as the manual-inject pad — click its
        // buttons to drive input over serial (INPUT.INJECT).
        this.wireOutputInject(`p${player}out`);
    }

    ensureInputSource(player, addr, name, source) {
        const key = `${player}:${addr}`;
        if (this.players[player].sources[addr]) {
            // Update name if we have one and it changed
            if (name) {
                const label = this.el.querySelector(`#srcLabel${player}a${addr}`);
                if (label) {
                    const text = source ? `${name} (${source})` : name;
                    if (label.textContent !== text) label.textContent = text;
                }
            }
            return;
        }

        const inputsContainer = this.el.querySelector(`#playerInputs${player}`);
        const row = document.createElement('div');
        row.className = 'input-source';
        row.id = `inputSrc${player}a${addr}`;
        row.innerHTML = `
            <div class="source-label" id="srcLabel${player}a${addr}">${source ? `${name} (${source})` : name}</div>
            <div class="input-display compact">
                ${renderButtons(`p${player}a${addr}Btns`)}
                ${renderAxes(`p${player}a${addr}`)}
            </div>`;
        inputsContainer.appendChild(row);

        this.players[player].sources[addr] = true;
    }

    removePlayerGroup(player) {
        const group = this.el.querySelector(`#playerGroup${player}`);
        if (group) group.remove();
        delete this.players[player];

        // Show hint if no players left
        if (Object.keys(this.players).length === 0) {
            const hint = this.el.querySelector('#streamHint');
            if (hint) hint.style.display = '';
        }
    }

    updateDisplay(prefix, buttons, axes) {
        const btns = this.el.querySelectorAll(`#${prefix}Btns .btn`);
        btns.forEach(btn => {
            const bit = parseInt(btn.dataset.bit);
            btn.classList.toggle('pressed', (buttons & (1 << bit)) !== 0);
        });

        if (axes && axes.length >= 6) {
            for (let i = 0; i < AXES.length; i++) {
                const name = AXES[i];
                const el = document.getElementById(`${prefix}Axis${name}`);
                const bar = document.getElementById(`${prefix}Axis${name}Bar`);
                if (el) el.textContent = axes[i];
                if (bar) bar.style.width = (axes[i] / 255 * 100) + '%';
            }
        }
    }

    async toggleStreaming() {
        const btn = this.el.querySelector('#streamBtn');
        try {
            this.streaming = !this.streaming;
            await this.protocol.enableInputStream(this.streaming);
            btn.textContent = this.streaming ? 'Stop Stream' : 'Start Stream';
            btn.style.background = this.streaming ? 'var(--success)' : '';
            this.log(this.streaming ? 'Input streaming enabled' : 'Input streaming disabled');

            if (this.streaming) {
                // Always show Player 1 so its Output row is clickable to inject even
                // with no controller connected, then fill in any real players.
                this.ensurePlayerGroup(0);
                await this.refreshPlayers();
            } else {
                // Release any held inject and clear all player groups.
                this.injectClear();
                this.players = {};
                const container = this.el.querySelector('#playerGroups');
                container.innerHTML = '<p class="hint" id="streamHint">Start streaming to see connected controllers. Click the Output buttons to drive input over serial (INPUT.INJECT).</p>';
            }
        } catch (e) {
            this.log(`Failed to toggle streaming: ${e.message}`, 'error');
            this.streaming = false;
        }
    }

    async refreshPlayers() {
        try {
            const result = await this.protocol.getPlayers();
            if (result.players && result.players.length > 0) {
                for (const p of result.players) {
                    const player = p.slot !== undefined ? p.slot : 0;
                    this.ensurePlayerGroup(player);
                    // Don't pre-create input source rows — streaming events
                    // will create them with the correct dev_addr and name
                }
            }
        } catch (e) {
            console.log('Failed to get players:', e.message);
        }
    }

    async testRumble() {
        try {
            this.log('Testing rumble...');
            await this.protocol.testRumble(0, 200, 200, 500);
            this.log('Rumble test sent', 'success');
        } catch (e) {
            this.log(`Rumble test failed: ${e.message}`, 'error');
        }
    }

    async stop() {
        // Release any held inject first so we never leave the board with a stuck
        // synthetic mask (the router holds INPUT.INJECT state until cleared/reboot).
        if (this.injectMask || this.injectAnalogActive) {
            this.injectMask = 0;
            this.injectAnalogActive = false;
            this.injectAnalog = [128, 128, 128, 128, 0, 0, 128];
            try { await this.protocol.sendCommand('INPUT.INJECT', { buttons: 0, analog: false }); } catch (e) { /* ignore */ }
        }
        if (this.streaming) {
            try { await this.protocol.enableInputStream(false); } catch (e) { /* ignore */ }
            this.streaming = false;
        }
    }
}
