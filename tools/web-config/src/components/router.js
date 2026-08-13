import { DirtyTracker } from './dirty-tracker.js';

/** Router Page — Topology, Input routing, and D-Pad mode configuration */
export class RouterCard {
    constructor(container, protocol, log) {
        this.protocol = protocol;
        this.log = log;
        this.el = container;
    }

    render() {
        this.el.innerHTML = `
            <div class="card">
                <h2>Router</h2>
                <div class="card-content">
                    <div class="pad-form-row">
                        <span class="label">Routing Mode</span>
                        <select id="routingMode">
                            <option value="0">Simple (1:1)</option>
                            <option value="1">Merge (N:1)</option>
                            <option value="2" disabled>Broadcast (1:N)</option>
                        </select>
                    </div>
                    <div class="pad-form-row" id="mergeModeRow">
                        <span class="label">Merge Mode</span>
                        <select id="mergeMode">
                            <option value="0">Priority</option>
                            <option value="1">Blend</option>
                            <option value="2">Latest</option>
                        </select>
                    </div>
                    <div class="buttons" style="margin-top: 12px;">
                        <button id="routerSaveBtn">Save &amp; Reboot</button>
                    </div>
                    <p class="hint" style="margin-top: 8px;">Device will reboot to apply changes.</p>
                </div>
            </div>

            <div class="card" id="topologyCard">
                <h2>I/O</h2>
                <div class="card-content">
                    <div id="topologyBody" class="hint">Loading…</div>
                </div>
            </div>

            <div class="card">
                <h2>D-Pad / Stick Swap</h2>
                <div class="card-content">
                    <div class="row">
                        <span class="label">Mode</span>
                        <select id="dpadMode">
                            <option value="0">Normal</option>
                            <option value="1">D-Pad ↔ Left Stick</option>
                            <option value="2">D-Pad ↔ Right Stick</option>
                            <option value="3">Left ↔ Right Stick</option>
                        </select>
                    </div>
                    <p class="hint">Swaps the d-pad with a stick (both directions), or swaps the two sticks. Also on-controller: SELECT + D-pad Left/Right. Applies to all input sources.</p>
                </div>
            </div>`;

        this.el.querySelector('#routingMode').addEventListener('change', (e) => {
            this.el.querySelector('#mergeModeRow').style.display = e.target.value === '1' ? '' : 'none';
        });
        this.el.querySelector('#routerSaveBtn').addEventListener('click', () => this.save());
        this.el.querySelector('#dpadMode').addEventListener('change', (e) => this.setDpadMode(e.target.value));

        // Dirty tracking — only the routing/merge mode card needs save+reboot
        this.dirty = new DirtyTracker(
            this.el.querySelectorAll('.card')[0],  // first card (Router config)
            this.el.querySelector('#routerSaveBtn')
        );
    }

    async load() {
        try {
            const config = await this.protocol.getRouter();
            this.el.querySelector('#routingMode').value = config.routing_mode || 0;
            this.el.querySelector('#mergeMode').value = config.merge_mode || 0;
            this.el.querySelector('#dpadMode').value = config.dpad_mode || 0;
            this.el.querySelector('#mergeModeRow').style.display =
                (config.routing_mode || 0) === 1 ? '' : 'none';
            this.dirty?.snapshot();
        } catch (e) {
            this.log(`Failed to load router config: ${e.message}`, 'error');
        }

        // Load topology (CAPS.GET) — gracefully no-op on older firmware
        try {
            const caps = await this.protocol.getCapabilities();
            this.renderTopology(caps);
        } catch (e) {
            const body = this.el.querySelector('#topologyBody');
            if (body) body.textContent = 'Capabilities not reported by this firmware.';
        }
    }

