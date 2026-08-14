/** Profile Management Card + Editor Modal */

// Index === JP_BUTTON_* bit position. Must stay in sync with core/buttons.h and
// the firmware's positional custom_profile_t.button_map (CUSTOM_PROFILE_BUTTON_COUNT).
const BUTTON_NAMES = [
    'B1', 'B2', 'B3', 'B4', 'L1', 'R1', 'L2', 'R2',
    'S1', 'S2', 'L3', 'R3', 'DU', 'DD', 'DL', 'DR',
    'A1', 'A2', 'A3', 'A4', 'L4', 'R4', 'F1', 'F2',
    'L5', 'R5'
];

const BUTTON_LABELS = {
    'B1': 'A / Cross', 'B2': 'B / Circle', 'B3': 'X / Square', 'B4': 'Y / Triangle',
    'L1': 'L1 / LB', 'R1': 'R1 / RB', 'L2': 'L2 / LT', 'R2': 'R2 / RT',
    'S1': 'Select / Back', 'S2': 'Start / Menu', 'L3': 'L3 / LS', 'R3': 'R3 / RS',
    'DU': 'D-Pad Up', 'DD': 'D-Pad Down', 'DL': 'D-Pad Left', 'DR': 'D-Pad Right',
    'A1': 'Home / Guide', 'A2': 'Capture / Touchpad',
    'A3': 'Aux 3', 'A4': 'Aux 4', 'L4': 'Left Paddle 1', 'R4': 'Right Paddle 1',
    'F1': 'Function 1', 'F2': 'Function 2',
    'L5': 'Left Paddle 2', 'R5': 'Right Paddle 2',
};

// F1/F2 are internal function buttons — not offered as remap inputs.
const NON_REMAPPABLE_INPUTS = new Set(['F1', 'F2']);
// Bit positions offered as remap inputs, in display order (skips F1/F2).
const REMAP_INPUT_BITS = BUTTON_NAMES
    .map((_, bit) => bit)
    .filter((bit) => !NON_REMAPPABLE_INPUTS.has(BUTTON_NAMES[bit]));
// Positional map length sent to firmware (must equal CUSTOM_PROFILE_BUTTON_COUNT).
const BUTTON_MAP_LEN = BUTTON_NAMES.length;
const REMAPPABLE_COUNT = REMAP_INPUT_BITS.length;  // exported for compat
const FLAG_SWAP_STICKS = 1;
const FLAG_INVERT_LY = 2;
const FLAG_INVERT_RY = 4;

export class ProfilesCard {
    constructor(container, protocol, log) {
        this.protocol = protocol;
        this.log = log;
        this.el = container;
        this.profiles = [];
        this.activeIndex = 0;
        this.editingIndex = null;
    }

