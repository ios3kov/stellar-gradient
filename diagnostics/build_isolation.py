"""Build a single-file diagnostic from the pinned, host-tested comparison runner.
Generated source is kept in ignored dist/diagnostics, not edited or checked in.
"""
from pathlib import Path
import hashlib
import json
import subprocess

here = Path(__file__).resolve().parent
root = here.parent
src = (here / 'Stellar_Cosmic_Compare.jsx').read_text(encoding='utf-8')
assert hashlib.sha256(src.encode()).hexdigest()=='ec24297f36ae2dda2b2595cfc75a5a45356f5a83113fa01092d820dce8d5c689'
header='''/* Stellar / Cosmic feature isolation 1.0.
 * Reuses the Compare 1.0 lifecycle, now observed creating four real-host frames.
 * Eleven paired 512x288 fixtures (22 frames) isolate the texture-producing stages.
 * Changes only checked numeric controls on newly created, owned effects.
 * Captures are not equivalence approval. Never changes plugin files or licensing.
 */'''
src=header+src[src.index('\n(function ()'):]
src=src.replace('"Compare 1.0"','"Isolate 1.0"').replace('"Stellar-Compare-"','"Stellar-Isolate-"')
a=src.index('    var definitions = [')
b=src.index('    var report = {',a)
src=src[:a]+'''    var definitions = [];
    var variants = ["base", "depth", "depth_softness", "turbulence", "turbulence_size6",
        "turbulence_softness", "grain", "glow", "diffusion", "defaults_no_grain", "defaults_no_turbulence"];
    for (var v = 0; v < variants.length; v++) {
        definitions.push(["cosmic_" + variants[v], REFERENCE, 512, 288, variants[v]]);
        definitions.push(["stellar_" + variants[v], MATCH, 512, 288, variants[v]]);
    }
    // Observed match names in user capture 1790620677995; never guess control positions.
    // Tuple: Stellar disk ID, Cosmic disk ID, observed English name.
    var controls = {
        bulge:[15,31,"Bulge"], turbulence:[19,32,"Amount"], size_x:[20,33,"Size X"],
        size_y:[21,78,"Size Y"], softness:[23,38,"Softness"], glow:[30,43,"Intensity"],
        grain:[34,47,"Amount"], animate:[37,52,"Animate"], diffusion:[40,56,"Blur"]
    };
'''+src[b:]
src=src.replace('schema: 3,','schema: 4,',1)
src=src.replace('fixture_policy: "Fresh default effects; no parameter writes; white opaque solid; frame 0; 24 fps; 8-bpc project.",',
'''fixture_policy: "Fresh owned effects with explicit per-feature overrides; readback required; white opaque 512x288 solid; frame 0; 24 fps; 8-bpc project.",''')
src=src.replace('"This compares out-of-box defaults, not mathematically matched settings or all presets.",','"Same numeric feature settings do not imply the same algorithms or all-preset equivalence.",')
src=src.replace('"Default animated grain may differ by implementation; one frame is not a noise-quality or deterministic parity test.",','''"Grain Animate is disabled on both effects; seeds/noise implementations may still differ.",
            "The red cross observed in Cosmic is left intact; no license state is inferred or modified.",''')
