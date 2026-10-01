/* Stellar AE Diagnostics 1.0 — diagnostic tool, NOT a plugin installer.
 * Run only with an empty, unsaved project. Does not install/replace plugins,
 * save/close projects, clear caches, change preferences, or send network data.
 * Report + four small PNG frames go into a NEW Desktop/Stellar-AE-* folder.
 * An AE/plugin crash cannot be caught by JavaScript; partial events are retained.
 */
(function () {
    var VERSION = "1.0";
    var MATCH = "StellarLabs.StellarGradient";
    var started = new Date().getTime();
    var runID = "Stellar-AE-" + started + "-" + Math.floor(Math.random() * 1000000000);
    var output = null, events = null, comp = null, source = null, solidFolder = null;
    var queued = null, undoOpen = false, oldDepth = null, touched = false;
    var report = {
        schema: 1, runner_version: VERSION, run_id: runID,
        candidate_commit: "deec78835801c5bf6f1aa44c772b508e43697b11",
        candidate_build_id: "sg-0.9.6-deec78835801-clean-873ad85d1063-aarch64-apple-darwin-36452443559.1",
        loaded_build_id: "NOT_VERIFIED", checks: [], captures: [],
        release_status: "NOT_APPROVED",
        limitations: [
            "Tests the currently installed effect, not necessarily the candidate above.",
            "Scripting version/inventory is NOT proof of the actually loaded Build ID.",
            "PNG output does not verify HDR precision, alpha correctness or numerical CPU/GPU parity.",
            "Auto engine can fall back to CPU; a successful frame is not proof of GPU execution.",
            "No UI preset callbacks, Undo/Redo, save/reopen, restart, migration or profiling certification.",
            "Time budget is checked BETWEEN host calls; JavaScript cannot preempt a hung native callback."
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
        if (new Date().getTime() - started > 120000) { fail("120-second between-call budget exhausted.", "BLOCKED"); }
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
    function capture(effect, label, depth, engine) {
        budget();
        var folder = new Folder(output.fsName + "/" + label);
        if (folder.exists || !folder.create()) { fail("Capture destination must be new.", "BLOCKED"); }
        app.project.bitsPerChannel = depth;
        if (app.project.bitsPerChannel !== depth) { fail("AE did not accept requested bit depth."); }
        findOne(effect, "Render Engine").setValue(engine);
        findOne(effect, "Quality").setValue(3);
        findOne(effect, "Animate").setValue(0);
        queued = app.project.renderQueue.items.add(comp);
        try {
            var om = queued.outputModule(1), templates = om.templates, chosen = null, i;
            for (i = 0; i < templates.length; i++) { if (/PNG/i.test(templates[i])) { chosen = templates[i]; break; } }
            if (!chosen) { fail("No existing PNG output template. No template was created or changed.", "BLOCKED"); }
            om.applyTemplate(chosen);
            om = queued.outputModule(1); // AE can invalidate OutputModule after settings changes.
            om.postRenderAction = PostRenderAction.NONE;
            om.includeSourceXMP = false;
            om.file = new File(folder.fsName + "/frame_[#####].png");
            queued.timeSpanStart = 0;
            queued.timeSpanDuration = comp.frameDuration;
            queued.skipFrames = 0;
            queued.render = true;
            var begin = new Date().getTime();
            write(events, json({event: "render_start", label: label, run_id: runID}) + "\n", "a");
            app.project.renderQueue.render();
            if (queued.status !== RQItemStatus.DONE) { fail("Render did not finish: " + String(queued.status)); }
            var images = folder.getFiles("*.png");
            if (images.length !== 1) { fail("Expected exactly one new PNG; found " + images.length); }
            var image = pngInfo(images[0]);
            image.case_name = label; image.project_bpc = depth; image.engine_requested = engine;
            image.engine_actually_used = "NOT_VERIFIED";
            image.output_template = chosen; image.elapsed_ms = new Date().getTime() - begin;
            image.pixel_correctness = "NOT_RUN";
            report.captures.push(image);
            record(label, "PASS", "AE Render Queue DONE; one valid 128x96 PNG. Pixel correctness not certified.");
        } finally {
            if (queued) { queued.remove(); queued = null; }
        }
    }
    function cleanup() {
        var errors = [];
        function attempt(name, fn) { try { fn(); } catch (e) { errors.push(name + ": " + errorText(e)); } }
        if (queued) { attempt("owned queue item", function () { queued.remove(); queued = null; }); }
        if (comp) { attempt("owned comp", function () { comp.remove(); comp = null; }); }
        if (source) { attempt("owned solid source", function () { source.remove(); source = null; }); }
        if (solidFolder) { attempt("owned empty solid folder", function () { if (solidFolder.numItems === 0) { solidFolder.remove(); } }); }
        if (oldDepth !== null) { attempt("restore bit depth", function () { app.project.bitsPerChannel = oldDepth; }); }
        if (undoOpen) { attempt("close undo group", function () { app.endUndoGroup(); undoOpen = false; }); }
        if (touched && (app.project.numItems !== 0 || app.project.renderQueue.numItems !== 0)) { errors.push("Owned fixture cleanup was incomplete; no broad deletion attempted."); }
        if (touched) { record("cleanup", errors.length ? "FAIL" : "PASS", errors.join("; ") || "Only owned fixture removed; original bit depth restored."); }
    }
    try {
        output = new Folder(Folder.desktop.fsName + "/" + runID);
        if (output.exists || !output.create()) { fail("Cannot create a fresh Desktop report folder.", "BLOCKED"); }
        events = new File(output.fsName + "/events.jsonl");
        write(events, json({run_id: runID, runner_version: VERSION, event: "start"}) + "\n");
        report.environment = {ae_version: app.version, ae_build: app.buildNumber, os: $.os};
        report.installed_effects = [];
        var installed = null, i, e;
        for (i = 0; i < app.effects.length; i++) {
            e = app.effects[i];
            if (/stellar|cosmic/i.test(e.displayName + " " + e.matchName)) {
                report.installed_effects.push({name: e.displayName, match_name: e.matchName, version: e.version});
            }
            if (e.matchName === MATCH) { installed = e; }
        }
        if (!app.project || app.project.numItems !== 0 || app.project.file !== null || app.project.renderQueue.numItems !== 0 || app.project.renderQueue.rendering) {
            fail("Use an EMPTY, UNSAVED project with an empty render queue. Existing project was not modified.", "BLOCKED");
        }
        if (!installed) { fail("StellarLabs.StellarGradient is not registered in this AE session. No installation attempted.", "BLOCKED"); }
        record("effect_registered", "PASS", "Reported internal version: " + installed.version + ". Loaded Build ID remains unverified.");
        report.project_settings = {bits_per_channel: app.project.bitsPerChannel, working_space: app.project.workingSpace, linear_blending: app.project.linearBlending};
        oldDepth = app.project.bitsPerChannel;
        app.beginUndoGroup("Stellar isolated diagnostics"); undoOpen = true; touched = true;
        comp = app.project.items.addComp(runID, 128, 96, 1, 1, 24);
        comp.resolutionFactor = [1, 1];
        var layer = comp.layers.addSolid([1,1,1], "Diagnostic solid", 128, 96, 1, 1);
        source = layer.source;
        if (source.parentFolder !== app.project.rootFolder) { solidFolder = source.parentFolder; }
        var parade = layer.property("ADBE Effect Parade"), effect;
        if (!parade.canAddProperty(MATCH)) { fail("AE cannot apply Stellar Gradient (PARAMS_SETUP may have failed)."); }
        effect = parade.addProperty(MATCH);
        report.default_controls = snapshot(effect, []);
        record("effect_initialization", "PASS", "Effect added; readable parameter tree. This tests initialization on this installed version only.");
        var before = json(report.default_controls);
        effect.remove(); effect = parade.addProperty(MATCH);
        if (json(snapshot(effect, [])) !== before) { fail("Default parameter tree changed on second application."); }
        record("repeat_apply", "PASS", "Two applications expose identical default parameter trees.");
        var cases = [["cpu_8",8,3],["cpu_16",16,3],["cpu_32",32,3],["auto_32",32,1]];
        for (i = 0; i < cases.length; i++) {
            try { capture(effect, cases[i][0], cases[i][1], cases[i][2]); }
            catch (renderError) { record(cases[i][0], renderError.sgStatus || "FAIL", errorText(renderError)); break; }
        }
    } catch (error) {
        try { record("runner", error.sgStatus || "FAIL", errorText(error)); }
        catch (writeError) { report.checks.push({name: "report_write", status: "BLOCKED", detail: errorText(writeError)}); }
    } finally {
        try { cleanup(); } catch (cleanupError) { report.checks.push({name: "cleanup", status: "FAIL", detail: errorText(cleanupError)}); }
        report.elapsed_ms = new Date().getTime() - started;
        report.summary = "BLOCKED";
        var failed = false, blocked = false;
        for (var n = 0; n < report.checks.length; n++) {
            failed = failed || report.checks[n].status === "FAIL";
            blocked = blocked || report.checks[n].status === "BLOCKED";
        }
        report.summary = failed ? "FAIL" : (blocked || report.captures.length !== 4 ? "BLOCKED" : "SMOKE_ONLY_PASS");
        try {
            if (!output || !output.exists) { throw new Error("Report directory unavailable"); }
            write(new File(output.fsName + "/report.json"), json(report) + "\n");
            alert("Stellar diagnostics: " + report.summary + "\n\nReport folder on Desktop:\n" + runID + "\n\nThis is a smoke test, not release approval.");
        } catch (finalError) {
            alert("Stellar diagnostics could not write the report.\nEnable Preferences > Scripting & Expressions > Allow Scripts To Write Files And Access Network.\nNo preference was changed automatically.\n\n" + json(report));
        }
    }
})();
