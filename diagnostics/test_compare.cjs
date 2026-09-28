// API-contract fixtures only; this does not run After Effects or Cosmic.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
// Reuse the prior fixture in an isolated module, without changing its file.
// Appending an export exposes the mock string; all retained tests still run.
const fixtureModule = {exports:{}};
const fixtureSource = fs.readFileSync(__dirname + '/test_runner_v1_1.cjs', 'utf8');
vm.runInNewContext(fixtureSource + '\nmodule.exports.mock = mock;',
 {require, process, console, module:fixtureModule, __dirname}, {timeout:10000});
const {mock: baseMock, restored} = fixtureModule.exports;
const script = fs.readFileSync(__dirname + '/Stellar_Cosmic_Compare.jsx', 'utf8');
const mock = baseMock.replace('^Stellar-AE-', '^Stellar-Compare-') + `
var applications=[];
app.effects=opt.notInstalled?[]:[{displayName:'Stellar Gradient',matchName:'StellarLabs.StellarGradient',version:'fixture'}];
if(!opt.noCosmic)app.effects.push({displayName:'Cosmic',matchName:'Loophouse Cosmic',version:'fixture'});
var originalEffect=makeEffect;
makeEffect=function(match){
 applications.push(match);
 if(opt.cosmicInitFails&&match==='Loophouse Cosmic')throw Error('Cosmic fixture init failure');
 var fx=originalEffect();
 var originalProperty=fx.property;
 // Distinct names/defaults ensure the reference isn't silently replaced by Stellar.
 fx.property=function(i){var p=originalProperty(i);if(!p)return p;
  p.setValue=function(){throw Error('FORBIDDEN: modifying a fresh effect default');};
  if(typeof i==='number')p.matchName=match+'-'+('0000'+i).slice(-4);
  if(match==='Loophouse Cosmic'){p.name='reference-'+i;p.value=42;}
  return p;
 };
 return fx;
};
var originalAddComp=project.items.addComp;
project.items.addComp=function(name,w,h,par,duration,fps){
 var c=originalAddComp(name);c.w=w;c.h=h;c.fps=fps;return c;
};
var originalRender=project.renderQueue.render;
project.renderQueue.render=function(){
 var q=qitems[0];originalRender();
 if(opt.badPNG||opt.badSize||opt.missingPNG||q.status!==3019)return;
 function u32(v){return String.fromCharCode((v>>>24)&255,(v>>>16)&255,(v>>>8)&255,v&255);}
 var path=q.outputModule(1).file.fsName.replace('[#####]','00000');
 var h=files[path];if(h)files[path]=h.slice(0,16)+u32(q.comp.w)+u32(q.comp.h)+h.slice(24);
};
`;
function run(opt={}) {
 const ctx=vm.createContext({opt});
 vm.runInContext(mock+script,ctx,{timeout:5000});
 if(opt.doubleRun)vm.runInContext(script,ctx,{timeout:5000});
 let n=0;
 while(ctx.tasks.length){
  assert.ok(++n<10,'unbounded scheduling');
  const task=ctx.tasks.shift();ctx.turn++;
  if(opt.switchProject&&ctx.turn===2)ctx.app.project={file:{},numItems:10,renderQueue:{numItems:0,rendering:false}};
  vm.runInContext(task.code,ctx,{timeout:5000});
 }
 const path=Object.keys(ctx.files).find(p=>p.endsWith('/report.json'));
 let report;try{report=path?JSON.parse(ctx.files[path]):null;}catch{}
 return {ctx,report};
}
let count=0;
function test(name,fn){fn();count++;console.log('PASS compare '+name);}
function stopped(s){assert.equal(s.report.summary,'BLOCKED');assert.equal(s.ctx.calls.length,0);assert.equal(s.ctx.changes,0);}
test('four exact effect/dimension pairs, one frame per callback',()=>{
 const s=run();assert.equal(s.report.summary,'CAPTURES_COMPLETE');assert.equal(s.report.captures.length,4);
 assert.equal(new Set(s.ctx.calls.map(c=>c.turn)).size,4);
 assert.deepEqual(Array.from(s.ctx.applications),['Loophouse Cosmic','StellarLabs.StellarGradient','Loophouse Cosmic','StellarLabs.StellarGradient']);
 assert.deepEqual(s.report.captures.map(x=>[x.width,x.height]),[[128,96],[128,96],[512,288],[512,288]]);
 assert.ok(s.report.cases.every(c=>c.parameter_writes===0&&c.project_bpc===8&&c.engine_requested==='DEFAULT_UNCHANGED'));
 restored(s);
});
test('actual default metadata retained for each host, no controls copied',()=>{
 const s=run();for(const c of s.report.cases){assert.ok(c.controls_at_render.every(p=>p.match_name.startsWith(c.effect_match)));}
 assert.equal(s.report.cases[0].controls_at_render[0].value,42);
 assert.equal(s.report.cases[1].controls_at_render[0].value,1);
 assert.ok(!script.includes('.setValue('));
});
test('capture success never claims parity, actual GPU or runtime build identity',()=>{
 const s=run();assert.equal(s.report.comparison_status,'NOT_EVALUATED');assert.equal(s.report.loaded_build_id,'NOT_VERIFIED');assert.equal(s.report.release_status,'NOT_APPROVED');
 assert.ok(s.report.captures.every(c=>c.engine_actually_used==='NOT_VERIFIED'&&c.pixel_correctness==='NOT_RUN'));
 assert.ok(s.ctx.alerts.at(-1).includes('Stellar-Compare-'));
});
test('missing reference blocks BEFORE any fixture',()=>stopped(run({noCosmic:true})));
test('missing Stellar cannot be replaced by registered Cosmic',()=>stopped(run({notInstalled:true})));
test('busy/saved project without consent unchanged',()=>{for(const opt of [{busy:true},{saved:true}]){stopped(run(opt));}});
test('consented new project uses native save path then captures',()=>{const s=run({saved:true,busy:true,approvePreparation:true});assert.equal(s.ctx.newProjectCalls,1);assert.equal(s.report.summary,'CAPTURES_COMPLETE');restored(s);});
test('native save cancellation stops without writes to user project',()=>{const s=run({saved:true,busy:true,approvePreparation:true,cancelNativeSave:true});stopped(s);assert.equal(s.ctx.itemCount,3);});
test('project changed while consent dialog open is not closed',()=>{const s=run({busy:true,approvePreparation:true,switchDuringConsent:true});stopped(s);assert.equal(s.ctx.newProjectCalls,0);});
test('unknown state, active render and disabled rendering fail closed',()=>{for(const opt of [{unknownFile:true},{unreadableItems:true},{invalidCount:true},{initialRendering:true},{disableRendering:true}]){stopped(run(opt));}});
test('no-effect fallback is forbidden when Cosmic cannot initialize',()=>{const s=run({cosmicInitFails:true});assert.equal(s.report.summary,'FAIL');assert.equal(s.ctx.calls.length,0);restored(s);});
test('apply failure clears owned fixture and undo group',()=>{const s=run({cannotApply:true});assert.equal(s.report.summary,'FAIL');restored(s);});
test('QUEUED is blocked, remaining independent pairs execute without retries',()=>{const s=run({queuedCase:'cosmic_128'});assert.equal(s.report.summary,'BLOCKED');assert.equal(s.report.cases[0].after_status,'QUEUED');assert.equal(s.report.cases[3].status,'PASS');assert.equal(s.ctx.calls.length,4);restored(s);});
test('all QUEUED never becomes captures-complete',()=>{const s=run({queuedAll:true});assert.equal(s.report.summary,'BLOCKED');assert.equal(s.report.captures.length,0);restored(s);});
test('native error stops further captures',()=>{const s=run({errorCase:'stellar_128'});assert.equal(s.report.summary,'FAIL');assert.equal(s.ctx.calls.length,2);assert.equal(s.report.cases[2].status,'NOT RUN');restored(s);});
test('user render cancellation is respected',()=>{const s=run({cancelCase:'cosmic_128'});assert.equal(s.report.summary,'BLOCKED');assert.equal(s.ctx.calls.length,1);restored(s);});
test('DONE with host error is not complete',()=>{const s=run({hostErrorCase:'cosmic_128'});assert.equal(s.report.summary,'FAIL');restored(s);});
test('existing error handler is preserved and its limitation recorded',()=>{const s=run({existingError:true,hostErrorCase:'cosmic_128'});assert.equal(s.ctx.priorErrors,1);assert.equal(s.ctx.app.onError,'previousError');assert.ok(s.report.cases[0].error_capture.includes('unavailable'));restored(s);});
test('render exceptions do not leak user paths',()=>{const s=run({throwCase:'cosmic_128'});assert.equal(s.report.summary,'FAIL');assert.ok(!JSON.stringify(s.report).includes('/Users/private'));restored(s);});
test('permission/late write failures never announce complete',()=>{for(const opt of [{writeDenied:true},{lateWriteFail:true}]){const s=run(opt);assert.ok(!s.ctx.alerts.some(a=>a.includes('CAPTURES_COMPLETE')));assert.equal(s.ctx.begins,s.ctx.ends);}});
test('missing PNG template blocked without editing templates',()=>{const s=run({noPNG:true});assert.equal(s.report.summary,'BLOCKED');assert.equal(s.ctx.calls.length,0);restored(s);});
test('invalid header, wrong size or missing file cannot pass',()=>{for(const opt of [{badPNG:true},{badSize:true},{missingPNG:true}]){const s=run(opt);assert.equal(s.report.summary,'FAIL');restored(s);}});
test('second launch does not start another job',()=>{const s=run({doubleRun:true});assert.equal(s.ctx.calls.length,4);restored(s);});
test('project switch between tasks stops, never prepares another',()=>{const s=run({switchProject:true,approvePreparation:true});assert.equal(s.report.summary,'BLOCKED');assert.equal(s.ctx.calls.length,1);assert.equal(s.ctx.newProjectCalls,0);restored(s);});
test('scheduler failure releases reservation',()=>{const s=run({scheduleFails:true});assert.equal(s.report.summary,'FAIL');restored(s);});
test('unsafe cleanup is not hidden or broadly repaired',()=>{const s=run({cleanupFails:true});assert.equal(s.report.summary,'FAIL');assert.equal(s.ctx.calls.length,1);assert.equal(s.ctx.begins,s.ctx.ends);});
test('active render left untouched, no forced stop',()=>{const s=run({stillRendering:'cosmic_128'});assert.equal(s.report.summary,'FAIL');assert.equal(s.ctx.rendering,true);assert.equal(s.ctx.qitems.length,1);});
console.log(count+' comparison harness checks PASS. Real Cosmic/Stellar reference capture: NOT RUN.');