    render() {
        const mapOptions = `<option value="0">Passthrough</option>` +
            BUTTON_NAMES.map((name, idx) =>
                `<option value="${idx + 1}">${name} (${BUTTON_LABELS[name] || name})</option>`
            ).join('') +
            `<option value="255">Disabled</option>`;

        const mapRows = REMAP_INPUT_BITS.map((bit) => {
            const name = BUTTON_NAMES[bit];
            return `<div class="button-map-row">
                <span class="input-label">${name}</span>
                <select id="buttonMap${bit}">${mapOptions}</select>
                <label class="turbo-cell" title="Turbo — auto-fire while held (uses the profile's Turbo Rate)">
                    <input type="checkbox" class="turbo-check" id="turbo${bit}">
                    <span class="turbo-icon">⚡</span>
                </label>
            </div>`;
        }).join('');

        this.el.innerHTML = `
            <div class="card" id="profilesCard">
                <h2>Profiles</h2>
                <div class="card-content">
                    <div class="row">
                        <span class="label">Manage Profiles</span>
                        <button id="newProfileBtn" class="secondary">+ New Profile</button>
                    </div>
                    <div id="profileList" class="profile-list"></div>
                    <p class="hint" style="margin-top: 12px;">
                        <a href="https://docs.joypad.ai/core/buttons/" target="_blank" style="color: var(--text-muted);">Button reference</a>
                         · <a href="https://docs.joypad.ai/core/profiles/" target="_blank" style="color: var(--text-muted);">Profile docs</a>
                    </p>
                </div>
            </div>

            <div id="profileEditorModal" class="modal hidden">
                <div class="modal-content">
                    <div class="modal-header">
                        <h3 id="profileEditorTitle">Edit Profile</h3>
                        <button class="modal-close" id="closeEditorBtn">&times;</button>
                    </div>
                    <div class="modal-body">
                        <div class="form-group">
                            <label>Profile Name</label>
                            <input type="text" id="profileNameInput" maxlength="11" placeholder="Profile name">
                        </div>
                        <div class="form-group" id="outputModeGroup" style="display:none;">
                            <label id="outputModeLabel">Device Mode</label>
                            <p class="hint" style="margin-bottom: 10px;" id="outputModeHint"></p>
                            <select id="outputModeSelect" style="width: 100%;"></select>
                        </div>
                        <div class="form-group">
                            <label>Button Mapping</label>
                            <p class="hint" style="margin-bottom: 10px;">For each input button, select what output button it should produce.</p>
                            <div id="buttonMapContainer" class="button-map-grid">${mapRows}</div>
                        </div>
                        <div class="form-group">
                            <label>Stick Sensitivity</label>
                            <div class="sensitivity-row">
                                <span>Left Stick</span>
                                <input type="range" id="leftStickSens" min="0" max="200" value="100">
                                <span id="leftStickSensValue">100%</span>
                            </div>
                            <div class="sensitivity-row">
                                <span>Right Stick</span>
                                <input type="range" id="rightStickSens" min="0" max="200" value="100">
                                <span id="rightStickSensValue">100%</span>
                            </div>
                        </div>
                        <div class="form-group">
                            <label>SOCD Cleaning</label>
                            <p class="hint" style="margin-bottom: 10px;">How to handle simultaneous opposite cardinal directions.</p>
                            <select id="socdModeSelect" style="width: 100%;">
                                <option value="0">Passthrough (no cleaning)</option>
                                <option value="1">Neutral (cancel both directions)</option>
                                <option value="2">Up Priority (U+D=U, L+R=neutral)</option>
                                <option value="3">Last Win (last input takes priority)</option>
                            </select>
                        </div>
                        <div class="form-group">
                            <label>Turbo Rate</label>
                            <p class="hint" style="margin-bottom: 10px;">Auto-fire speed for buttons marked ⚡. One rate for the whole profile.</p>
                            <select id="autofireRate" style="width: 100%;">
                                <option value="0">Off</option>
                                <option value="1">30 Hz</option>
                                <option value="2">20 Hz</option>
                                <option value="3">15 Hz</option>
                                <option value="4">12 Hz</option>
                                <option value="5">10 Hz</option>
                                <option value="6">7.5 Hz</option>
                            </select>
                        </div>
                        <div class="form-group">
                            <label>Options</label>
                            <div class="checkbox-row"><input type="checkbox" id="flagSwapSticks"><label for="flagSwapSticks">Swap Left/Right Sticks</label></div>
                            <div class="checkbox-row"><input type="checkbox" id="flagInvertLY"><label for="flagInvertLY">Invert Left Y Axis</label></div>
                            <div class="checkbox-row"><input type="checkbox" id="flagInvertRY"><label for="flagInvertRY">Invert Right Y Axis</label></div>
                        </div>
                    </div>
                    <div class="modal-footer">
                        <button id="deleteProfileBtn" class="secondary danger">Delete</button>
                        <div style="flex: 1;"></div>
                        <button id="cancelEditorBtn" class="secondary">Cancel</button>
                        <button id="saveProfileBtn">Save Profile</button>
                    </div>
                </div>
            </div>`;

        // Events
        this.el.querySelector('#newProfileBtn').addEventListener('click', () => this.openEditor(null));
        this.el.querySelector('#closeEditorBtn').addEventListener('click', () => this.closeEditor());
        this.el.querySelector('#cancelEditorBtn').addEventListener('click', () => this.closeEditor());
        this.el.querySelector('#saveProfileBtn').addEventListener('click', () => this.save());
        this.el.querySelector('#deleteProfileBtn').addEventListener('click', () => this.delete());

        // Enable/disable the Turbo Rate select as ⚡ toggles change.
        this.el.querySelector('#buttonMapContainer').addEventListener('change', (e) => {
            if (e.target.classList.contains('turbo-check')) this.updateTurboState();
        });

        this.el.querySelector('#leftStickSens').addEventListener('input', (e) => {
            this.el.querySelector('#leftStickSensValue').textContent = e.target.value + '%';
        });
        this.el.querySelector('#rightStickSens').addEventListener('input', (e) => {
            this.el.querySelector('#rightStickSensValue').textContent = e.target.value + '%';
        });
    }