    renderTopology(caps) {
        const body = this.el.querySelector('#topologyBody');
        if (!body) return;

        const inputs = caps.inputs || [];
        const outputs = caps.outputs || [];
        const routes = caps.routes || [];
        const routing = caps.routing || {};

        const modeLabel = (routing.mode_name || '').replace(/^./, c => c.toUpperCase()) || '—';
        const showMerge = routing.mode_name === 'merge';
        const mergeLabel = (routing.merge_mode_name || '').replace(/^./, c => c.toUpperCase());

        const inputCard = inputs.length === 0
            ? '<div class="topology-empty">No inputs registered</div>'
            : inputs.map(i => `
                <div class="topology-node" data-source="${i.source}">
                    <div class="topology-node-name">${i.name}</div>
                    <div class="topology-node-meta">
                        <span class="topology-tag">${i.source_name}</span>
                        ${i.connected === true ? `<span class="topology-tag topology-tag-ok">${i.devices} connected</span>` : ''}
                        ${i.connected === false ? '<span class="topology-tag topology-tag-dim">none connected</span>' : ''}
                    </div>
                </div>`).join('');

        const outputCard = outputs.length === 0
            ? '<div class="topology-empty">No outputs registered</div>'
            : outputs.map(o => `
                <div class="topology-node" data-target="${o.target}">
                    <div class="topology-node-name">${o.name}</div>
                    <div class="topology-node-meta">
                        <span class="topology-tag">${o.target_name}</span>
                        <span class="topology-tag">${o.max_players} ${o.max_players === 1 ? 'player' : 'players'}</span>
                    </div>
                </div>`).join('');

        const routesList = routes.length === 0
            ? '<div class="hint">No routes registered.</div>'
            : `<ul class="topology-routes">${routes.map(r => `
                <li>
                    <span class="topology-pill">${r.input_name}</span>
                    <span class="topology-arrow">→</span>
                    <span class="topology-pill">${r.output_name}</span>
                    <span class="hint">priority ${r.priority}</span>
                </li>`).join('')}</ul>`;

        body.innerHTML = `
            <div class="topology-header">
                <span class="topology-tag topology-tag-mode">${modeLabel}</span>
                ${showMerge ? `<span class="topology-tag">strategy: ${mergeLabel}</span>` : ''}
                <span class="hint">${inputs.length} input${inputs.length === 1 ? '' : 's'} · ${outputs.length} output${outputs.length === 1 ? '' : 's'}</span>
            </div>
            <div class="topology-grid">
                <div class="topology-col">
                    <div class="topology-col-title">Inputs</div>
                    ${inputCard}
                </div>
                <div class="topology-divider">→</div>
                <div class="topology-col">
                    <div class="topology-col-title">Outputs</div>
                    ${outputCard}
                </div>
            </div>
            <div class="topology-routes-section">
                <div class="topology-col-title">Routes</div>
                ${routesList}
            </div>
        `;
    }

    async save() {
        if (!confirm('Save router configuration? The device will reboot.')) return;

        const config = {
            routing_mode: parseInt(this.el.querySelector('#routingMode').value),
            merge_mode: parseInt(this.el.querySelector('#mergeMode').value),
            dpad_mode: parseInt(this.el.querySelector('#dpadMode').value),
        };

        try {
            this.log('Saving router config...');
            const result = await this.protocol.setRouter(config);
            this.log(result.reboot ? 'Config saved. Device rebooting...' : 'Config saved.', 'success');
        } catch (e) {
            this.log(`Failed to save router config: ${e.message}`, 'error');
        }
    }

    async setDpadMode(mode) {
        try {
            await this.protocol.setDpadMode(parseInt(mode));
            const names = ['Normal', 'D-Pad ↔ Left Stick', 'D-Pad ↔ Right Stick', 'Left ↔ Right Stick'];
            this.log(`D-Pad / Stick swap: ${names[parseInt(mode)]}`, 'success');
        } catch (e) {
            this.log(`Failed to set D-Pad mode: ${e.message}`, 'error');
        }
    }
}
