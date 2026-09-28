/* Stellar / Cosmic default-reference capture 1.0.
 * Derived from diagnostics 1.2 (real-host eight-case smoke passed).
 * Four fresh fixtures: each installed effect at 128x96 and 512x288, frame 0,
 * 8-bpc project. All effect parameters remain at freshly applied defaults.
 * No plugin files/preferences/cache changed. Optional new project requires
 * consent and the native save prompt. Never certifies visual parity by itself.
 */
(function () {
    var KEY = "StellarDiagnostics11", global = $.global;
    if (global[KEY]) { alert("Stellar diagnostics already running. No second run started."); return; }
    var VERSION = "Compare 1.0", MATCH = "StellarLabs.StellarGradient", REFERENCE = "Loophouse Cosmic";
    var started = new Date().getTime(), executionStarted = null;
    var runID = "Stellar-Compare-" + started + "-" + Math.floor(Math.random() * 1000000000);
    var output = null, events = null, project = null, index = 0, task = null, finished = false;
    var definitions = [
        ["cosmic_128", REFERENCE, 128, 96], ["stellar_128", MATCH, 128, 96],
        ["cosmic_512", REFERENCE, 512, 288], ["stellar_512", MATCH, 512, 288]
    ];
    var report = {
        schema: 3, runner_version: VERSION, run_id: runID,
        candidate_commit: "deec78835801c5bf6f1aa44c772b508e43697b11",
        candidate_build_id: "sg-0.9.6-deec78835801-clean-873ad85d1063-aarch64-apple-darwin-36452443559.1",
        loaded_build_id: "NOT_VERIFIED", release_status: "NOT_APPROVED",
        checks: [], captures: [], cases: [], preflight: [], summary: "IN_PROGRESS",
        comparison_status: "NOT_EVALUATED",
        fixture_policy: "Fresh default effects; no parameter writes; white opaque solid; frame 0; 24 fps; 8-bpc project.",
        limitations: [
            "This compares out-of-box defaults, not mathematically matched settings or all presets.",
            "Registration and target Build ID do not verify the loaded binary.",
            "PNG capture completion is not visual parity, HDR or alpha certification.",
            "Engine and Quality remain at each effect's defaults; actual execution path NOT_VERIFIED.",
            "Default animated grain may differ by implementation; one frame is not a noise-quality or deterministic parity test.",
            "No presets selected: scripting is not assumed to invoke interactive preset callbacks.",
            "180-second budget is between calls; native hangs/crashes cannot be preempted.",
            "No install, migration, full-scale performance or release certification."
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
    function pngInfo(file, width, height) {
        file.encoding = "BINARY";
        if (!file.open("r")) { fail("PNG is unreadable."); }
        var h;
        try { h = file.read(24); } finally { file.close(); }
        var signature = [137,80,78,71,13,10,26,10], i;
        if (h.length !== 24 || h.substr(12, 4) !== "IHDR") { fail("Invalid PNG header."); }
        for (i = 0; i < 8; i++) { if (h.charCodeAt(i) !== signature[i]) { fail("Invalid PNG signature."); } }
        function u32(p) { return h.charCodeAt(p)*16777216 + h.charCodeAt(p+1)*65536 + h.charCodeAt(p+2)*256 + h.charCodeAt(p+3); }
        if (u32(16) !== width || u32(20) !== height) { fail("Unexpected PNG dimensions; possible output-template crop/resize."); }
        return {file: file.name, bytes: file.length, width: width, height: height};
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
        report.summary = bad ? "FAIL" : (complete && report.captures.length === definitions.length ? "CAPTURES_COMPLETE" : "BLOCKED");
        delete global[KEY];
        try {
            checkpoint();
            var detail = "";
            for (i = report.checks.length - 1; i >= 0; i--) {
                if (report.checks[i].status !== "PASS") { detail = "\n" + report.checks[i].detail; break; }
            }
            alert("Stellar diagnostics " + VERSION + ": " + report.summary + detail + "\nDesktop folder: " + runID + "\nCaptures only; comparison not yet evaluated.");
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
            comp = project.items.addComp(runID + "-" + c.name, c.width, c.height, 1, 1, 24);
            comp.resolutionFactor = [1,1];
            var layer = comp.layers.addSolid([1,1,1], "Diagnostic solid", c.width, c.height, 1, 1);
            source = layer.source;
            if (source.parentFolder !== project.rootFolder) { folder = source.parentFolder; }
            var parade = layer.property("ADBE Effect Parade");
            if (!parade.canAddProperty(c.effect_match)) { fail("AE cannot apply " + c.effect_match); }
            var effect = parade.addProperty(c.effect_match);
            // Do not set any control, menu, engine or quality. We need the
            // actual installed default state, not presumed Cosmic parameter IDs.
            c.controls_at_render = snapshot(effect, []);
            c.parameter_writes = 0;
            c.frame_time_seconds = 0;
            c.effect_identity_status = "REGISTRATION_ONLY";
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
            var image = pngInfo(images[0], c.width, c.height);
            image.case_name = c.name; image.project_bpc = c.project_bpc;
            image.engine_requested = "DEFAULT_UNCHANGED"; image.effect_match = c.effect_match; image.engine_actually_used = "NOT_VERIFIED";
            image.pixel_correctness = "NOT_RUN"; report.captures.push(image);
            if (c.host_errors.length) { healthy = false; fail("AE reported an error despite DONE; see host_errors."); }
            c.status = "PASS"; c.detail = "DONE and one valid " + c.width + "x" + c.height + " PNG; comparison NOT_EVALUATED.";
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
            report.cases.push({name:definitions[i][0], effect_match:definitions[i][1],
                width:definitions[i][2], height:definitions[i][3], project_bpc:8,
                engine_requested:"DEFAULT_UNCHANGED", status:"NOT RUN"});
        }
        output = new Folder(Folder.desktop.fsName + "/" + runID);
        if (output.exists || !output.create()) { fail("Cannot create a new report folder.", "BLOCKED"); }
        events = new File(output.fsName + "/events.jsonl");
        write(events, json({event:"start", run_id:runID, runner_version:VERSION}) + "\n");
        report.environment = {ae_version:app.version, ae_build:app.buildNumber, os:$.os};
        report.rq_status_constants = {};
        var names = ["QUEUED","RENDERING","ERR_STOPPED","USER_STOPPED","DONE"];
        for (i = 0; i < names.length; i++) { report.rq_status_constants[names[i]] = Number(RQItemStatus[names[i]]); }
        report.installed_effects = []; var installed = false, referenceInstalled = false;
        var effects = app.effects;
        for (i = 0; i < effects.length; i++) {
            var e = effects[i];
            if (/stellar|cosmic/i.test(e.displayName + " " + e.matchName)) {
                report.installed_effects.push({name:e.displayName, match_name:e.matchName, version:e.version});
            }
            installed = installed || e.matchName === MATCH;
            referenceInstalled = referenceInstalled || e.matchName === REFERENCE;
        }
        if (!installed || !referenceInstalled) { fail("Both Stellar and Cosmic must be registered. No installation attempted.", "BLOCKED"); }
        record("effect_registered", "PASS", "Stellar and Cosmic registration observed; loaded identities NOT_VERIFIED.");
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