    async load() {
        try {
            const result = await this.protocol.listProfiles();
            this.profiles = result.profiles || [];
            this.activeIndex = result.active || 0;
            this.renderList();

            const builtinCount = this.profiles.filter(p => p.builtin).length;
            const customCount = this.profiles.filter(p => !p.builtin).length;
            this.log(`Loaded ${builtinCount} built-in + ${customCount} custom profiles, active: ${result.active}`);
        } catch (e) {
            this.log(`Failed to load profiles: ${e.message}`, 'error');
        }

        // App-declared device/output modes (optional — older firmware won't have it).
        try {
            const m = await this.protocol.getProfileModes();
            this.outputModes = (m && m.ok && Array.isArray(m.modes)) ? m : null;
        } catch (e) {
            this.outputModes = null;
        }
        this.populateOutputModes();
    }

    // Fill the Device Mode dropdown from the app's mode list; hide it if none.
    populateOutputModes() {
        const group = this.el.querySelector('#outputModeGroup');
        const sel = this.el.querySelector('#outputModeSelect');
        if (!group || !sel) return;
        const modes = this.outputModes && this.outputModes.modes;
        if (!modes || modes.length === 0) {
            group.style.display = 'none';
            return;
        }
        const type = this.outputModes.type || 'Device';
        this.el.querySelector('#outputModeLabel').textContent = `${type} Mode`;
        this.el.querySelector('#outputModeHint').textContent =
            `Output format sent to the ${type}. Applies whenever this profile is active.`;
        sel.innerHTML = modes.map((name, i) => `<option value="${i}">${name}</option>`).join('');
        group.style.display = '';
    }

    renderList() {
        const list = this.el.querySelector('#profileList');
        list.innerHTML = '';

        for (const profile of this.profiles) {
            const isActive = profile.index === this.activeIndex;
            const item = document.createElement('div');
            item.className = 'profile-item' + (isActive ? ' active' : '');

            if (profile.disabled) item.classList.add('profile-disabled');

            const info = document.createElement('div');
            info.className = 'profile-item-info';
            let detail = profile.builtin ? (profile.index === 0 ? 'Passthrough' : 'Built-in') : 'Custom';
            if (profile.disabled) detail += ' · hidden from hotkey';
            info.innerHTML = `<div class="profile-item-name">${profile.name}</div>
                              <div class="profile-item-details">${detail}</div>`;
            item.appendChild(info);

            const actions = document.createElement('div');
            actions.className = 'profile-item-actions';

            if (!isActive) {
                const selectBtn = document.createElement('button');
                selectBtn.className = 'secondary';
                selectBtn.textContent = 'Select';
                selectBtn.addEventListener('click', () => this.select(profile.index));
                actions.appendChild(selectBtn);
            }

            if (profile.builtin) {
                const cloneBtn = document.createElement('button');
                cloneBtn.className = 'secondary';
                cloneBtn.textContent = 'Clone';
                cloneBtn.addEventListener('click', () => this.clone(profile.index, profile.name));
                actions.appendChild(cloneBtn);

                // Enable/Disable in the hotkey cycle (built-ins only).
                const disBtn = document.createElement('button');
                disBtn.className = 'secondary';
                disBtn.textContent = profile.disabled ? 'Enable' : 'Disable';
                disBtn.title = profile.disabled
                    ? 'Show this profile in the SELECT + D-pad hotkey cycle'
                    : 'Hide this profile from the SELECT + D-pad hotkey cycle';
                disBtn.addEventListener('click', () => this.setDisabled(profile.index, !profile.disabled));
                actions.appendChild(disBtn);
            }

            if (profile.editable) {
                const editBtn = document.createElement('button');
                editBtn.className = 'secondary';
                editBtn.textContent = 'Edit';
                editBtn.addEventListener('click', () => this.openEditor(profile.index));
                actions.appendChild(editBtn);
            }

            item.appendChild(actions);
            list.appendChild(item);
        }

        // Update new profile button
        const newBtn = this.el.querySelector('#newProfileBtn');
        const customCount = this.profiles.filter(p => !p.builtin).length;
        newBtn.disabled = customCount >= 4;
        newBtn.textContent = customCount >= 4 ? 'Max Profiles (4)' : '+ New Profile';
    }

