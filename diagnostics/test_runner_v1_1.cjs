// Executes the exact shipped JSX against simulated AE; never a real-host PASS.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const script = fs.readFileSync(process.env.STELLAR_DIAGNOSTIC_SCRIPT || __dirname + '/Stellar_AE_Diagnostics_v1_1.jsx', 'utf8');
const mock = `
var files={},folders={'/Desktop':true},alerts=[],changes=0,begins=0,ends=0,turn=0;
var tasks=[],serial=0,qitems=[],itemCount=opt.busy?3:0,depth=8,applied=0;
var calls=[],rendering=!!opt.initialRendering,reads=[],priorErrors=0;
var confirmations=[],newProjectCalls=0,newProjectOriginal=null;
if(opt.initialQueue)qitems.push({fixture:'existing user queue'});
var $={os:'mock macOS, NOT real AE',global:this};
var PropertyType={PROPERTY:1},PropertyValueType={NO_VALUE:0,CUSTOM_VALUE:99};
var PostRenderAction={NONE:0};
var RQItemStatus={QUEUED:3015,RENDERING:3016,DONE:3019,ERR_STOPPED:3018,USER_STOPPED:3017,WILL_CONTINUE:3012,NEEDS_OUTPUT:3013,UNQUEUED:3014};
function File(path){
 this.fsName=path;this.name=path.split('/').pop();
 Object.defineProperty(this,'length',{get:function(){return (files[path]||'').length;}});
 this.open=function(mode){this.mode=mode;if(opt.writeDenied&&mode!=='r')return false;if(mode==='w')files[path]='';return mode!=='r'||path in files;};
 this.write=function(s){if(opt.lateWriteFail&&calls.length)return false;files[path]=(files[path]||'')+s;return true;};
 this.read=function(n){return (files[path]||'').slice(0,n);};this.close=function(){};
}
function Folder(path){
 this.fsName=path;Object.defineProperty(this,'exists',{get:function(){return !!folders[path];}});
 this.create=function(){folders[path]=true;return true;};
 this.getFiles=function(){var a=[];for(var p in files)if(p.indexOf(path+'/')===0&&/\\.png$/.test(p))a.push(new File(p));return a;};
}
Folder.desktop={fsName:'/Desktop'};
function control(name,id,value){return {name:name,matchName:'StellarLabs.StellarGradient-'+('0000'+id).slice(-4),propertyType:1,propertyValueType:2,value:value,canVaryOverTime:true,hasMin:false,hasMax:false,setValue:function(v){this.value=v;changes++;}};}
function makeEffect(){
 if(opt.initFails)throw new Error('AE initialization 25::3');applied++;
 var props=[control('Render Engine',48,1),control('Quality',49,2),control('Animate',37,1),control('Bulge',15,60),control('Amount',19,40),control('Intensity',30,160),control('Amount',34,20),control('Blur',40,15)];
 if(opt.drift&&applied===2)props[0].value=2;
 return {numProperties:props.length,remove:function(){changes++;},property:function(key){if(typeof key==='number')return props[key-1];for(var i=0;i<props.length;i++)if(props[i].matchName===key)return props[i];return null;}};
}
function status(q,v){q.status=v;if(q.onStatusChanged)$.global[q.onStatusChanged]();}
var project={file:opt.saved?{}:null,rootFolder:{},workingSpace:'sRGB',linearBlending:false,
 get numItems(){return itemCount;},get bitsPerChannel(){return depth;},set bitsPerChannel(v){depth=v;changes++;},
 items:{addComp:function(name){itemCount++;changes++;return {name:name,frameDuration:1/24,remove:function(){if(opt.cleanupFails)throw Error('cleanup fixture');itemCount--;},layers:{addSolid:function(){
  itemCount+=2;var folder={numItems:1,remove:function(){itemCount--;}};
  var src={parentFolder:folder,remove:function(){folder.numItems--;itemCount--;}};
  return {source:src,property:function(){return {canAddProperty:function(){return !opt.cannotApply;},addProperty:makeEffect};}};
 }}};}},
 renderQueue:{
  get numItems(){return qitems.length;},get rendering(){return rendering;},
  items:{add:function(comp){
   var om={templates:opt.noPNG?['Lossless']:['png'],applyTemplate:function(){},file:null,getSetting:function(k){reads.push(k);return 'mock-'+k;}};
   var q={comp:comp,status:3014,onStatusChanged:null,elapsedSeconds:0,startTime:null,remove:function(){qitems.splice(qitems.indexOf(q),1);},outputModule:function(){return om;},getSetting:om.getSetting};
   Object.defineProperty(q,'render',{set:function(v){q.status=v?3015:3014;}});qitems.push(q);return q;
  }},
  render:function(){
   var q=qitems[0],label=q.comp.name.replace(/^Stellar-AE-\\d+-\\d+-/,'');
   calls.push({name:label,bpc:depth,turn:turn});
   if(opt.queuedCase===label||opt.queuedAll)return;
   status(q,3016);q.startTime=new Date();q.elapsedSeconds=1;
   if(opt.throwCase===label)throw Error('render exception /Users/private/project.aep');
   if(opt.hostErrorCase===label&&app.onError)$.global[app.onError]('native test error','error');
   if(opt.errorCase===label){status(q,3018);return;}
   if(opt.cancelCase===label){status(q,3017);return;}
   if(opt.stillRendering===label){rendering=true;return;}
   status(q,3019);
   var h=String.fromCharCode(137,80,78,71,13,10,26,10,0,0,0,13)+'IHDR'+String.fromCharCode(0,0,0,opt.badSize?64:128,0,0,0,96)+'MOCK PIXELS ONLY';
   if(!opt.missingPNG)files[q.outputModule(1).file.fsName.replace('[#####]','00000')]=opt.badPNG?'bad':h;
  }
 }
};
function previousError(){priorErrors++;}
var app={version:'25.mock',buildNumber:0,project:project,disableRendering:!!opt.disableRendering,onError:opt.existingError?'previousError':null,
 effects:opt.notInstalled?[]:[{displayName:'Stellar Gradient',matchName:'StellarLabs.StellarGradient',version:'0.9.6x1'}],
 beginUndoGroup:function(){begins++;},endUndoGroup:function(){ends++;},
 scheduleTask:function(code,delay,repeat){if(opt.scheduleFails)throw Error('schedule failed');if(repeat)throw Error('unexpected repeat');if(begins!==ends||qitems.length||itemCount||app.onError!==(opt.existingError?'previousError':null))throw Error('ASYNC STATE LEAK');tasks.push({id:++serial,code:code});return serial;},
 cancelTask:function(id){tasks=tasks.filter(function(t){return t.id!==id;});}
};
function confirm(text,noAsDefault){
 confirmations.push({text:text,noAsDefault:noAsDefault,turn:turn});
 if(opt.switchDuringConsent)app.project={file:{},numItems:11,renderQueue:{numItems:0,rendering:false}};
 if(opt.renderDuringConsent)rendering=true;
 return opt.approvePreparation===true;
}
app.newProject=function(){
 newProjectCalls++;
 if(opt.approvePreparation!==true)throw Error('project creation without consent');
 if(opt.newProjectThrows)throw Error('mock native save failure');
 if(opt.cancelNativeSave)return null;
 newProjectOriginal={items:itemCount,queue:qitems.length,file:project.file};
 itemCount=opt.startupTemplate?1:0;qitems=[];
 project=Object.assign({},project,{file:null});
 Object.defineProperty(project,'numItems',{get:function(){return itemCount;}});
 Object.defineProperty(project,'bitsPerChannel',{get:function(){return depth;},set:function(v){depth=v;changes++;}});
 app.project=project;return project;
};
if(opt.unknownFile)project.file=undefined;
if(opt.unreadableItems)Object.defineProperty(project,'numItems',{get:function(){throw Error('private path must not leak');}});
if(opt.invalidCount)Object.defineProperty(project,'numItems',{get:function(){return '0';}});
if(opt.noProject)app.project=null;
function alert(text){alerts.push(text);}
`;
function run(opt={}) {
 const ctx=vm.createContext({opt});
 vm.runInContext(mock+script,ctx,{timeout:5000});
 if(opt.doubleRun)vm.runInContext(script,ctx,{timeout:5000});
 let count=0;
 while(ctx.tasks.length) {
  assert.ok(++count<20,'unbounded timer loop');
  const task=ctx.tasks.shift();ctx.turn++;
  if(opt.switchProject&&ctx.turn===2)ctx.app.project={file:{},numItems:10,renderQueue:{numItems:0,rendering:false}};
  vm.runInContext(task.code,ctx,{timeout:5000});
 }
 const file=Object.keys(ctx.files).find(p=>p.endsWith('/report.json'));
 let report=null;try{report=file?JSON.parse(ctx.files[file]):null;}catch{}
 return {ctx,report};
}
function restored(s){assert.equal(s.ctx.itemCount,0);assert.equal(s.ctx.depth,8);assert.equal(s.ctx.qitems.length,0);assert.equal(s.ctx.begins,s.ctx.ends);assert.equal(s.ctx.StellarDiagnostics11,undefined);assert.equal(s.ctx.StellarDiagnostics11Error,undefined);assert.equal(s.ctx.StellarDiagnostics11Status,undefined);}
let total=0;
function test(name,fn){fn();total++;console.log('PASS '+name);}
test('all eight cases execute on different callbacks, preserve state',()=>{const s=run();assert.equal(s.report.summary,'SMOKE_ONLY_PASS');assert.equal(s.report.captures.length,8);assert.equal(new Set(s.ctx.calls.map(c=>c.turn)).size,8);assert.equal(s.report.loaded_build_id,'NOT_VERIFIED');assert.equal(s.report.release_status,'NOT_APPROVED');restored(s);});
test('3015 on CPU16 is BLOCKED, not plugin FAIL, and 32 still runs',()=>{const s=run({queuedCase:'cpu_16'});const c=s.report.cases.find(c=>c.name==='cpu_16');assert.equal(c.status,'BLOCKED');assert.equal(c.after_status,'QUEUED');assert.equal(c.after_status_code,3015);assert.equal(s.report.cases.find(c=>c.name==='cpu_32').status,'PASS');assert.equal(s.report.summary,'BLOCKED');restored(s);});
test('all QUEUED never becomes PASS and receives no blind retries',()=>{const s=run({queuedAll:true});assert.equal(s.report.summary,'BLOCKED');assert.equal(s.ctx.calls.length,8);assert.equal(s.report.captures.length,0);restored(s);});
test('explicit ERR_STOPPED is FAIL and remaining cases NOT RUN',()=>{const s=run({errorCase:'cpu_16'});assert.equal(s.report.summary,'FAIL');assert.equal(s.report.cases[4].after_status,'ERR_STOPPED');assert.equal(s.report.cases[5].status,'NOT RUN');restored(s);});
test('USER_STOPPED respects cancellation and stops scheduling',()=>{const s=run({cancelCase:'control_8'});assert.equal(s.report.summary,'BLOCKED');assert.equal(s.ctx.calls.length,1);assert.equal(s.report.cases[1].status,'NOT RUN');restored(s);});
test('native host error cannot yield PASS even when frame completes',()=>{const s=run({hostErrorCase:'control_8'});assert.equal(s.report.summary,'FAIL');assert.equal(s.report.cases[0].host_errors.length,1);restored(s);});
test('prior error callback preserved, never overwritten',()=>{const s=run({existingError:true,hostErrorCase:'control_8'});assert.equal(s.ctx.priorErrors,1);assert.equal(s.ctx.app.onError,'previousError');assert.ok(s.report.cases[0].error_capture.includes('preserved'));restored(s);});
test('render exception still cleans temporary hooks and fixture',()=>{const s=run({throwCase:'cpu_8'});assert.equal(s.report.summary,'FAIL');assert.ok(!JSON.stringify(s.report).includes('/Users/private'));restored(s);});
test('actual transitions retained rather than guessed',()=>{const s=run();assert.deepEqual(s.report.cases[0].transitions.map(x=>x.name),['RENDERING','DONE']);assert.equal(s.report.rq_status_constants.QUEUED,3015);restored(s);});
test('existing or saved project never modified',()=>{for(const opt of [{busy:true},{saved:true}]){const s=run(opt);assert.equal(s.report.summary,'BLOCKED');assert.equal(s.ctx.changes,0);assert.equal(s.ctx.calls.length,0);}});
test('missing plugin and disabled rendering remain BLOCKED',()=>{for(const opt of [{notInstalled:true},{disableRendering:true}]){const s=run(opt);assert.equal(s.report.summary,'BLOCKED');assert.equal(s.ctx.changes,0);}});
test('write denial and late disk error do not claim completion',()=>{for(const opt of [{writeDenied:true},{lateWriteFail:true}]){const s=run(opt);assert.ok(!s.ctx.alerts.some(x=>x.includes('SMOKE_ONLY_PASS')));assert.equal(s.ctx.StellarDiagnostics11,undefined);assert.equal(s.ctx.begins,s.ctx.ends);}});
test('missing template does not produce a fake capture',()=>{const s=run({noPNG:true});assert.equal(s.report.summary,'BLOCKED');assert.equal(s.ctx.calls.length,0);restored(s);});
test('invalid or missing image does not pass',()=>{for(const opt of [{badPNG:true},{badSize:true},{missingPNG:true}]){const s=run(opt);assert.equal(s.report.summary,'FAIL');restored(s);}});
test('initialization and reapply regression still covered',()=>{for(const opt of [{initFails:true},{cannotApply:true},{drift:true}]){const s=run(opt);assert.equal(s.report.summary,'FAIL');restored(s);}});
test('double launch is blocked without a second job',()=>{const s=run({doubleRun:true});assert.equal(s.ctx.calls.length,8);assert.ok(s.ctx.alerts[0].includes('already running'));restored(s);});
test('project changed between tasks is untouched',()=>{const s=run({switchProject:true});assert.equal(s.report.summary,'BLOCKED');assert.equal(s.ctx.calls.length,1);assert.equal(s.ctx.app.project.numItems,10);restored(s);});
test('scheduler failure clears registration with no fixture left',()=>{const s=run({scheduleFails:true});assert.equal(s.report.summary,'FAIL');assert.equal(s.ctx.calls.length,0);restored(s);});
test('cleanup failure is reported, not broadly repaired',()=>{const s=run({cleanupFails:true});assert.equal(s.report.summary,'FAIL');assert.equal(s.report.cases[0].cleanup,'FAIL');assert.equal(s.ctx.calls.length,1);assert.equal(s.ctx.begins,s.ctx.ends);});
test('in-progress host render is never modified or forced stopped',()=>{const s=run({stillRendering:'control_8'});assert.equal(s.report.summary,'FAIL');assert.equal(s.ctx.rendering,true);assert.equal(s.ctx.qitems.length,1);assert.ok(s.ctx.itemCount>0);assert.equal(s.ctx.begins,s.ctx.ends);});
test('clean fixture controls and no-effect controls remain separate',()=>{const s=run();assert.equal(s.report.cases[0].engine_requested,0);assert.equal(s.report.cases[0].controls_at_render,undefined);assert.deepEqual(s.report.cases[2].disabled_parameter_ids,[15,19,30,34,40]);assert.ok(s.report.captures.every(c=>c.engine_actually_used==='NOT_VERIFIED'&&c.pixel_correctness==='NOT_RUN'));restored(s);});
console.log(total+' sequence tests PASS; real AE NOT RUN.');

module.exports = {run, restored, test};