idx=src.index('    function pngInfo(')
helper='''    function applyVariant(effect, c) {
        var values = {animate:0};
        if (c.variant === "defaults_no_grain") { values.grain = 0; }
        else if (c.variant === "defaults_no_turbulence") { values.turbulence = 0; }
        else {
            values.bulge = 0; values.turbulence = 0; values.softness = 0;
            values.glow = 0; values.grain = 0; values.diffusion = 0;
            if (c.variant === "depth" || c.variant === "depth_softness") { values.bulge = 60; }
            if (c.variant === "depth_softness" || c.variant === "turbulence_softness") { values.softness = 40; }
            if (c.variant === "turbulence" || c.variant === "turbulence_size6" || c.variant === "turbulence_softness") {
                values.turbulence = 40;
            }
            if (c.variant === "turbulence_size6") { values.size_x = 6; values.size_y = 6; }
            if (c.variant === "grain") { values.grain = 20; }
            if (c.variant === "glow") { values.glow = 160; }
            if (c.variant === "diffusion") { values.diffusion = 15; }
        }
        var pending = [], key, i;
        function find(group, match, found) {
            for (var n = 1; n <= group.numProperties; n++) {
                var p = group.property(n);
                if (p.matchName === match) { found.push(p); }
                if (p.propertyType !== PropertyType.PROPERTY) { find(p, match, found); }
            }
        }
        // Validate every target before the first write. No name-only fallback.
        for (key in values) { if (Object.prototype.hasOwnProperty.call(values, key)) {
            var spec = controls[key], id = spec[c.effect_match === MATCH ? 0 : 1];
            var match = c.effect_match + "-" + ("0000" + id).slice(-4), found = [];
            find(effect, match, found);
            if (found.length !== 1 || found[0].propertyType !== PropertyType.PROPERTY ||
                found[0].propertyValueType !== PropertyValueType.OneD || found[0].name !== spec[2]) {
                fail("ISOLATION_CONTROL_MISMATCH: " + match, "BLOCKED");
            }
            var p = found[0], wanted = values[key];
            if (typeof p.value !== "number" || !isFinite(p.value) ||
                (p.hasMin && wanted < p.minValue) || (p.hasMax && wanted > p.maxValue)) {
                fail("ISOLATION_CONTROL_RANGE: " + match, "BLOCKED");
            }
            pending.push({property:p, key:key, match_name:match, before:p.value, requested:wanted});
        } }
        c.controls_before = snapshot(effect, []); c.parameter_writes = [];
        for (i = 0; i < pending.length; i++) {
            var item = pending[i];
            var entry = {key:item.key, match_name:item.match_name, before:item.before,
                requested:item.requested, observed:null, status:"NOT VERIFIED"};
            c.parameter_writes.push(entry); // Retain partial progress if host throws.
            item.property.setValue(item.requested);
            entry.observed = item.property.value;
            if (typeof entry.observed !== "number" || !isFinite(entry.observed) || Math.abs(entry.observed - item.requested) > 0.000001) {
                fail("ISOLATION_WRITE_NOT_APPLIED: " + item.match_name);
            }
            entry.status = "PASS";
        }
        // A later host callback must not silently reset an earlier target.
        for (i = 0; i < pending.length; i++) {
            if (typeof pending[i].property.value !== "number" || !isFinite(pending[i].property.value) ||
                Math.abs(pending[i].property.value - pending[i].requested) > 0.000001) {
                fail("ISOLATION_TARGET_RESET: " + pending[i].match_name);
            }
        }
        c.controls_at_render = snapshot(effect, []);
        // Detect side effects on colors/preset/engine/etc. rather than silently masking them.
        var allowed = {};
        for (i = 0; i < pending.length; i++) { allowed[pending[i].match_name] = true; }
        function unchanged(before, after) {
            if (before.length !== after.length) { fail("ISOLATION_TREE_CHANGED"); }
            for (var n = 0; n < before.length; n++) {
                if (before[n].match_name !== after[n].match_name) { fail("ISOLATION_TREE_CHANGED"); }
                if (!allowed[before[n].match_name] && json(before[n].value) !== json(after[n].value)) {
                    fail("ISOLATION_UNEXPECTED_SIDE_EFFECT: " + before[n].match_name);
                }
                if (before[n].children) { unchanged(before[n].children, after[n].children || []); }
            }
        }
        unchanged(c.controls_before, c.controls_at_render);
    }
'''
src=src[:idx]+helper+src[idx:]
old='''            // Do not set any control, menu, engine or quality. We need the
            // actual installed default state, not presumed Cosmic parameter IDs.
            c.controls_at_render = snapshot(effect, []);
            c.parameter_writes = 0;'''
assert src.count(old)==1
src=src.replace(old,'            applyVariant(effect, c);')
old='width:definitions[i][2], height:definitions[i][3], project_bpc:8,'
assert src.count(old)==1
src=src.replace(old,'width:definitions[i][2], height:definitions[i][3], variant:definitions[i][4], project_bpc:8,')
for x in ('Stellar-Compare-','Fresh default effects; no parameter writes','Do not set any control'):
 assert x not in src,x
src=src.replace('    function applyVariant(effect, c) {','''    function isolationBlocked(message) {
        var e = new Error(message); e.sgStatus = "BLOCKED"; e.sgStop = true; throw e;
    }
    function applyVariant(effect, c) {''')
src=src.replace('fail("ISOLATION_CONTROL_MISMATCH: " + match, "BLOCKED");','isolationBlocked("ISOLATION_CONTROL_MISMATCH: " + match);')
src=src.replace('fail("ISOLATION_CONTROL_RANGE: " + match, "BLOCKED");','isolationBlocked("ISOLATION_CONTROL_RANGE: " + match);')
src=src.replace('if (c.status === "FAIL") { healthy = false; }','if (c.status === "FAIL" || e.sgStop === true) { healthy = false; }')
builder_hash = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
base_hash = hashlib.sha256((here / 'Stellar_Cosmic_Compare.jsx').read_bytes()).hexdigest()
build_id = 'isolate-1.0-' + base_hash[:12] + '-' + builder_hash[:12]
src = src.replace('        schema: 4, runner_version: VERSION, run_id: runID,',
                  '        schema: 4, runner_version: VERSION, run_id: runID, runner_build_id: "' + build_id + '",')
out = root / 'dist/diagnostics/Stellar_Feature_Isolation.jsx'
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(src, encoding='utf-8')
def git(*args):
    try:
        r = subprocess.run(['git', '-C', str(root), *args], capture_output=True,
                           text=True, timeout=10, check=False)
        return r.stdout.strip() if r.returncode == 0 else None
    except (OSError, subprocess.TimeoutExpired):
        return None
state = git('status', '--porcelain', '--untracked-files=normal')
record = {'schema': 1, 'runner_build_id': build_id, 'git_commit': git('rev-parse', 'HEAD'),
          'source_state': 'unknown' if state is None else ('dirty' if state else 'clean'),
          'base_sha256': base_hash, 'generator_sha256': builder_hash,
          'artifact': out.name, 'artifact_sha256': hashlib.sha256(out.read_bytes()).hexdigest(),
          'real_host_status': 'NOT_RUN', 'release_status': 'NOT_APPROVED'}
(out.parent / 'build.json').write_text(json.dumps(record, indent=2) + '\n', encoding='utf-8')
print(record['artifact_sha256'] + '  ' + out.name)