    async select(index) {
        try {
            this.log(`Selecting profile ${index}...`);
            const result = await this.protocol.setProfile(parseInt(index));
            this.activeIndex = index;
            this.renderList();
            this.log(`Profile set to ${result.name}`, 'success');
        } catch (e) {
            this.log(`Failed to select profile: ${e.message}`, 'error');
        }
    }

    async clone(index, originalName) {
        const cloneName = (originalName + ' Copy').substring(0, 11);
        try {
            this.log(`Cloning profile "${originalName}"...`);
            const result = await this.protocol.cloneProfile(index, cloneName);
            this.log(`Profile cloned as "${result.name}"`, 'success');
            await this.load();
        } catch (e) {
            this.log(`Failed to clone profile: ${e.message}`, 'error');
        }
    }

    async setDisabled(index, disabled) {
        try {
            await this.protocol.disableProfile(index, disabled);
            this.log(`Profile ${disabled ? 'hidden from' : 'shown in'} the hotkey cycle`, 'success');
            await this.load();
        } catch (e) {
            this.log(`Failed to update profile: ${e.message}`, 'error');
        }
    }

    async openEditor(index) {
        this.editingIndex = index;
        const isNew = index === null;
        const modal = this.el.querySelector('#profileEditorModal');

        this.el.querySelector('#profileEditorTitle').textContent = isNew ? 'New Profile' : 'Edit Profile';
        this.el.querySelector('#deleteProfileBtn').classList.toggle('hidden', isNew);

        if (isNew) {
            this.el.querySelector('#profileNameInput').value = '';
            for (const bit of REMAP_INPUT_BITS) {
                this.el.querySelector(`#buttonMap${bit}`).value = '0';
                this.el.querySelector(`#turbo${bit}`).checked = false;
            }
            this.el.querySelector('#leftStickSens').value = 100;
            this.el.querySelector('#rightStickSens').value = 100;
            this.el.querySelector('#leftStickSensValue').textContent = '100%';
            this.el.querySelector('#rightStickSensValue').textContent = '100%';
            this.el.querySelector('#socdModeSelect').value = '0';
            this.el.querySelector('#autofireRate').value = '0';
            this.el.querySelector('#outputModeSelect').value = '0';
            this.el.querySelector('#flagSwapSticks').checked = false;
            this.el.querySelector('#flagInvertLY').checked = false;
            this.el.querySelector('#flagInvertRY').checked = false;
        } else {
            try {
                const profile = await this.protocol.getProfile(index);
                this.el.querySelector('#profileNameInput').value = profile.name || '';
                const map = profile.button_map || [];
                const turbo = profile.turbo_mask || 0;
                for (const bit of REMAP_INPUT_BITS) {
                    this.el.querySelector(`#buttonMap${bit}`).value = map[bit] !== undefined ? map[bit] : 0;
                    this.el.querySelector(`#turbo${bit}`).checked = ((turbo >> bit) & 1) !== 0;
                }
                const ls = this.el.querySelector('#leftStickSens');
                const rs = this.el.querySelector('#rightStickSens');
                ls.value = profile.left_stick_sens || 100;
                rs.value = profile.right_stick_sens || 100;
                this.el.querySelector('#leftStickSensValue').textContent = ls.value + '%';
                this.el.querySelector('#rightStickSensValue').textContent = rs.value + '%';
                this.el.querySelector('#socdModeSelect').value = (profile.socd_mode || 0).toString();
                this.el.querySelector('#autofireRate').value = (profile.autofire_rate || 0).toString();
                this.el.querySelector('#outputModeSelect').value = (profile.output_mode || 0).toString();
                const flags = profile.flags || 0;
                this.el.querySelector('#flagSwapSticks').checked = (flags & FLAG_SWAP_STICKS) !== 0;
                this.el.querySelector('#flagInvertLY').checked = (flags & FLAG_INVERT_LY) !== 0;
                this.el.querySelector('#flagInvertRY').checked = (flags & FLAG_INVERT_RY) !== 0;
            } catch (e) {
                this.log(`Failed to load profile: ${e.message}`, 'error');
                return;
            }
        }

        this.updateTurboState();
        modal.classList.remove('hidden');
    }

