// Original 21 sequence tests are re-run on the exact v1.3 JSX as well.
// These model API contracts; they do not execute After Effects or its save UI.
const assert = require('node:assert/strict');
const fs = require('node:fs');
process.env.STELLAR_DIAGNOSTIC_SCRIPT = __dirname + '/Stellar_AE_Diagnostics_v1_3.jsx';
const {run, restored} = require('./test_runner_v1_1.cjs');
let count=0;
function test(name,fn){fn();count++;console.log('PASS v1.3 '+name);}
function stopped(s){assert.equal(s.report.summary,'BLOCKED');assert.equal(s.ctx.calls.length,0);assert.equal(s.ctx.begins,0);}
test('empty project needs neither confirmation nor newProject',()=>{
 const s=run();restored(s);assert.equal(s.ctx.confirmations.length,0);assert.equal(s.ctx.newProjectCalls,0);
 assert.equal(s.report.preflight[0].item_count,0);assert.equal(s.report.preflight[0].queue_count,0);
 assert.equal(s.report.runner_version,'1.3');assert.ok(s.report.environment);
});
test('each formerly conflated project condition is explicit',()=>{
 for(const [opt,reason] of [[{busy:true},'PROJECT_ITEMS=3'],[{saved:true},'PROJECT_HAS_FILE'],[{initialQueue:true},'QUEUE_ITEMS=1']]){
  const s=run(opt);stopped(s);assert.ok(s.report.preflight[0].reasons.includes(reason));
  assert.equal(s.ctx.newProjectCalls,0);assert.equal(s.ctx.changes,0);
  assert.ok(s.ctx.confirmations[0].noAsDefault);assert.ok(s.report.environment);
 }
});
test('one approved native preparation followed by all eight cases',()=>{
 const s=run({busy:true,saved:true,initialQueue:true,approvePreparation:true});
 assert.equal(s.ctx.newProjectCalls,1);assert.equal(s.ctx.confirmations.length,1);
 assert.equal(s.ctx.newProjectOriginal.items,3);assert.equal(s.ctx.newProjectOriginal.queue,1);
 assert.equal(s.report.summary,'SMOKE_ONLY_PASS');assert.equal(s.report.captures.length,8);restored(s);
 assert.equal(s.report.preflight.find(x=>x.stage==='after_new_project').reasons.length,0);
});
test('native save cancellation does not create or render fixtures',()=>{
 const s=run({busy:true,saved:true,approvePreparation:true,cancelNativeSave:true});stopped(s);
 assert.equal(s.ctx.itemCount,3);assert.ok(s.ctx.app.project.file);assert.equal(s.ctx.changes,0);
 assert.equal(s.ctx.newProjectCalls,1);assert.ok(s.report.checks.some(x=>x.detail.includes('NEW_PROJECT_CANCELLED')));
});
test('active or paused render is untouched without asking to reset',()=>{
 const s=run({initialRendering:true,busy:true,approvePreparation:true});stopped(s);
 assert.equal(s.ctx.confirmations.length,0);assert.equal(s.ctx.newProjectCalls,0);assert.equal(s.ctx.rendering,true);
 assert.ok(s.report.preflight[0].reasons.includes('RENDER_ACTIVE_OR_PAUSED'));
});
test('disabled rendering cannot be bypassed by approved preparation',()=>{
 const s=run({busy:true,disableRendering:true,approvePreparation:true});stopped(s);
 assert.equal(s.ctx.newProjectCalls,0);assert.equal(s.ctx.confirmations.length,0);
 assert.ok(s.report.preflight[0].reasons.includes('RENDER_DISABLED'));
});
test('unknown file/count or failed getter fails closed with exact fields',()=>{
 for(const opt of [{unknownFile:true},{unreadableItems:true},{invalidCount:true}]){
  const s=run({...opt,approvePreparation:true});stopped(s);
  assert.ok(s.report.preflight[0].read_errors.length);assert.equal(s.ctx.newProjectCalls,0);
  assert.equal(s.ctx.confirmations.length,0);assert.equal(s.ctx.changes,0);
  assert.ok(!JSON.stringify(s.report).includes('private path must not leak'));
 }
});
test('consent is invalidated if project changes while dialog is open',()=>{
 const s=run({busy:true,approvePreparation:true,switchDuringConsent:true});stopped(s);
 assert.equal(s.ctx.newProjectCalls,0);assert.equal(s.ctx.app.project.numItems,11);
 assert.ok(s.report.checks.some(x=>x.detail.includes('PROJECT_CHANGED_DURING_CONFIRMATION')));
});
test('render starting during consent prevents closing the project',()=>{
 const s=run({busy:true,approvePreparation:true,renderDuringConsent:true});stopped(s);
 assert.equal(s.ctx.newProjectCalls,0);assert.equal(s.ctx.rendering,true);
});
test('a nonempty startup template is blocked rather than deleted or retried',()=>{
 const s=run({busy:true,approvePreparation:true,startupTemplate:true});stopped(s);
 assert.equal(s.ctx.newProjectCalls,1);assert.equal(s.ctx.itemCount,1);assert.equal(s.ctx.changes,0);
 assert.ok(s.report.preflight.find(x=>x.stage==='after_new_project').reasons.includes('PROJECT_ITEMS=1'));
 assert.ok(s.ctx.alerts[0].includes('PROJECT_ITEMS=1'));
});
test('missing project can be prepared only with consent',()=>{
 const no=run({noProject:true});stopped(no);assert.equal(no.ctx.newProjectCalls,0);
 const yes=run({noProject:true,approvePreparation:true});assert.equal(yes.report.summary,'SMOKE_ONLY_PASS');restored(yes);
});
test('file permission denial and missing effect happen before project replacement',()=>{
 for(const opt of [{writeDenied:true},{notInstalled:true}]){
  const s=run({...opt,busy:true,approvePreparation:true});assert.equal(s.ctx.newProjectCalls,0);
  assert.equal(s.ctx.confirmations.length,0);assert.equal(s.ctx.itemCount,3);assert.equal(s.ctx.changes,0);
 }
});
test('native preparation exception is FAIL, never silent retry',()=>{
 const s=run({busy:true,approvePreparation:true,newProjectThrows:true});
 assert.equal(s.report.summary,'FAIL');assert.equal(s.ctx.newProjectCalls,1);assert.equal(s.ctx.changes,0);
 assert.equal(s.ctx.StellarDiagnostics11,undefined);assert.equal(s.ctx.calls.length,0);
});
test('project switch between cases cannot trigger a new preparation dialog',()=>{
 const s=run({switchProject:true,approvePreparation:true});assert.equal(s.report.summary,'BLOCKED');
 assert.equal(s.ctx.newProjectCalls,0);assert.equal(s.ctx.confirmations.length,0);
 assert.ok(s.report.preflight.some(x=>x.reasons.includes('PROJECT_CHANGED')));restored(s);
});
test('preflight records no project paths/names or implicit native saving',()=>{
 const s=run({saved:true});assert.ok(!JSON.stringify(s.report.preflight).includes('fsName'));
 const source=fs.readFileSync(process.env.STELLAR_DIAGNOSTIC_SCRIPT,'utf8');
 assert.ok(!/app\.project\.close\s*\(/.test(source));assert.ok(!/app\.project\.save\s*\(/.test(source));
 assert.equal((source.match(/app\.newProject\(\)/g)||[]).length,1);
 assert.equal(s.report.loaded_build_id,'NOT_VERIFIED');assert.equal(s.report.release_status,'NOT_APPROVED');
});
test('exact v0.10 validation target is embedded without claiming runtime identity',()=>{
 const s=run();
 assert.equal(s.report.candidate_commit,'9f3e73bc92534941db1106f521786d8c3c792347');
 assert.equal(s.report.candidate_build_id,'sg-0.10.0-9f3e73bc9253-clean-bc5efe6bf48a-aarch64-apple-darwin-36911997718.1');
 assert.equal(s.report.candidate_artifact_id,11187002476);
 assert.equal(s.report.loaded_build_id,'NOT_VERIFIED');
 assert.equal(s.report.release_status,'NOT_APPROVED');
 restored(s);
});
console.log(`21 retained + ${count} v1.3 tests PASS; real AE 1.3 NOT RUN.`);
