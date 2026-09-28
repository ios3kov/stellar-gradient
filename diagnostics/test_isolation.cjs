// Runs the exact isolation JSX with a fake AE; does not render native plugins.
const assert=require('node:assert/strict'), fs=require('node:fs'), vm=require('node:vm');
const fixture={exports:{}};
vm.runInNewContext(fs.readFileSync(__dirname+'/test_runner_v1_1.cjs','utf8')+'\nmodule.exports.mock=mock;',
 {require,process,console,module:fixture,__dirname},{timeout:10000});
const {mock:baseMock,restored}=fixture.exports;
const script=fs.readFileSync(__dirname+'/../dist/diagnostics/Stellar_Feature_Isolation.jsx','utf8');
const mock=baseMock.replace('^Stellar-AE-','^Stellar-Isolate-')+`
PropertyValueType.OneD=2;
var writes=[],applications=[],allEffects=[];
app.effects=opt.notInstalled?[]:[{displayName:'Stellar Gradient',matchName:'StellarLabs.StellarGradient',version:'fixture'}];
if(!opt.noCosmic)app.effects.push({displayName:'Cosmic',matchName:'Loophouse Cosmic',version:'fixture'});
makeEffect=function(match){
 if(opt.initFails)throw Error('initialization fixture failure');
 applications.push(match);
 var specs=[[15,31,'Bulge',60],[19,32,'Amount',40],[20,33,'Size X',3],[21,78,'Size Y',3],
 [23,38,'Softness',40],[30,43,'Intensity',160],[34,47,'Amount',20],[37,52,'Animate',1],[40,56,'Blur',15]];
 var props=[],cosmic=match==='Loophouse Cosmic';
 for(var i=0;i<specs.length;i++){
  var s=specs[i],p=control(s[2],s[cosmic?1:0],s[3]);
  p.matchName=match+'-'+('0000'+s[cosmic?1:0]).slice(-4); props.push(p);
 }
 props.push(control('Unrelated color',98,[.1,.2,.3,1]));
 props[9].matchName=match+'-0098';
 props.push(control('Presets',99,5));props[10].matchName=match+'-0099';
 if(opt.targetMissing)props.splice(0,1);
 if(opt.targetDuplicate)props.push(props[0]);
 if(opt.wrongType)props[0].propertyValueType=99;
 if(opt.wrongName)props[0].name='Other';
 if(opt.outOfRange){props[0].hasMin=true;props[0].minValue=10;}
 if(opt.nonNumeric)props[0].value='60';
 for(var i=0;i<props.length;i++)(function(p){p.setValue=function(v){
  if(p.name==='Unrelated color'||p.name==='Presets')throw Error('FORBIDDEN write');
  writes.push({match:p.matchName,value:v,turn:turn}); changes++;
  if(opt.throwWrite)throw Error('host write failed');
  if(!opt.ignoreWrite)p.value=v;
  if(opt.sideEffect)props[9].value=[1,0,0,1];
  if(opt.resetEarlier&&p.name==='Blur')props[7].value=1;
 };})(props[i]);
 var fx={numProperties:props.length,property:function(key){if(typeof key==='number')return props[key-1];return null;},remove:function(){changes++;}};
 allEffects.push(fx); return fx;
};
var oldAddComp=project.items.addComp;
project.items.addComp=function(name,w,h,par,duration,fps){var c=oldAddComp(name);c.w=w;c.h=h;c.fps=fps;return c;};
var oldRender=project.renderQueue.render;
project.renderQueue.render=function(){
 var q=qitems[0];oldRender();
 if(opt.badPNG||opt.badSize||opt.missingPNG||q.status!==3019)return;
 function u32(v){return String.fromCharCode(v>>>24&255,v>>>16&255,v>>>8&255,v&255);}
 var path=q.outputModule(1).file.fsName.replace('[#####]','00000');
 var h=files[path];if(h)files[path]=h.slice(0,16)+u32(q.comp.w)+u32(q.comp.h)+h.slice(24);
};
`;
function run(opt={}){
 const ctx=vm.createContext({opt});vm.runInContext(mock+script,ctx,{timeout:5000});
 if(opt.doubleRun)vm.runInContext(script,ctx,{timeout:5000});
 let n=0;while(ctx.tasks.length){assert.ok(++n<=24,'unbounded scheduling');const t=ctx.tasks.shift();ctx.turn++;
  if(opt.switchProject&&ctx.turn===2)ctx.app.project={file:{},numItems:10,renderQueue:{numItems:0,rendering:false}};
  vm.runInContext(t.code,ctx,{timeout:5000});}
 const f=Object.keys(ctx.files).find(p=>p.endsWith('/report.json'));
 let report=null;try{report=f?JSON.parse(ctx.files[f]):null;}catch{}
 return {ctx,report};
}
let count=0;function test(name,fn){fn();count++;console.log('PASS isolation '+name);}
function blocked(s){assert.equal(s.report.summary,'BLOCKED');assert.equal(s.ctx.calls.length,0);assert.equal(s.ctx.changes,0);}
function values(c){return Object.fromEntries(c.parameter_writes.map(w=>[w.key,w.observed]));}
test('eleven paired fixtures, separate callbacks, verified target readback',()=>{
 const s=run();assert.equal(s.report.summary,'CAPTURES_COMPLETE');assert.equal(s.report.captures.length,22);
 assert.equal(new Set(s.ctx.calls.map(c=>c.turn)).size,22);assert.equal(s.ctx.allEffects.length,22);
 for(let i=0;i<22;i+=2){let a=s.report.cases[i],b=s.report.cases[i+1];
  assert.equal(a.effect_match,'Loophouse Cosmic');assert.equal(b.effect_match,'StellarLabs.StellarGradient');
  assert.equal(a.variant,b.variant);assert.deepEqual(values(a),values(b));
  assert.ok(a.parameter_writes.every(w=>w.status==='PASS'&&w.observed===w.requested));
  assert.equal(a.width,512);assert.equal(a.height,288);assert.equal(a.frame_time_seconds,0);
 }restored(s);
});
test('feature inputs match acceptance, not hidden preset/engine edits',()=>{
 const r=run().report, all=Object.fromEntries(r.cases.filter(c=>c.effect_match==='Loophouse Cosmic').map(c=>[c.variant,values(c)]));
 const base={animate:0,bulge:0,turbulence:0,softness:0,glow:0,grain:0,diffusion:0};
 assert.deepEqual(all.base,base);assert.deepEqual(all.depth,{...base,bulge:60});
 assert.deepEqual(all.depth_softness,{...base,bulge:60,softness:40});
 assert.deepEqual(all.turbulence,{...base,turbulence:40});assert.deepEqual(all.turbulence_size6,{...base,turbulence:40,size_x:6,size_y:6});
 assert.deepEqual(all.turbulence_softness,{...base,turbulence:40,softness:40});
 assert.deepEqual(all.grain,{...base,grain:20});assert.deepEqual(all.glow,{...base,glow:160});assert.deepEqual(all.diffusion,{...base,diffusion:15});
 assert.deepEqual(all.defaults_no_grain,{animate:0,grain:0});assert.deepEqual(all.defaults_no_turbulence,{animate:0,turbulence:0});
});
test('missing/duplicate/wrong controls blocked before any parameter writes',()=>{
 for(const key of ['targetMissing','targetDuplicate','wrongType','wrongName','outOfRange','nonNumeric']){
  const s=run({[key]:true});assert.equal(s.report.summary,'BLOCKED',key);assert.equal(s.ctx.calls.length,0,key);assert.equal(s.ctx.writes.length,0,key);restored(s);
 }
});
test('ignored/failed write cannot produce captures',()=>{for(const key of ['ignoreWrite','throwWrite']){const s=run({[key]:true});assert.equal(s.report.summary,'FAIL');assert.equal(s.ctx.calls.length,0);restored(s);}});
test('late reset and unrelated side effects fail',()=>{for(const key of ['resetEarlier','sideEffect']){const s=run({[key]:true});assert.equal(s.report.summary,'FAIL');assert.equal(s.ctx.calls.length,0);restored(s);}});
test('both installed references required before host mutation',()=>{blocked(run({notInstalled:true}));blocked(run({noCosmic:true}));});
test('existing user project without consent is untouched',()=>{blocked(run({busy:true}));blocked(run({saved:true}));});
test('approved new project preserves native save decision',()=>{const s=run({busy:true,saved:true,approvePreparation:true});assert.equal(s.ctx.newProjectCalls,1);assert.equal(s.report.summary,'CAPTURES_COMPLETE');restored(s);blocked(run({busy:true,approvePreparation:true,cancelNativeSave:true}));});
test('unsafe state and consent races do not close user work',()=>{for(const opt of [{initialRendering:true},{disableRendering:true},{unknownFile:true},{unreadableItems:true},{busy:true,approvePreparation:true,switchDuringConsent:true}]){const s=run(opt);blocked(s);assert.equal(s.ctx.newProjectCalls,0);}});
test('QUEUED does not imply native error or silent retry',()=>{const s=run({queuedCase:'cosmic_base'});assert.equal(s.report.summary,'BLOCKED');assert.equal(s.ctx.calls.length,22);assert.equal(s.report.cases[0].after_status,'QUEUED');assert.equal(s.report.cases[21].status,'PASS');restored(s);});
test('explicit errors and cancellation stop remaining cases',()=>{for(const opt of [{errorCase:'stellar_base'},{cancelCase:'stellar_base'},{hostErrorCase:'stellar_base'},{throwCase:'stellar_base'}]){const s=run(opt);assert.ok(['FAIL','BLOCKED'].includes(s.report.summary));assert.equal(s.ctx.calls.length,2);assert.equal(s.report.cases[2].status,'NOT RUN');restored(s);}});
test('missing template or corrupt/missing PNG cannot succeed',()=>{for(const opt of [{noPNG:true},{badPNG:true},{badSize:true},{missingPNG:true}]){const s=run(opt);assert.notEqual(s.report.summary,'CAPTURES_COMPLETE');restored(s);}});
test('disk/permission failure not announced as complete',()=>{for(const opt of [{writeDenied:true},{lateWriteFail:true}]){const s=run(opt);assert.ok(!s.ctx.alerts.some(a=>a.includes('CAPTURES_COMPLETE')));assert.equal(s.ctx.begins,s.ctx.ends);}});
test('project switch and double-run retain reservation safeguards',()=>{const s=run({switchProject:true,approvePreparation:true});assert.equal(s.report.summary,'BLOCKED');assert.equal(s.ctx.calls.length,1);assert.equal(s.ctx.newProjectCalls,0);restored(s);const d=run({doubleRun:true});assert.equal(d.ctx.calls.length,22);restored(d);});
test('scheduler/init/cleanup failure stops safely',()=>{for(const opt of [{scheduleFails:true},{initFails:true},{cannotApply:true}]){const s=run(opt);assert.equal(s.report.summary,'FAIL');restored(s);}const s=run({cleanupFails:true});assert.equal(s.report.summary,'FAIL');assert.equal(s.ctx.calls.length,1);assert.equal(s.ctx.begins,s.ctx.ends);});
test('render in progress never force-stopped',()=>{const s=run({stillRendering:'cosmic_base'});assert.equal(s.report.summary,'FAIL');assert.equal(s.ctx.rendering,true);assert.equal(s.ctx.qitems.length,1);});
test('existing error handler not replaced and limitation retained',()=>{const s=run({existingError:true,hostErrorCase:'cosmic_base'});assert.equal(s.ctx.priorErrors,1);assert.equal(s.ctx.app.onError,'previousError');assert.ok(s.report.cases[0].error_capture.includes('unavailable'));restored(s);});
test('no license/identity/GPU/parity certification from creation',()=>{const s=run();assert.equal(s.report.loaded_build_id,'NOT_VERIFIED');assert.equal(s.report.comparison_status,'NOT_EVALUATED');assert.equal(s.report.release_status,'NOT_APPROVED');assert.ok(s.report.captures.every(c=>c.pixel_correctness==='NOT_RUN'&&c.engine_actually_used==='NOT_VERIFIED'));assert.ok(s.ctx.alerts.at(-1).includes('Stellar-Isolate-'));assert.ok(!JSON.stringify(s.report).includes('/Users/private'));});
console.log(count+' isolation harness tests PASS; actual host isolation NOT RUN.');
