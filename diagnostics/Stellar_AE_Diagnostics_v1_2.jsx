/* Stellar AE Diagnostics 1.2. Not an installer or release certificate.
 * Empty unsaved project only. Can prepare one WITH consent and native save prompt.
 * One fresh fixture per deferred task; no shared
 * cache/preferences changes. Existing app.onError is never replaced.
 * Scheduled tasks cannot interrupt a hung native call. See DIAGNOSTICS_V12.md.
 */
(function () {
    var KEY = "StellarDiagnostics11", global = $.global;
    if (global[KEY]) { alert("Stellar diagnostics already running. No second run started."); return; }
    var VERSION = "1.2", MATCH = "StellarLabs.StellarGradient";
    var started = new Date().getTime(), executionStarted = null;
    var runID = "Stellar-AE-" + started + "-" + Math.floor(Math.random() * 1000000000);
    var output = null, events = null, project = null, index = 0, task = null, finished = false;
    var definitions = [
        ["control_8",8,0], ["cpu_8",8,3], ["cpu_8_clean",8,3],
        ["control_16",16,0], ["cpu_16",16,3],
        ["control_32",32,0], ["cpu_32",32,3], ["auto_32",32,1]
    ];
    var report = {
        schema: 2, runner_version: VERSION, run_id: runID,
        candidate_commit: "deec78835801c5bf6f1aa44c772b508e43697b11",
        candidate_build_id: "sg-0.9.6-deec78835801-clean-873ad85d1063-aarch64-apple-darwin-36452443559.1",
        loaded_build_id: "NOT_VERIFIED", release_status: "NOT_APPROVED",
        checks: [], captures: [], cases: [], preflight: [], summary: "IN_PROGRESS",
        limitations: [
            "Registration version and target Build ID do not verify the loaded binary.",
            "PNG and DONE verify output creation, not visual parity or HDR precision.",
            "Requested CPU/Auto engine is recorded; actual execution path is NOT_VERIFIED.",
            "Deferred scheduling is an investigative change, not a proven fix of AE queue behavior.",
            "No Cosmic reference render, preset callbacks, migration or performance certification.",
            "180-second budget is between calls; native hangs/crashes cannot be preempted.",
            "Control and clean frames are separate fixtures, not substitutes for default-effect checks."
        ]
    };
    function quote(s) {
        return '"' + String(s).replace(/[\\"\u0000-\u001f]/g, function (c) {
            if (c === '"' || c === '\\') { return '\\' + c; }
            var h = c.charCodeAt(0).toString(16);
            return '\\u' + ('0000' + h).slice(-4);
        }) + '"';
    }
    function json(v) {
        if (v === null || typeof v === "undefined") { return "null"; }
        if (typeof v === "string") { return quote(v); }
        if (typeof v === "number") { return isFinite(v) ? String(v) : "null"; }
        if (typeof v === "boolean") { return v ? "true" : "false"; }
        var a = [], i, k;
        if (v instanceof Array) {
            for (i = 0; i < v.length; i++) { a.push(json(v[i])); }
            return "[" + a.join(",") + "]";
        }
        for (k in v) { if (Object.prototype.hasOwnProperty.call(v, k)) { a.push(quote(k) + ":" + json(v[k])); } }
        return "{" + a.join(",") + "}";
    }
    function fail(message, status) {
        var e = new Error(message); e.sgStatus = status || "FAIL"; throw e;
    }
    function errorText(e) {
        // Do not publish user/project paths from host exceptions.
        var s = String(e.message || e).replace(/\/(?:Users|home)\/[^\s\r\n]+/g, "[path]");
        return s.slice(0, 1000);
    }
    function write(file, text, mode) {
        file.encoding = "UTF-8";
        if (!file.open(mode || "w")) { fail("Report file is not writable. Check scripting file-write permission.", "BLOCKED"); }
        try { if (!file.write(text)) { fail("Report write failed.", "BLOCKED"); } }
        finally { file.close(); }
    }
    function record(name, status, detail) {
        var entry = {name: name, status: status, detail: detail || "", elapsed_ms: new Date().getTime() - started};
        report.checks.push(entry);
        if (events) { write(events, json(entry) + "\n", "a"); }
    }
    function budget() {
        if (new Date().getTime() - (executionStarted === null ? started : executionStarted) > 180000) { fail("180-second between-call budget exhausted.", "BLOCKED"); }
    }
    function snapshot(group, prefix) {
        var out = [], i, p, entry, path;
        for (i = 1; i <= group.numProperties; i++) {
            p = group.property(i); path = prefix.concat([i]);
            entry = {index_path: path, name: p.name, match_name: p.matchName, type: String(p.propertyType)};
            if (p.propertyType === PropertyType.PROPERTY) {
                entry.value_type = String(p.propertyValueType);
                if (p.propertyValueType !== PropertyValueType.NO_VALUE && p.propertyValueType !== PropertyValueType.CUSTOM_VALUE) {
                    entry.value = p.value;
                }
                entry.can_vary = p.canVaryOverTime;
                if (p.hasMin) { entry.min = p.minValue; }
                if (p.hasMax) { entry.max = p.maxValue; }
            } else { entry.children = snapshot(p, path); }
            out.push(entry);
        }
        return out;
    }
    function findOne(group, name) {
        var result = [], i, p;
        function visit(g) {
            for (var n = 1; n <= g.numProperties; n++) {
                var x = g.property(n);
                if (x.propertyType === PropertyType.PROPERTY && x.name === name) { result.push(x); }
                if (x.propertyType !== PropertyType.PROPERTY) { visit(x); }
            }
        }
        visit(group);
        if (result.length !== 1) { fail("Expected one control: " + name + "; found " + result.length, "BLOCKED"); }
        return result[0];
    }
    function pngInfo(file) {
        file.encoding = "BINARY";
        if (!file.open("r")) { fail("PNG is unreadable."); }
        var h;
        try { h = file.read(24); } finally { file.close(); }
        var signature = [137,80,78,71,13,10,26,10], i;
        if (h.length !== 24 || h.substr(12, 4) !== "IHDR") { fail("Invalid PNG header."); }
        for (i = 0; i < 8; i++) { if (h.charCodeAt(i) !== signature[i]) { fail("Invalid PNG signature."); } }
        function u32(p) { return h.charCodeAt(p)*16777216 + h.charCodeAt(p+1)*65536 + h.charCodeAt(p+2)*256 + h.charCodeAt(p+3); }
        if (u32(16) !== 128 || u32(20) !== 96) { fail("Unexpected PNG dimensions; possible output-template crop/resize."); }
        return {file: file.name, bytes: file.length, width: 128, height: 96};
    }
    function state(value) {
        var names = ["WILL_CONTINUE","NEEDS_OUTPUT","UNQUEUED","QUEUED","RENDERING","USER_STOPPED","ERR_STOPPED","DONE"];
        for (var i = 0; i < names.length; i++) {
            if (value == RQItemStatus[names[i]]) { return names[i]; }
        }
        return "UNKNOWN(" + String(value) + ")";
    }
    function checkpoint() {
        report.elapsed_ms = new Date().getTime() - started;
        write(new File(output.fsName + "/report.json"), json(report) + "\n");
    }
    function finish() {
        if (finished) { return; }
        finished = true;
        if (task !== null) { try { app.cancelTask(task); } catch (_) {} task = null; }
        var bad = false, complete = true, i;
        for (i = 0; i < report.cases.length; i++) {
            bad = bad || report.cases[i].status === "FAIL";
            complete = complete && report.cases[i].status === "PASS";
        }
        for (i = 0; i < report.checks.length; i++) {
            bad = bad || report.checks[i].status === "FAIL";
            complete = complete && report.checks[i].status === "PASS";
        }
        report.summary = bad ? "FAIL" : (complete && report.captures.length === definitions.length ? "SMOKE_ONLY_PASS" : "BLOCKED");
        delete global[KEY];
        try {
            checkpoint();
            var detail = "";
            for (i = report.checks.length - 1; i >= 0; i--) {
                if (report.checks[i].status !== "PASS") { detail = "\n" + report.checks[i].detail; break; }
            }
            alert("Stellar diagnostics " + VERSION + ": " + report.summary + detail + "\nDesktop folder: " + runID + "\nNot release approval.");
        } catch (e) {
            alert("Report could not be completed: " + errorText(e) + "\nNo installation or preferences were changed.");
        }
    }
    function settings(object, keys) {
        var result = {}, i;
        for (i = 0; i < keys.length; i++) {
            try { result[keys[i]] = errorText({message: String(object.getSetting(keys[i]))}); }
            catch (_) { result[keys[i]] = "NOT_AVAILABLE"; }
        }
        return result;
    }
    function projectState(stage) {
        // Read the current object once. Store only primitive metadata, no names/paths.
        var p = app.project, q = null, s = {
            stage: stage, project_exists: !!p, expected_project_matches: project ? p === project : null,
            file_is_null: null, item_count: null, queue_count: null, rendering: null,
            disable_rendering: app.disableRendering === true, read_errors: [], reasons: []
        };
        if (p) {
            try {
                var file = p.file;
                if (typeof file === "undefined") { s.read_errors.push("project.file unavailable"); }
                else { s.file_is_null = file === null; }
            } catch (_) { s.read_errors.push("project.file unreadable"); }
            try {
                var items = p.numItems;
                if (typeof items !== "number" || !isFinite(items) || items < 0 || Math.floor(items) !== items) {
                    s.read_errors.push("project.numItems invalid");
                } else { s.item_count = items; }
            } catch (_) { s.read_errors.push("project.numItems unreadable"); }
            try {
                q = p.renderQueue;
                var count = q.numItems, rendering = q.rendering;
                if (typeof count !== "number" || !isFinite(count) || count < 0 || Math.floor(count) !== count) {
                    s.read_errors.push("queue.numItems invalid");
                } else { s.queue_count = count; }
                if (typeof rendering !== "boolean") { s.read_errors.push("queue.rendering invalid"); }
                else { s.rendering = rendering; }
            } catch (_) { s.read_errors.push("render queue unreadable"); }
        }
        if (!s.project_exists) { s.reasons.push("NO_PROJECT"); }
        if (s.expected_project_matches === false) { s.reasons.push("PROJECT_CHANGED"); }
        if (s.file_is_null === false) { s.reasons.push("PROJECT_HAS_FILE"); }
        if (s.item_count !== null && s.item_count > 0) { s.reasons.push("PROJECT_ITEMS=" + s.item_count); }
        if (s.queue_count !== null && s.queue_count > 0) { s.reasons.push("QUEUE_ITEMS=" + s.queue_count); }
        if (s.rendering === true) { s.reasons.push("RENDER_ACTIVE_OR_PAUSED"); }
        if (s.disable_rendering) { s.reasons.push("RENDER_DISABLED"); }
        if (s.read_errors.length) { s.reasons.push("STATE_UNREADABLE: " + s.read_errors.join(", ")); }
        report.preflight.push(s);
        write(events, json({event:"project_preflight", state:s}) + "\n", "a");
        return s;
    }
    function preflightMessage(s) {
        return "Project check: " + s.reasons.join("; ") + ". No fixture was created.";
    }
    function requireEmpty(stage) {
        var s = projectState(stage || "before_case");
        if (s.reasons.length) { fail(preflightMessage(s), "BLOCKED"); }
    }
    function prepareProject() {
        var s = projectState("initial");
        if (!s.reasons.length) { return; }
        // Unknown state, active/paused render or disabled rendering must never be reset.
        if (s.read_errors.length || s.rendering === true || s.disable_rendering) {
            fail(preflightMessage(s), "BLOCKED");
        }
        checkpoint(); // Record observations BEFORE asking to change the open project.
        var preparationProject = app.project;
        var approved = confirm(
            "Stellar: нужен пустой несохранённый проект.\n" + s.reasons.join("; ") +
            "\n\nСоздать новый тестовый проект? Текущий проект закроется.\n" +
            "При несохранённых изменениях After Effects предложит сохранение. Выбери Save / Сохранить.\n" +
            "Отмена в окне сохранения остановит тест. Нет — оставить текущий проект без изменений.",
            true, "Stellar diagnostics");
        if (!approved) { fail("USER_CANCELLED_PREPARATION: current project left unchanged.", "BLOCKED"); }
        // Modal dialogs may take time; recheck render safety immediately before newProject.
        if (app.project !== preparationProject) {
            fail("PROJECT_CHANGED_DURING_CONFIRMATION: preparation cancelled, current project untouched.", "BLOCKED");
        }
        s = projectState("before_new_project");
        if (s.read_errors.length || s.rendering === true || s.disable_rendering) {
            fail(preflightMessage(s), "BLOCKED");
        }
        write(events, json({event:"new_project_requested", consent:true}) + "\n", "a");
        // Documented API prompts for unsaved work and returns null on cancellation.
        // Never call project.close(DO_NOT_SAVE_CHANGES), suppress dialogs or save to a guessed path.
        var created = app.newProject();
        if (!created) { fail("NEW_PROJECT_CANCELLED: no diagnostic fixture created.", "BLOCKED"); }
        requireEmpty("after_new_project"); // A configured startup template is NOT assumed empty.
        record("project_preparation", "PASS", "User-approved new empty unsaved project observed.");
    }
    function renderCase(c) {
        var comp = null, source = null, folder = null, queued = null, undo = false;
        var depth = null, oldError = null, ownsError = false, healthy = true, hookedStatus = false;
        var errorKey = KEY + "Error", statusKey = KEY + "Status";
        c.status = "RUNNING";
        try {
            budget(); requireEmpty();
            depth = project.bitsPerChannel;
            app.beginUndoGroup("Stellar diagnostic " + c.name); undo = true;
            project.bitsPerChannel = c.project_bpc;
            if (project.bitsPerChannel !== c.project_bpc) { fail("Requested project bit depth was not applied."); }
            comp = project.items.addComp(runID + "-" + c.name, 128, 96, 1, 1, 24);
            comp.resolutionFactor = [1,1];
            var layer = comp.layers.addSolid([1,1,1], "Diagnostic solid", 128, 96, 1, 1);
            source = layer.source;
            if (source.parentFolder !== project.rootFolder) { folder = source.parentFolder; }
            if (c.engine_requested !== 0) {
                var parade = layer.property("ADBE Effect Parade");
                if (!parade.canAddProperty(MATCH)) { fail("AE cannot apply Stellar Gradient."); }
                var effect = parade.addProperty(MATCH);
                if (c.name === "cpu_8") {
                    report.default_controls = snapshot(effect, []);
                    record("effect_initialization", "PASS", "Effect applied; defaults captured from installed version.");
                    var before = json(report.default_controls);
                    effect.remove(); effect = parade.addProperty(MATCH);
                    if (before !== json(snapshot(effect, []))) { fail("Repeated application changed defaults."); }
                    record("repeat_apply", "PASS", "Identical defaults on two applications.");
                }
                findOne(effect, "Render Engine").setValue(c.engine_requested);
                findOne(effect, "Quality").setValue(3);
                findOne(effect, "Animate").setValue(0);
                if (c.name === "cpu_8_clean") {
                    // Persistent match names, not ambiguous duplicated Amount labels.
                    var disabled = [15,19,30,34,40];
                    for (var d = 0; d < disabled.length; d++) {
                        var key = MATCH + "-" + ("0000" + disabled[d]).slice(-4);
                        var control = effect.property(key);
                        if (!control) { fail("Missing clean-fixture control: " + key, "BLOCKED"); }
                        control.setValue(0);
                    }
                    c.disabled_parameter_ids = disabled;
                }
                c.controls_at_render = snapshot(effect, []);
            }
            var dest = new Folder(output.fsName + "/" + c.name);
            if (dest.exists || !dest.create()) { fail("Capture directory must be new.", "BLOCKED"); }
            queued = project.renderQueue.items.add(comp);
            var om = queued.outputModule(1), templates = om.templates, selected = null;
            for (var n = 0; n < templates.length; n++) { if (/PNG/i.test(templates[n])) { selected = templates[n]; break; } }
            if (!selected) { fail("No existing PNG output template. No template was created or changed.", "BLOCKED"); }
            om.applyTemplate(selected); om = queued.outputModule(1);
            om.postRenderAction = PostRenderAction.NONE; om.includeSourceXMP = false;
            om.file = new File(dest.fsName + "/frame_[#####].png");
            queued.timeSpanStart = 0; queued.timeSpanDuration = comp.frameDuration;
            queued.skipFrames = 0; queued.render = true;
            c.output_template = selected;
            // Whitelist settings: never include Output File Info or user paths.
            c.output_settings = settings(om, ["Format","Channels","Depth","Color","Resize","Crop"]);
            c.render_settings = settings(queued, ["Quality","Resolution","Effects","Color Depth"]);
            c.before_status = state(queued.status); c.before_status_code = Number(queued.status);
            c.transitions = []; c.host_errors = [];
            c.disable_rendering = app.disableRendering === true;
            global[statusKey] = function () {
                try { if (c.transitions.length < 32) { c.transitions.push({name:state(queued.status), code:Number(queued.status)}); } } catch (_) {}
            };
            queued.onStatusChanged = statusKey; hookedStatus = true;
            oldError = app.onError;
            if (!oldError) {
                global[errorKey] = function (message, severity) {
                    try { if (c.host_errors.length < 16) { c.host_errors.push({message:errorText({message:message}), severity:String(severity)}); } } catch (_) {}
                };
                app.onError = errorKey; ownsError = true;
                c.error_capture = "temporary callback";
            } else { c.error_capture = "existing callback preserved; capture unavailable"; }
            checkpoint();
            write(events, json({event:"render_start", case_name:c.name, before_status:c.before_status}) + "\n", "a");
            var begin = new Date().getTime();
            try { project.renderQueue.render(); }
            finally {
                c.elapsed_ms = new Date().getTime() - begin;
                c.after_status = state(queued.status); c.after_status_code = Number(queued.status);
                try { c.ae_elapsed_seconds = queued.elapsedSeconds; c.start_time = queued.startTime ? String(queued.startTime) : null; } catch (_) {}
                write(events, json({event:"render_return", case_name:c.name, state:c.after_status, host_errors:c.host_errors, transitions:c.transitions}) + "\n", "a");
            }
            if (c.after_status !== "DONE") {
                if (c.after_status === "USER_STOPPED") { healthy = false; }
                if (c.after_status === "ERR_STOPPED") { healthy = false; fail("AE stopped render due to an error; see host_errors."); }
                fail("Render returned with " + c.after_status + "; no completed frame. This is not a plugin-failure diagnosis.", "BLOCKED");
            }
            var images = dest.getFiles("*.png");
            if (images.length !== 1) { fail("Expected exactly one fresh PNG, got " + images.length); }
            var image = pngInfo(images[0]);
            image.case_name = c.name; image.project_bpc = c.project_bpc;
            image.engine_requested = c.engine_requested; image.engine_actually_used = "NOT_VERIFIED";
            image.pixel_correctness = "NOT_RUN"; report.captures.push(image);
            if (c.host_errors.length) { healthy = false; fail("AE reported an error despite DONE; see host_errors."); }
            c.status = "PASS"; c.detail = "DONE and one valid 128x96 PNG; pixel correctness NOT_RUN.";
        } catch (e) {
            c.status = e.sgStatus || "FAIL"; c.detail = errorText(e);
            if (c.status === "FAIL") { healthy = false; }
        } finally {
            var errors = [];
            function attempt(label, fn) { try { fn(); } catch (e) { errors.push(label + ": " + errorText(e)); } }
            if (hookedStatus) { attempt("status callback", function () { queued.onStatusChanged = null; }); }
            if (ownsError) { attempt("error callback", function () { if (app.onError === errorKey) { app.onError = oldError; } }); }
            delete global[statusKey]; delete global[errorKey];
            var safe = app.project === project && !project.renderQueue.rendering;
            if (!safe) { errors.push("Project changed or render still active; owned objects left untouched."); }
            else {
                if (queued) { attempt("queue item", function () { queued.remove(); }); }
                if (comp) { attempt("composition", function () { comp.remove(); }); }
                if (source) { attempt("solid source", function () { source.remove(); }); }
                if (folder) { attempt("empty solid folder", function () { if (folder.numItems === 0) { folder.remove(); } }); }
                if (depth !== null) { attempt("bit depth", function () { project.bitsPerChannel = depth; }); }
            }
            if (undo) { attempt("undo group", function () { app.endUndoGroup(); }); }
            c.cleanup = errors.length ? "FAIL" : "PASS";
            if (errors.length) { c.cleanup_errors = errors; c.status = "FAIL"; healthy = false; }
        }
        record(c.name, c.status, c.detail); checkpoint();
        return healthy;
    }
    function next() {
        task = null;
        try {
            if (index >= report.cases.length) { finish(); return; }
            budget(); requireEmpty();
            var healthy = renderCase(report.cases[index]); index++;
            if (!healthy) { finish(); return; }
            checkpoint();
            // Return to AE's event loop before the next render. No undo group,
            // temporary handler or host fixture survives across callbacks.
            task = app.scheduleTask(KEY + ".next()", 250, false);
        } catch (e) {
            try { record("runner", e.sgStatus || "FAIL", errorText(e)); } catch (_) {}
            finish();
        }
    }
    try {
        for (var i = 0; i < definitions.length; i++) {
            report.cases.push({name:definitions[i][0], project_bpc:definitions[i][1],
                engine_requested:definitions[i][2], status:"NOT RUN"});
        }
        output = new Folder(Folder.desktop.fsName + "/" + runID);
        if (output.exists || !output.create()) { fail("Cannot create a new report folder.", "BLOCKED"); }
        events = new File(output.fsName + "/events.jsonl");
        write(events, json({event:"start", run_id:runID, runner_version:VERSION}) + "\n");
        report.environment = {ae_version:app.version, ae_build:app.buildNumber, os:$.os};
        report.rq_status_constants = {};
        var names = ["QUEUED","RENDERING","ERR_STOPPED","USER_STOPPED","DONE"];
        for (i = 0; i < names.length; i++) { report.rq_status_constants[names[i]] = Number(RQItemStatus[names[i]]); }
        report.installed_effects = []; var installed = false;
        var effects = app.effects;
        for (i = 0; i < effects.length; i++) {
            var e = effects[i];
            if (/stellar|cosmic/i.test(e.displayName + " " + e.matchName)) {
                report.installed_effects.push({name:e.displayName, match_name:e.matchName, version:e.version});
            }
            installed = installed || e.matchName === MATCH;
        }
        if (!installed) { fail("Stellar is not registered. No installation attempted.", "BLOCKED"); }
        record("effect_registered", "PASS", "Installed registration observed; runtime Build ID NOT_VERIFIED.");
        global[KEY] = {next:next}; // Reserve across preparation/save dialogs too.
        prepareProject(); project = app.project;
        report.project_settings = {bits_per_channel:project.bitsPerChannel, working_space:project.workingSpace, linear_blending:project.linearBlending};
        executionStarted = new Date().getTime(); // Do not charge save-dialog time to render budget.
        checkpoint();
        task = app.scheduleTask(KEY + ".next()", 250, false);
    } catch (e) {
        try { record("runner", e.sgStatus || "FAIL", errorText(e)); } catch (_) {}
        finish();
    }
})();
