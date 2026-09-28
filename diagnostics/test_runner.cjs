// Harness tests execute the real JSX against a fake AE, not an actual host.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const source = fs.readFileSync(__dirname + '/Stellar_AE_Diagnostics.jsx', 'utf8');
const mock = `
var files = {}, folders = {'/Desktop': true}, alerts = [], changes = 0, begins = 0, ends = 0;
var itemCount = opt.busy ? 3 : 0, applied = 0, renders = 0, qitems = [], depth = 16;
var PropertyType = {PROPERTY:1, NAMED_GROUP:2}, PropertyValueType = {NO_VALUE:0, CUSTOM_VALUE:99};
var PostRenderAction = {NONE:0}, RQItemStatus = {DONE:1};
var $ = {os:'Macintosh mock (NOT real AE)'};
function File(path) {
 this.fsName = path; this.name = path.split('/').pop(); this.encoding='';
 Object.defineProperty(this,'length',{get:function(){return (files[path] || '').length;}});
 Object.defineProperty(this,'exists',{get:function(){return path in files;}});
 this.open = function(mode) {this.mode=mode; if(opt.writeDenied && mode!=='r') return false; if(mode==='w') files[path]=''; return mode!=='r' || path in files;};
 this.write = function(s){files[path] = (files[path] || '') + s; return true;};
 this.read = function(n){return (files[path] || '').slice(0,n);}; this.close=function(){};
}
function Folder(path) {
 this.fsName=path;
 Object.defineProperty(this,'exists',{get:function(){return !!folders[path];}});
 this.create=function(){folders[path]=true; return true;};
 this.getFiles=function(){var out=[]; for(var p in files){if(p.indexOf(path+'/')===0 && /\\.png$/.test(p)) out.push(new File(p));} return out;};
}
Folder.desktop={fsName:'/Desktop'};
function control(name,value){return {name:name,matchName:name,propertyType:1,propertyValueType:2,canVaryOverTime:true,hasMin:false,hasMax:false,value:value,setValue:function(v){this.value=v; changes++;}};}
function newEffect(){
 if(opt.initFails) throw new Error('AE error 25::3');
 applied++;
 var props=[control('Render Engine',1),control('Quality',2),control('Animate',1),control('Color 1',[0.1,0.2,0.3,1])];
 if(opt.defaultDrift && applied===2) props[0].value=2;
 return {numProperties:props.length,property:function(i){return props[i-1];},remove:function(){changes++;}};
}
var rootFolder={};
var rq={
 get numItems(){return qitems.length;}, rendering:false,
 items:{add:function(c){
  var om={templates:opt.noPNG ? ['Lossless'] : ['PNG Sequence'],applyTemplate:function(){},file:null};
  var q={comp:c,outputModule:function(){return om;},remove:function(){qitems.splice(qitems.indexOf(q),1);},status:0};
  qitems.push(q); return q;
 }},
 render:function(){
  renders++; if(opt.renderFails) throw new Error('native render exception');
  var q=qitems[0]; q.status=opt.cancel ? 9 : 1;
  var h=String.fromCharCode(137,80,78,71,13,10,26,10,0,0,0,13)+'IHDR'+String.fromCharCode(0,0,0,opt.badDimensions?64:128,0,0,0,96)+'MOCK NOT REAL PIXELS';
  files[q.outputModule(1).file.fsName.replace('[#####]','00000')]=opt.badPNG?'NOT PNG':h;
 }
};
var project={
 get numItems(){return itemCount;}, file:opt.saved ? {} : null,renderQueue:rq,rootFolder:rootFolder,
 workingSpace:'mock sRGB',linearBlending:false,
 get bitsPerChannel(){return depth;},set bitsPerChannel(v){depth=v; changes++;},
 items:{addComp:function(){
  itemCount++; changes++;
  return {frameDuration:1/24,remove:function(){itemCount--;},layers:{addSolid:function(){
   itemCount+=2;
   var solidFolder={numItems:1,remove:function(){itemCount--;}};
   var src={parentFolder:solidFolder,remove:function(){solidFolder.numItems=0;itemCount--;}};
   return {source:src,property:function(){return {canAddProperty:function(){return !opt.cannotApply;},addProperty:newEffect};}};
  }}};
 }}
};
var app={
 version:'26.mock',buildNumber:0,project:project,
 effects:opt.notInstalled ? [] : [{displayName:'Stellar Gradient',matchName:'StellarLabs.StellarGradient',version:'0.9.5'}],
 beginUndoGroup:function(){begins++;},endUndoGroup:function(){ends++;},
 save:function(){throw new Error('FORBIDDEN');},newProject:function(){throw new Error('FORBIDDEN');},purge:function(){throw new Error('FORBIDDEN');}
};
function alert(text){alerts.push(text);}
`;
function run(opt={}) {
 const state={opt};
 vm.runInNewContext(mock+'\n'+source,state,{timeout:4000});
 const filename=Object.keys(state.files).find(x=>x.endsWith('/report.json'));
 return {...state,report: filename?JSON.parse(state.files[filename]):null};
}
let tests=0;
function test(name,fn){fn(); tests++; console.log('PASS '+name);}
function restored(s){assert.equal(s.itemCount,0);assert.equal(s.depth,16);assert.equal(s.qitems.length,0);assert.equal(s.begins,s.ends);}
test('ES-compatible syntax and four scoped captures',()=>{const s=run();assert.equal(s.report.summary,'SMOKE_ONLY_PASS');assert.equal(s.renders,4);assert.equal(s.report.loaded_build_id,'NOT_VERIFIED');assert.equal(s.report.release_status,'NOT_APPROVED');restored(s);});
test('existing user project is untouched',()=>{const s=run({busy:true});assert.equal(s.report.summary,'BLOCKED');assert.equal(s.changes,0);assert.equal(s.begins,0);assert.equal(s.itemCount,3);});
test('saved empty project is untouched',()=>{const s=run({saved:true});assert.equal(s.report.summary,'BLOCKED');assert.equal(s.changes,0);});
test('missing effect is blocked without install',()=>{const s=run({notInstalled:true});assert.equal(s.report.summary,'BLOCKED');assert.equal(s.changes,0);});
test('file permission denial performs no host mutation',()=>{const s=run({writeDenied:true});assert.equal(s.report,null);assert.equal(s.changes,0);assert.ok(s.alerts[0].includes('Allow Scripts'));});
test('AE initialization 25::3 is recorded and cleaned',()=>{const s=run({initFails:true});assert.equal(s.report.summary,'FAIL');assert.ok(s.report.checks.some(x=>x.detail.includes('25::3')));restored(s);});
test('cannot-apply failure closes undo group',()=>{const s=run({cannotApply:true});assert.equal(s.report.summary,'FAIL');restored(s);});
test('repeat default drift is detected',()=>{const s=run({defaultDrift:true});assert.equal(s.report.summary,'FAIL');restored(s);});
test('missing PNG template is BLOCKED, never bypassed',()=>{const s=run({noPNG:true});assert.equal(s.report.summary,'BLOCKED');assert.equal(s.renders,0);restored(s);});
test('native render exception retains partial record',()=>{const s=run({renderFails:true});assert.equal(s.report.summary,'FAIL');assert.ok(Object.values(s.files).some(x=>x.includes('render_start')));restored(s);});
test('render cancellation is not PASS',()=>{const s=run({cancel:true});assert.equal(s.report.summary,'FAIL');restored(s);});
test('bad PNG header is not PASS',()=>{const s=run({badPNG:true});assert.equal(s.report.summary,'FAIL');restored(s);});
test('wrong frame dimensions are not PASS',()=>{const s=run({badDimensions:true});assert.equal(s.report.summary,'FAIL');restored(s);});
test('each run has its own ID and never certifies GPU/HDR',()=>{const a=run(),b=run();assert.notEqual(a.report.run_id,b.report.run_id);assert.ok(a.report.captures.every(x=>x.engine_actually_used==='NOT_VERIFIED'&&x.pixel_correctness==='NOT_RUN'));});
console.log(tests+'/14 harness checks PASS; real AE: NOT RUN');