    // Grey out the Turbo Rate select when no ⚡ is set; when turbo is first
    // enabled with the rate still Off, pick a sensible default so it does something.
    updateTurboState() {
        const anyTurbo = REMAP_INPUT_BITS.some(
            (bit) => this.el.querySelector(`#turbo${bit}`).checked);
        const rate = this.el.querySelector('#autofireRate');
        rate.disabled = !anyTurbo;
        if (anyTurbo && rate.value === '0') rate.value = '3';  // 15 Hz default
    }

    closeEditor() {
        this.el.querySelector('#profileEditorModal').classList.add('hidden');
        this.editingIndex = null;
    }

    async save() {
        const name = this.el.querySelector('#profileNameInput').value.trim();
        if (!name) { alert('Please enter a profile name'); return; }

        // Positional array indexed by input bit (length CUSTOM_PROFILE_BUTTON_COUNT).
        // F1/F2 have no row — always passthrough (0) to keep bit alignment.
        const buttonMap = [];
        for (let bit = 0; bit < BUTTON_MAP_LEN; bit++) {
            if (NON_REMAPPABLE_INPUTS.has(BUTTON_NAMES[bit])) {
                buttonMap.push(0);
            } else {
                buttonMap.push(parseInt(this.el.querySelector(`#buttonMap${bit}`).value));
            }
        }

        let flags = 0;
        if (this.el.querySelector('#flagSwapSticks').checked) flags |= FLAG_SWAP_STICKS;
        if (this.el.querySelector('#flagInvertLY').checked) flags |= FLAG_INVERT_LY;
        if (this.el.querySelector('#flagInvertRY').checked) flags |= FLAG_INVERT_RY;

        // Turbo: bitmask over physical input bits, plus the shared rate index.
        let turboMask = 0;
        for (const bit of REMAP_INPUT_BITS) {
            if (this.el.querySelector(`#turbo${bit}`).checked) turboMask |= (1 << bit);
        }
        const autofireRate = turboMask ? parseInt(this.el.querySelector('#autofireRate').value) || 0 : 0;

        const data = {
            name,
            button_map: buttonMap,
            left_stick_sens: parseInt(this.el.querySelector('#leftStickSens').value),
            right_stick_sens: parseInt(this.el.querySelector('#rightStickSens').value),
            flags,
            socd_mode: parseInt(this.el.querySelector('#socdModeSelect').value),
            autofire_rate: autofireRate,
            turbo_mask: turboMask,
        };

        // Include device mode only when the app exposes modes.
        if (this.outputModes && this.outputModes.modes && this.outputModes.modes.length) {
            data.output_mode = parseInt(this.el.querySelector('#outputModeSelect').value) || 0;
        }

        const index = this.editingIndex === null ? 255 : this.editingIndex;

        try {
            this.log('Saving profile...');
            const result = await this.protocol.saveProfile(index, data);
            this.log(`Profile "${result.name}" saved`, 'success');
            this.closeEditor();
            await this.load();
        } catch (e) {
            this.log(`Failed to save profile: ${e.message}`, 'error');
        }
    }

    async delete() {
        if (this.editingIndex === null) return;
        const profile = this.profiles.find(p => p.index === this.editingIndex);
        if (profile && profile.builtin) { alert('Cannot delete built-in profiles'); return; }
        if (!confirm('Delete this profile?')) return;

        try {
            this.log(`Deleting profile ${this.editingIndex}...`);
            await this.protocol.deleteProfile(this.editingIndex);
            this.log('Profile deleted', 'success');
            this.closeEditor();
            await this.load();
        } catch (e) {
            this.log(`Failed to delete profile: ${e.message}`, 'error');
        }
    }
}

export { BUTTON_NAMES, BUTTON_LABELS, REMAPPABLE_COUNT };
